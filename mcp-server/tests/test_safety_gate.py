"""Phase C regression tests — Safe Action Gate.

Validates:
  - Dry-run short-circuits destructive tools and returns standard envelope
  - Console command whitelist allows safe prefixes, rejects denied prefixes
  - Protected path gate refuses /Engine/, /Script/, custom prefixes
  - Reversibility annotation tags successful responses correctly
  - Middleware integration: pre-call gate blocks execution end-to-end
"""

from __future__ import annotations

import asyncio
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1] / "src"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from unreal_mcp.adapter import safety_gate as sg
from unreal_mcp.adapter.middleware import _wrap_tool_fn, install_response_middleware
from unreal_mcp.adapter.response_adapter import ResponseAdapter


# ---------------------------------------------------------------------------
# Fakes
# ---------------------------------------------------------------------------

class _FakeTool:
    def __init__(self, name, fn):
        self.name = name
        self.fn = fn
        self.parameters = {}


class _FakeServer:
    def __init__(self, tools):
        class _M:
            pass
        self._tool_manager = _M()
        self._tool_manager._tools = {t.name: t for t in tools}


def _fn_returning(payload):
    """Build an async fn that returns a JSON string of `payload`."""
    async def fn(**kwargs):
        return json.dumps(payload, ensure_ascii=False)
    return fn


# ---------------------------------------------------------------------------
# is_dry_run
# ---------------------------------------------------------------------------

class TestIsDryRun:
    def test_unset_is_false(self, monkeypatch):
        monkeypatch.delenv(sg.ENV_DRY_RUN, raising=False)
        assert sg.is_dry_run() is False

    @pytest.mark.parametrize("val", ["1", "true", "True", "TRUE", "yes", "on"])
    def test_truthy(self, monkeypatch, val):
        monkeypatch.setenv(sg.ENV_DRY_RUN, val)
        assert sg.is_dry_run() is True

    @pytest.mark.parametrize("val", ["0", "false", "no", "off", "", "garbage"])
    def test_falsy(self, monkeypatch, val):
        monkeypatch.setenv(sg.ENV_DRY_RUN, val)
        assert sg.is_dry_run() is False


# ---------------------------------------------------------------------------
# Console whitelist
# ---------------------------------------------------------------------------

class TestValidateConsoleCommand:
    def setup_method(self):
        sg.reset_whitelist_cache()

    def test_empty_rejected(self):
        ok, reason = sg.validate_console_command("")
        assert not ok
        assert "Empty" in reason

    def test_allowed_prefix(self):
        ok, reason = sg.validate_console_command("r.ScreenPercentage 50")
        assert ok
        assert reason == ""

    def test_allowed_stat(self):
        ok, _ = sg.validate_console_command("stat fps")
        assert ok

    def test_denied_quit(self):
        ok, reason = sg.validate_console_command("quit")
        assert not ok
        assert "Denied" in reason

    def test_denied_exit(self):
        ok, reason = sg.validate_console_command("exit")
        assert not ok

    def test_unknown_command_rejected(self):
        ok, reason = sg.validate_console_command("MyArbitraryCommand foo")
        assert not ok
        assert "whitelist" in reason

    def test_case_insensitive_prefix(self):
        ok, _ = sg.validate_console_command("R.ScreenPercentage 50")
        assert ok

    def test_missing_whitelist_uses_safe_defaults(self, monkeypatch, tmp_path):
        sg.reset_whitelist_cache()
        monkeypatch.setenv(sg.ENV_WHITELIST, str(tmp_path / "nonexistent.json"))
        ok, _ = sg.validate_console_command("r.SomeFlag 1")
        assert ok  # safe default still allows r.

    def test_custom_whitelist(self, monkeypatch, tmp_path):
        sg.reset_whitelist_cache()
        custom = tmp_path / "wl.json"
        custom.write_text(json.dumps({
            "prefixes": ["MyOnly"],
            "deny_prefixes": [],
        }), encoding="utf-8")
        monkeypatch.setenv(sg.ENV_WHITELIST, str(custom))
        ok, _ = sg.validate_console_command("MyOnly something")
        assert ok
        ok, _ = sg.validate_console_command("r.ScreenPercentage 50")
        assert not ok  # r. not in custom whitelist


