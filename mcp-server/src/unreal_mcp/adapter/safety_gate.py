"""Phase C — Safe Action Gate for destructive UE5 operations.

Three layers of protection, all hooked into the FastMCP middleware wrapper
(middleware.py) so individual tool files stay untouched:

  1. Dry-run mode (UNREAL_MCP_DRY_RUN=true) — destructive tools short-circuit
     with a `_dry_run` report instead of executing.
  2. Console command whitelist — `run_console_command` validates against an
     allowlist of safe prefixes before reaching the C++ handler.
  3. Protected paths — `delete_asset` rejects paths under /Engine/, /Script/,
     etc. before reaching the C++ handler.

A fourth, lighter step annotates responses with `_reversible` so Claude can
factor "Can the user Ctrl+Z this?" into its planning.
"""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

ENV_DRY_RUN = "UNREAL_MCP_DRY_RUN"
ENV_WHITELIST = "UNREAL_MCP_CONSOLE_WHITELIST"
ENV_PROTECTED = "UNREAL_MCP_PROTECTED_PATHS"

DEFAULT_PROTECTED_PATHS = ("/Engine/", "/Script/")
DEFAULT_WHITELIST_PATH = "config/console_command_whitelist.json"

# Tools that mutate persistent / hard-to-reverse state.
# These are subject to dry-run short-circuit and (where applicable) param validation.
DESTRUCTIVE_TOOLS: frozenset[str] = frozenset({
    "delete_asset",
    "delete_actor",
    "run_console_command",
    "load_level",       # discards unsaved changes in the current level
    "play_in_editor",   # large side-effects (PIE start)
    "save_level",       # writes to disk, but generally desired — kept for audit
})

# Tools whose effect can be undone via the editor's Undo stack.
REVERSIBLE_TOOLS: frozenset[str] = frozenset({
    "create_actor",
    "delete_actor",
    "duplicate_actor",
    "set_actor_transform",
    "set_actor_property",
    "set_light_property",
    "spawn_blueprint_actor",
    "spawn_actor_batch",
    "spawn_actor_grid",
    "spawn_actor_circle",
    "spawn_actor_line",
    "spawn_actor_scatter",
    "mirror_actors",
    "select_actors",
    "set_viewport_camera",
})

# Tools that perform filesystem-level deletion (not Undo-able from the editor).
NON_REVERSIBLE_DESTRUCTIVE: frozenset[str] = frozenset({
    "delete_asset",
    "run_console_command",
})


# ---------------------------------------------------------------------------
# Dry-run gate
# ---------------------------------------------------------------------------

def is_dry_run() -> bool:
    """Resolve dry-run flag from env. Truthy values: 1/true/yes/on (case-insensitive)."""
    raw = os.environ.get(ENV_DRY_RUN, "").strip().lower()
    return raw in ("1", "true", "yes", "on")


def make_dry_run_response(tool_name: str, params: dict[str, Any]) -> dict[str, Any]:
    """Standard dry-run report — same envelope as a successful tool call.

    Clients that parse `success`/`result`/`error` keep working; the `_dry_run`
    flag and `would_execute` field signal that no side-effect happened.
    """
    return {
        "success": True,
        "result": {
            "_dry_run": True,
            "would_execute": tool_name,
            "params": params,
            "reversible": tool_name in REVERSIBLE_TOOLS,
            "non_reversible": tool_name in NON_REVERSIBLE_DESTRUCTIVE,
        },
        "error": None,
    }


def should_short_circuit_dry_run(tool_name: str) -> bool:
    return is_dry_run() and tool_name in DESTRUCTIVE_TOOLS


# ---------------------------------------------------------------------------
# Console command whitelist
# ---------------------------------------------------------------------------

_WHITELIST_CACHE: dict[str, Any] | None = None
_WHITELIST_PATH_CACHE: str | None = None


