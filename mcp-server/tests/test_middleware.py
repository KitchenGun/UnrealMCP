"""Phase B regression tests — FastMCP response middleware.

Validates:
  - Each mode (raw/compact/summary/hermes) transforms output as expected
  - Wrapper preserves the original signature for FastMCP's arg validation
  - Idempotent install (re-applying does not double-wrap)
  - Bypass list excludes named tools
  - Defensive: summarizer exception does not crash the tool
"""

from __future__ import annotations

import asyncio
import inspect
import json
import os
import sys
from pathlib import Path
from unittest.mock import patch

import pytest

ROOT = Path(__file__).resolve().parents[1] / "src"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from unreal_mcp.adapter import middleware as mw
from unreal_mcp.adapter.middleware import (
    DEFAULT_MODE,
    ENV_BYPASS,
    ENV_MODE,
    VALID_MODES,
    _post_process,
    _resolve_mode,
    _wrap_tool_fn,
    install_response_middleware,
)
from unreal_mcp.adapter.response_adapter import ResponseAdapter


# ---------------------------------------------------------------------------
# Helpers — fake FastMCP server / Tool
# ---------------------------------------------------------------------------

class _FakeTool:
    def __init__(self, name, fn, parameters=None):
        self.name = name
        self.fn = fn
        self.parameters = parameters or {}


class _FakeToolManager:
    def __init__(self, tools):
        self._tools = {t.name: t for t in tools}


class _FakeServer:
    def __init__(self, tools):
        self._tool_manager = _FakeToolManager(tools)


SAMPLE_RESULT = {
    "id": "abc",
    "success": True,
    "result": {
        "name": "DirectionalLight",
        "actor_class": "DirectionalLight",
        "intensity": 6.0,
    },
    "error": None,
}


def _make_async_tool_fn(name: str, payload: dict):
    """Build an async fn whose signature matches a real tool (typed kwargs)."""
    async def get_actor_properties(name: str = "DirectionalLight") -> str:
        return json.dumps(payload, indent=2, ensure_ascii=False)
    get_actor_properties.__name__ = name
    return get_actor_properties


# ---------------------------------------------------------------------------
# _resolve_mode
# ---------------------------------------------------------------------------

class TestResolveMode:
    def test_default_when_unset(self, monkeypatch):
        monkeypatch.delenv(ENV_MODE, raising=False)
        assert _resolve_mode() == DEFAULT_MODE

    def test_valid_modes(self, monkeypatch):
        for m in VALID_MODES:
            monkeypatch.setenv(ENV_MODE, m)
            assert _resolve_mode() == m

    def test_invalid_falls_back(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "garbage")
        assert _resolve_mode() == DEFAULT_MODE

    def test_case_insensitive(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "SUMMARY")
        assert _resolve_mode() == "summary"


# ---------------------------------------------------------------------------
# _post_process — each mode
# ---------------------------------------------------------------------------

class TestPostProcessRaw:
    def test_returns_input_verbatim(self):
        raw = json.dumps(SAMPLE_RESULT, indent=2)
        assert _post_process("get_actor_properties", raw, "raw", ResponseAdapter()) is raw


class TestPostProcessCompact:
    def test_strips_indentation(self):
        raw = json.dumps(SAMPLE_RESULT, indent=2)
        out = _post_process("get_actor_properties", raw, "compact", ResponseAdapter())
        assert "\n" not in out
        assert json.loads(out) == SAMPLE_RESULT
        assert len(out) < len(raw)

    def test_invalid_json_unchanged(self):
        raw = "not json"
        out = _post_process("x", raw, "compact", ResponseAdapter())
        assert out == raw