# ---------------------------------------------------------------------------
# Protected paths
# ---------------------------------------------------------------------------

class TestValidateDestructivePath:
    def test_empty_rejected(self):
        ok, reason = sg.validate_destructive_path("")
        assert not ok

    def test_engine_protected(self, monkeypatch):
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)
        ok, reason = sg.validate_destructive_path("/Engine/Foo")
        assert not ok
        assert "/Engine/" in reason

    def test_script_protected(self, monkeypatch):
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)
        ok, _ = sg.validate_destructive_path("/Script/Bar")
        assert not ok

    def test_game_allowed(self, monkeypatch):
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)
        ok, reason = sg.validate_destructive_path("/Game/MyAsset")
        assert ok
        assert reason == ""

    def test_custom_protected(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_PROTECTED, "/Game/_Locked/,/Engine/")
        ok, reason = sg.validate_destructive_path("/Game/_Locked/Important")
        assert not ok
        ok, _ = sg.validate_destructive_path("/Game/MyAsset")
        assert ok


# ---------------------------------------------------------------------------
# precall_gate orchestration
# ---------------------------------------------------------------------------

class TestPrecallGate:
    def test_non_destructive_returns_none(self):
        assert sg.precall_gate("get_actor_properties", {"name": "X"}) is None

    def test_destructive_with_dry_run_returns_report(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "true")
        out = sg.precall_gate("delete_actor", {"name": "X"})
        assert out is not None
        assert out["success"] is True
        assert out["result"]["_dry_run"] is True
        assert out["result"]["would_execute"] == "delete_actor"
        assert out["result"]["reversible"] is True

    def test_dry_run_marks_non_reversible(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "true")
        out = sg.precall_gate("delete_asset", {"asset_path": "/Game/X"})
        assert out["result"]["non_reversible"] is True

    def test_destructive_without_dry_run_validates(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        sg.reset_whitelist_cache()
        out = sg.precall_gate("run_console_command", {"command_string": "quit"})
        assert out is not None
        assert out["success"] is False
        assert out["error"]["code"] == "CONSOLE_COMMAND_DENIED"

    def test_console_allowed_passes(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        sg.reset_whitelist_cache()
        assert sg.precall_gate(
            "run_console_command", {"command_string": "stat fps"}
        ) is None

    def test_protected_path_rejected(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)
        out = sg.precall_gate("delete_asset", {"asset_path": "/Engine/Foo"})
        assert out is not None
        assert out["error"]["code"] == "PROTECTED_PATH"

    def test_unprotected_path_passes(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)
        assert sg.precall_gate(
            "delete_asset", {"asset_path": "/Game/MyAsset"}
        ) is None


# ---------------------------------------------------------------------------
# annotate_reversibility
# ---------------------------------------------------------------------------

class TestAnnotateReversibility:
    def test_reversible_tool(self):
        resp = {"success": True, "result": {"name": "X"}, "error": None}
        sg.annotate_reversibility("create_actor", resp)
        assert resp["result"]["_reversible"] is True
        assert resp["result"]["_undo_hint"] == "Editor: Ctrl+Z"

    def test_non_reversible_destructive(self):
        resp = {"success": True, "result": {"deleted": "X"}, "error": None}
        sg.annotate_reversibility("delete_asset", resp)
        assert resp["result"]["_reversible"] is False
        assert resp["result"]["_undo_hint"] is None

    def test_unknown_tool_no_annotation(self):
        resp = {"success": True, "result": {"k": "v"}, "error": None}
        sg.annotate_reversibility("get_actor_properties", resp)
        assert "_reversible" not in resp["result"]

    def test_failure_response_skipped(self):
        resp = {"success": False, "result": None,
                "error": {"code": "X", "message": "y"}}
        sg.annotate_reversibility("create_actor", resp)
        assert resp["result"] is None  # untouched

    def test_does_not_overwrite_existing(self):
        resp = {"success": True, "result": {"_reversible": "preset"}, "error": None}
        sg.annotate_reversibility("create_actor", resp)
        assert resp["result"]["_reversible"] == "preset"


# ---------------------------------------------------------------------------
# Middleware integration — end-to-end
# ---------------------------------------------------------------------------

class TestMiddlewareIntegration:
    def test_dry_run_short_circuits_through_wrapper(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "true")
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")
        sg.reset_whitelist_cache()

        executed = {"called": False}

        async def real_delete(name: str = "X") -> str:
            executed["called"] = True
            return json.dumps({"success": True, "result": {"deleted": name}})

        wrapped = _wrap_tool_fn(
            "delete_actor", real_delete, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(name="DirectionalLight"))
        parsed = json.loads(result)

        assert executed["called"] is False
        assert parsed["result"]["_dry_run"] is True
        assert parsed["result"]["would_execute"] == "delete_actor"

    def test_console_blocked_through_wrapper(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")
        sg.reset_whitelist_cache()

        executed = {"called": False}

        async def real_console(command_string: str = "") -> str:
            executed["called"] = True
            return json.dumps({"success": True, "result": {}})

        wrapped = _wrap_tool_fn(
            "run_console_command", real_console, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(command_string="quit"))
        parsed = json.loads(result)

        assert executed["called"] is False
        assert parsed["success"] is False
        assert parsed["error"]["code"] == "CONSOLE_COMMAND_DENIED"

    def test_console_allowed_passes_through(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")
        sg.reset_whitelist_cache()

        async def real_console(command_string: str = "") -> str:
            return json.dumps({"success": True, "result": {"command": command_string}})

        wrapped = _wrap_tool_fn(
            "run_console_command", real_console, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(command_string="stat fps"))
        parsed = json.loads(result)
        assert parsed["success"] is True

    def test_protected_path_blocks_delete_asset(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")
        monkeypatch.delenv(sg.ENV_PROTECTED, raising=False)

        executed = {"called": False}

        async def real_delete(asset_path: str = "") -> str:
            executed["called"] = True
            return json.dumps({"success": True, "result": {"deleted": asset_path}})

        wrapped = _wrap_tool_fn(
            "delete_asset", real_delete, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(asset_path="/Engine/EngineMaterials/Foo"))
        parsed = json.loads(result)

        assert executed["called"] is False
        assert parsed["error"]["code"] == "PROTECTED_PATH"

    def test_reversibility_annotated_on_success(self, monkeypatch):
        monkeypatch.setenv(sg.ENV_DRY_RUN, "false")
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")

        async def real_create(actor_class: str = "") -> str:
            return json.dumps({
                "success": True,
                "result": {"name": "NewActor", "actor_class": actor_class},
                "error": None,
            })

        wrapped = _wrap_tool_fn(
            "create_actor", real_create, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(actor_class="StaticMeshActor"))
        parsed = json.loads(result)
        assert parsed["result"]["_reversible"] is True
        assert parsed["result"]["_undo_hint"] == "Editor: Ctrl+Z"

    def test_safe_tool_not_annotated(self, monkeypatch):
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")

        async def real_get(name: str = "") -> str:
            return json.dumps({
                "success": True,
                "result": {"name": name, "actor_class": "Light"},
                "error": None,
            })

        wrapped = _wrap_tool_fn(
            "get_actor_properties", real_get, "raw", ResponseAdapter(),
        )
        result = asyncio.run(wrapped(name="X"))
        parsed = json.loads(result)
        assert "_reversible" not in parsed["result"]


# ---------------------------------------------------------------------------
# install_response_middleware in raw mode still wraps safety-relevant tools
# ---------------------------------------------------------------------------

class TestInstallRawModeSafety:
    def test_raw_mode_still_wraps_destructive(self, monkeypatch):
        monkeypatch.setenv("UNREAL_MCP_RESPONSE_MODE", "raw")
        async def safe_fn(**kw):
            return "{}"
        async def destructive_fn(**kw):
            return "{}"
        tools = [
            _FakeTool("get_actor_properties", safe_fn),
            _FakeTool("delete_actor", destructive_fn),
        ]
        server = _FakeServer(tools)
        install_response_middleware(server)
        assert tools[0].fn is safe_fn  # safe tool not wrapped in raw mode
        assert tools[1].fn is not destructive_fn  # destructive wrapped
        assert getattr(tools[1].fn, "__unreal_mcp_wrapped__", False) is True