def _load_whitelist() -> dict[str, list[str]]:
    """Load and cache console whitelist JSON. Safe defaults if missing/invalid."""
    global _WHITELIST_CACHE, _WHITELIST_PATH_CACHE
    path_str = os.environ.get(ENV_WHITELIST, DEFAULT_WHITELIST_PATH)
    if _WHITELIST_CACHE is not None and _WHITELIST_PATH_CACHE == path_str:
        return _WHITELIST_CACHE

    path = Path(path_str)
    if not path.is_absolute():
        # resolve relative to mcp-server package root (parent of src/)
        pkg_root = Path(__file__).resolve().parents[3]
        path = pkg_root / path_str

    safe_default: dict[str, list[str]] = {
        "prefixes": ["r.", "stat ", "t.", "ShowFlag.", "DumpConsoleCommands"],
        "deny_prefixes": ["quit", "exit", "DestroyAll", "obj gc"],
    }

    if not path.exists():
        logger.info("Console whitelist not found at %s; using safe defaults", path)
        _WHITELIST_CACHE = safe_default
        _WHITELIST_PATH_CACHE = path_str
        return safe_default

    try:
        with path.open("r", encoding="utf-8") as f:
            loaded = json.load(f)
        if not isinstance(loaded, dict):
            raise ValueError("whitelist JSON must be an object")
        # normalize
        result: dict[str, list[str]] = {
            "prefixes": list(loaded.get("prefixes") or []),
            "deny_prefixes": list(loaded.get("deny_prefixes") or []),
        }
        _WHITELIST_CACHE = result
        _WHITELIST_PATH_CACHE = path_str
        return result
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        logger.warning("Console whitelist invalid at %s (%s); using safe defaults", path, exc)
        _WHITELIST_CACHE = safe_default
        _WHITELIST_PATH_CACHE = path_str
        return safe_default


def reset_whitelist_cache() -> None:
    """Test helper — force re-read on next call."""
    global _WHITELIST_CACHE, _WHITELIST_PATH_CACHE
    _WHITELIST_CACHE = None
    _WHITELIST_PATH_CACHE = None


def validate_console_command(cmd: str) -> tuple[bool, str]:
    """Returns (allowed, reason). reason is empty when allowed."""
    if not cmd or not cmd.strip():
        return False, "Empty console command"
    cmd = cmd.strip()
    rules = _load_whitelist()

    for deny in rules.get("deny_prefixes", []):
        if cmd.lower().startswith(deny.lower()):
            return False, f"Denied by whitelist (prefix={deny!r})"

    for allow in rules.get("prefixes", []):
        if cmd.lower().startswith(allow.lower()):
            return True, ""

    return False, "Not in console whitelist (add prefix to console_command_whitelist.json to allow)"


# ---------------------------------------------------------------------------
# Protected path gate
# ---------------------------------------------------------------------------

def _resolve_protected_paths() -> tuple[str, ...]:
    raw = os.environ.get(ENV_PROTECTED, "")
    if not raw:
        return DEFAULT_PROTECTED_PATHS
    return tuple(p.strip() for p in raw.split(",") if p.strip())


def validate_destructive_path(path: str) -> tuple[bool, str]:
    """Returns (allowed, reason). Empty reason when allowed."""
    if not path or not path.strip():
        return False, "Empty asset path"
    path = path.strip()
    for protected in _resolve_protected_paths():
        if path.startswith(protected):
            return False, f"Path is protected: {protected}"
    return True, ""


# ---------------------------------------------------------------------------
# Pre-call gate orchestration — returns an error response or None
# ---------------------------------------------------------------------------

def _make_error(code: str, message: str) -> dict[str, Any]:
    return {
        "success": False,
        "result": None,
        "error": {"code": code, "message": message},
    }


def precall_gate(tool_name: str, params: dict[str, Any]) -> dict[str, Any] | None:
    """Run all pre-call gates for a destructive tool.

    Returns:
        - dry-run response dict when dry-run is active (caller short-circuits)
        - error response dict when validation fails
        - None when the call should proceed normally
    """
    if should_short_circuit_dry_run(tool_name):
        return make_dry_run_response(tool_name, params)

    if tool_name == "run_console_command":
        cmd = str(params.get("command_string", "")).strip()
        ok, reason = validate_console_command(cmd)
        if not ok:
            return _make_error("CONSOLE_COMMAND_DENIED", reason)

    if tool_name == "delete_asset":
        path = str(params.get("asset_path", "")).strip()
        ok, reason = validate_destructive_path(path)
        if not ok:
            return _make_error("PROTECTED_PATH", reason)

    return None


# ---------------------------------------------------------------------------
# Post-call annotation — reversibility metadata
# ---------------------------------------------------------------------------

def annotate_reversibility(tool_name: str, response: dict[str, Any]) -> dict[str, Any]:
    """Inject `_reversible` and `_undo_hint` into a successful response.

    Mutates and returns `response`. No-op if the response is not a successful
    standard envelope or if the tool is not in any tracked set.
    """
    if not isinstance(response, dict) or not response.get("success"):
        return response

    inner = response.get("result")
    if not isinstance(inner, dict):
        return response

    if tool_name in REVERSIBLE_TOOLS:
        inner.setdefault("_reversible", True)
        inner.setdefault("_undo_hint", "Editor: Ctrl+Z")
    elif tool_name in NON_REVERSIBLE_DESTRUCTIVE:
        inner.setdefault("_reversible", False)
        inner.setdefault("_undo_hint", None)

    return response