class TestPostProcessSummary:
    def test_prepends_summary_field(self):
        raw = json.dumps(SAMPLE_RESULT)
        out = _post_process("get_actor_properties", raw, "summary", ResponseAdapter())
        parsed = json.loads(out)
        assert "_summary" in parsed
        # original payload preserved
        assert parsed["success"] is True
        assert parsed["result"]["intensity"] == 6.0

    def test_summary_appears_first(self):
        raw = json.dumps(SAMPLE_RESULT)
        out = _post_process("get_actor_properties", raw, "summary", ResponseAdapter())
        # _summary should be at the very start of the JSON object
        assert out.startswith('{"_summary"')

    def test_failure_response_passes_through(self):
        failure = {"success": False, "result": None,
                   "error": {"code": "ACTOR_NOT_FOUND", "message": "x"}}
        raw = json.dumps(failure)
        out = _post_process("get_actor_properties", raw, "summary", ResponseAdapter())
        parsed = json.loads(out)
        assert "_summary" not in parsed
        assert parsed["success"] is False

    def test_unknown_tool_uses_generic(self):
        raw = json.dumps(SAMPLE_RESULT)
        out = _post_process("totally_unknown_tool", raw, "summary", ResponseAdapter())
        parsed = json.loads(out)
        # generic summarizer dumps the result as text — no exception
        assert "_summary" in parsed


class TestPostProcessHermes:
    def test_emits_hermes_envelope(self):
        raw = json.dumps(SAMPLE_RESULT)
        out = _post_process("get_actor_properties", raw, "hermes", ResponseAdapter())
        parsed = json.loads(out)
        assert parsed["mode"] == "unreal-mcp-adapter"
        assert parsed["ok"] is True
        assert "result_text" in parsed

    def test_compresses_large_payloads(self):
        """Hermes envelope adds ~70 bytes of overhead but strips heavy result.

        For large payloads (the common case for token-heavy tools), Hermes
        produces a much smaller output than raw. Verify with a 100-actor list.
        """
        bulky = {
            "id": "x",
            "success": True,
            "result": {
                "actors": [
                    {"name": f"Actor_{i}", "actor_class": "StaticMeshActor",
                     "location": {"x": i * 10, "y": 0, "z": 0}}
                    for i in range(100)
                ],
                "count": 100,
            },
            "error": None,
        }
        raw = json.dumps(bulky, indent=2)
        out = _post_process("get_actors_in_level", raw, "hermes", ResponseAdapter())
        # large payload → hermes output is much smaller
        assert len(out) < len(raw) * 0.2


# ---------------------------------------------------------------------------
# _wrap_tool_fn — signature preservation + invocation
# ---------------------------------------------------------------------------

class TestWrapToolFn:
    def test_signature_preserved(self):
        async def original(name: str, count: int = 5) -> str:
            return json.dumps({"success": True, "result": {"name": name, "count": count}})

        wrapped = _wrap_tool_fn("x", original, "raw", ResponseAdapter())
        sig = inspect.signature(wrapped)
        params = list(sig.parameters.keys())
        assert params == ["name", "count"]
        assert sig.parameters["count"].default == 5

    def test_invocation_passes_kwargs(self):
        captured = {}

        async def original(name: str = "default") -> str:
            captured["name"] = name
            return json.dumps({"success": True, "result": {"name": name}})

        wrapped = _wrap_tool_fn("x", original, "raw", ResponseAdapter())
        result = asyncio.run(wrapped(name="custom"))
        assert captured["name"] == "custom"
        assert json.loads(result)["result"]["name"] == "custom"

    def test_non_string_return_passes_through(self):
        async def original() -> dict:
            return {"foo": "bar"}

        wrapped = _wrap_tool_fn("x", original, "summary", ResponseAdapter())
        result = asyncio.run(wrapped())
        assert result == {"foo": "bar"}


# ---------------------------------------------------------------------------
# install_response_middleware — server-level integration
# ---------------------------------------------------------------------------

