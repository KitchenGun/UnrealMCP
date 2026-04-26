"""FastMCP response post-processing middleware.

The FastMCP `@server.tool` path bypasses `mcp_adapter.py`, so summarizers in
`response_adapter.py` were never reaching Claude Desktop. This middleware
hooks into FastMCP after registration by replacing each Tool's `fn` with a
wrapper that post-processes the JSON response based on `UNREAL_MCP_RESPONSE_MODE`.

Modes:
    raw     — original JSON (escape hatch)
    compact — `indent=None` only (~30% smaller, no semantic loss)
    summary — original JSON + leading `_summary` field (default)
    hermes  — full ResponseAdapter.to_hermes() (~70-90% smaller, for orchestrators)

Signature preservation:
    `Tool.fn_metadata` is built from the original function signature at
    registration time and validates kwargs before invoking `tool.fn`.
    The wrapper accepts **kwargs and forwards to the original, so validation
    keeps working unchanged. `functools.wraps` + explicit `__signature__`
    copy keep the wrapper introspectable.
"""

from __future__ import annotations

import functools
import inspect
import json
import logging
import os
from collections.abc import Awaitable, Callable
from typing import Any

from .response_adapter import ResponseAdapter, _SUMMARIZERS, _summarize_generic
from .safety_gate import (
    DESTRUCTIVE_TOOLS,
    NON_REVERSIBLE_DESTRUCTIVE,
    REVERSIBLE_TOOLS,
    annotate_reversibility,
    precall_gate,
)

logger = logging.getLogger(__name__)

ENV_MODE = "UNREAL_MCP_RESPONSE_MODE"
ENV_BYPASS = "UNREAL_MCP_MIDDLEWARE_BYPASS"  # comma-separated tool names

VALID_MODES: frozenset[str] = frozenset({"raw", "compact", "summary", "hermes"})
DEFAULT_MODE = "summary"


def _resolve_mode() -> str:
    raw = os.environ.get(ENV_MODE, DEFAULT_MODE).strip().lower()
    if raw not in VALID_MODES:
        logger.warning(
            "Invalid %s=%r; falling back to %r. Valid: %s",
            ENV_MODE, raw, DEFAULT_MODE, sorted(VALID_MODES),
        )
        return DEFAULT_MODE
    return raw


def _resolve_bypass_set() -> frozenset[str]:
    raw = os.environ.get(ENV_BYPASS, "")
    return frozenset(t.strip() for t in raw.split(",") if t.strip())


def _post_process(tool_name: str, raw_str: str, mode: str, adapter: ResponseAdapter) -> str:
    """Transform tool's JSON-string output according to mode.

    Returns a string (possibly JSON, possibly Hermes dict serialized as JSON)
    matching the original tool's return type contract (`-> str`).
    """
    if not raw_str or not isinstance(raw_str, str):
        return raw_str

    if mode == "raw":
        return raw_str

    if mode == "compact":
        try:
            parsed = json.loads(raw_str)
        except (json.JSONDecodeError, ValueError):
            return raw_str
        return json.dumps(parsed, ensure_ascii=False, indent=None)

    if mode == "summary":
        try:
            parsed = json.loads(raw_str)
        except (json.JSONDecodeError, ValueError):
            return raw_str
        if not isinstance(parsed, dict):
            return raw_str
        if parsed.get("success") and isinstance(parsed.get("result"), dict):
            summarizer = _SUMMARIZERS.get(tool_name, _summarize_generic)
            try:
                summary_text = summarizer(parsed["result"])
            except Exception as exc:  # defensive: never fail the tool because of summary
                logger.debug("Summarizer for %r failed: %s", tool_name, exc)
                summary_text = ""
            if summary_text:
                # prepend _summary while preserving raw payload
                rebuilt = {"_summary": summary_text, **parsed}
                return json.dumps(rebuilt, ensure_ascii=False, indent=None)
        return json.dumps(parsed, ensure_ascii=False, indent=None)

    if mode == "hermes":
        hermes = adapter.to_hermes(tool_name, raw_str)
        return json.dumps(hermes, ensure_ascii=False, indent=None)

    return raw_str