class TestInstall:
    def test_skips_when_raw(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "raw")
        fn = _make_async_tool_fn("get_actor_properties", SAMPLE_RESULT)
        tool = _FakeTool("get_actor_properties", fn)
        server = _FakeServer([tool])
        install_response_middleware(server)
        # fn unchanged
        assert tool.fn is fn

    def test_wraps_all_tools_summary(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "summary")
        tools = [
            _FakeTool(n, _make_async_tool_fn(n, SAMPLE_RESULT))
            for n in ("get_actor_properties", "set_light_property")
        ]
        server = _FakeServer(tools)
        install_response_middleware(server)
        for t in tools:
            assert getattr(t.fn, "__unreal_mcp_wrapped__", False) is True

    def test_idempotent(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "summary")
        fn = _make_async_tool_fn("get_actor_properties", SAMPLE_RESULT)
        tool = _FakeTool("get_actor_properties", fn)
        server = _FakeServer([tool])
        install_response_middleware(server)
        first = tool.fn
        install_response_middleware(server)
        # second install should detect marker and not re-wrap
        assert tool.fn is first

    def test_bypass_list(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "summary")
        monkeypatch.setenv(ENV_BYPASS, "set_light_property")
        kept = _make_async_tool_fn("set_light_property", SAMPLE_RESULT)
        wrapped_fn = _make_async_tool_fn("get_actor_properties", SAMPLE_RESULT)
        tools = [
            _FakeTool("set_light_property", kept),
            _FakeTool("get_actor_properties", wrapped_fn),
        ]
        server = _FakeServer(tools)
        install_response_middleware(server)
        assert tools[0].fn is kept  # bypassed
        assert tools[1].fn is not wrapped_fn  # wrapped

    def test_invocation_after_install_summary(self, monkeypatch):
        monkeypatch.setenv(ENV_MODE, "summary")
        fn = _make_async_tool_fn("get_actor_properties", SAMPLE_RESULT)
        tool = _FakeTool("get_actor_properties", fn)
        server = _FakeServer([tool])
        install_response_middleware(server)
        result = asyncio.run(tool.fn(name="DirectionalLight"))
        parsed = json.loads(result)
        assert "_summary" in parsed
        assert parsed["result"]["intensity"] == 6.0


# ---------------------------------------------------------------------------
# Defensive: summarizer raising should not crash the tool
# ---------------------------------------------------------------------------

class TestSummarizerSafety:
    def test_broken_summarizer_does_not_crash(self, monkeypatch):
        from unreal_mcp.adapter import response_adapter as ra

        def bad(_):
            raise RuntimeError("intentional")

        monkeypatch.setitem(ra._SUMMARIZERS, "broken_tool", bad)
        raw = json.dumps(SAMPLE_RESULT)
        out = _post_process("broken_tool", raw, "summary", ResponseAdapter())
        # falls back to raw payload (sans summary), still valid JSON
        parsed = json.loads(out)
        assert "_summary" not in parsed
        assert parsed["result"]["intensity"] == 6.0


# ---------------------------------------------------------------------------
# New summarizers (Phase B-3 extension)
# ---------------------------------------------------------------------------

class TestNewSummarizers:
    def test_blueprint_graph(self):
        from unreal_mcp.adapter.response_adapter import _summarize_blueprint_graph
        s = _summarize_blueprint_graph({
            "blueprint_name": "BP_Player",
            "graph_name": "EventGraph",
            "nodes": [
                {"node_class": "K2Node_Event"},
                {"node_class": "K2Node_Event"},
                {"node_class": "K2Node_CallFunction"},
            ],
            "total_count": 3,
            "returned": 3,
        })
        assert "BP_Player/EventGraph" in s
        assert "3 nodes" in s
        assert "K2Node_Event" in s

    def test_inspect_uobject(self):
        from unreal_mcp.adapter.response_adapter import _summarize_inspect_uobject
        s = _summarize_inspect_uobject({
            "class_name": "Character",
            "package": "/Script/Engine",
            "class_hierarchy": ["Character", "Pawn", "Actor"],
            "properties": [{"name": "X"}, {"name": "Y"}],
            "functions": [{"name": "F"}],
            "property_count": 2,
            "function_count": 1,
        })
        assert "Character" in s
        assert "props=2" in s

    def test_search_assets(self):
        from unreal_mcp.adapter.response_adapter import _summarize_search_assets
        s = _summarize_search_assets({
            "assets": [{"name": "M_Rock"}, {"name": "M_Wood"}],
            "total_count": 2,
        })
        assert "M_Rock" in s
        assert "M_Wood" in s

    def test_search_assets_empty(self):
        from unreal_mcp.adapter.response_adapter import _summarize_search_assets
        s = _summarize_search_assets({"assets": [], "total_count": 0})
        assert "매칭 없음" in s