def _needs_safety_gate(tool_name: str) -> bool:
    """Tools that require pre-call validation or post-call reversibility tagging."""
    return (
        tool_name in DESTRUCTIVE_TOOLS
        or tool_name in REVERSIBLE_TOOLS
        or tool_name in NON_REVERSIBLE_DESTRUCTIVE
    )


def _apply_post_call(
    tool_name: str,
    raw: Any,
    mode: str,
    adapter: ResponseAdapter,
    annotate: bool,
) -> Any:
    """Combine reversibility annotation with mode-specific post-processing."""
    if not isinstance(raw, str):
        return raw

    if annotate:
        try:
            parsed = json.loads(raw)
        except (json.JSONDecodeError, ValueError):
            parsed = None
        if isinstance(parsed, dict):
            annotate_reversibility(tool_name, parsed)
            raw = json.dumps(parsed, ensure_ascii=False, indent=None)

    return _post_process(tool_name, raw, mode, adapter)


def _wrap_tool_fn(
    tool_name: str,
    original_fn: Callable[..., Awaitable[Any]],
    mode: str,
    adapter: ResponseAdapter,
) -> Callable[..., Awaitable[Any]]:
    """Build a wrapper preserving signature, applying safety gates + post-processing."""
    sig = inspect.signature(original_fn)
    apply_gate = _needs_safety_gate(tool_name)
    annotate = (
        tool_name in REVERSIBLE_TOOLS or tool_name in NON_REVERSIBLE_DESTRUCTIVE
    )

    @functools.wraps(original_fn)
    async def wrapped(**kwargs: Any) -> Any:
        # Pre-call safety gate (dry-run, console whitelist, protected paths)
        if apply_gate and tool_name in DESTRUCTIVE_TOOLS:
            short_circuit = precall_gate(tool_name, dict(kwargs))
            if short_circuit is not None:
                # Short-circuit: serialize as if it were a real tool response.
                raw = json.dumps(short_circuit, ensure_ascii=False, indent=2)
                return _post_process(tool_name, raw, mode, adapter)

        raw = await original_fn(**kwargs)
        return _apply_post_call(tool_name, raw, mode, adapter, annotate)

    # Explicit signature copy — fn_metadata is built from original_fn at
    # registration time, but other introspection (debugging, logging) benefits.
    wrapped.__signature__ = sig  # type: ignore[attr-defined]
    return wrapped


def install_response_middleware(server: Any) -> str:
    """Wrap every registered tool's `fn` with mode-aware post-processing.

    In raw mode, only safety-relevant tools (destructive/reversible) are wrapped
    so dry-run, console whitelist, protected paths, and reversibility tagging
    keep working regardless of UNREAL_MCP_RESPONSE_MODE.

    Returns the resolved mode string for logging/observability.
    Idempotent: detects already-wrapped tools via a marker attribute.
    """
    mode = _resolve_mode()
    bypass = _resolve_bypass_set()
    adapter = ResponseAdapter()

    tool_manager = getattr(server, "_tool_manager", None)
    tools_dict = getattr(tool_manager, "_tools", None)
    if not isinstance(tools_dict, dict):
        logger.warning("Cannot install middleware: server._tool_manager._tools missing")
        return mode

    wrapped_count = 0
    safety_only = 0
    for tool_name, tool in tools_dict.items():
        if tool_name in bypass:
            continue
        if getattr(tool.fn, "__unreal_mcp_wrapped__", False):
            continue
        # In raw mode, only wrap tools that need safety gating.
        if mode == "raw" and not _needs_safety_gate(tool_name):
            continue
        original_fn = tool.fn
        new_fn = _wrap_tool_fn(tool_name, original_fn, mode, adapter)
        new_fn.__unreal_mcp_wrapped__ = True  # type: ignore[attr-defined]
        tool.fn = new_fn
        wrapped_count += 1
        if mode == "raw":
            safety_only += 1

    if mode == "raw":
        logger.info(
            "Response middleware: mode=raw, safety-wrapped only (count=%d, bypassed=%d)",
            safety_only, len(bypass),
        )
    else:
        logger.info(
            "Response middleware installed: mode=%s, wrapped=%d, bypassed=%d",
            mode, wrapped_count, len(bypass),
        )
    return mode
