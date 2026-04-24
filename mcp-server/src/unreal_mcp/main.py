"""Unreal Engine MCP server entry point.

Claude Desktop --(stdio)--> This server --(TCP:13377)--> UE5 C++ plugin
"""

import asyncio
import logging
import os
import sys
import threading
import time
from mcp.server.fastmcp import FastMCP

from .tools.actor import register_actor_tools
from .tools.blueprint import register_blueprint_tools
from .tools.material import register_material_tools
from .tools.ai_system import register_ai_tools
from .tools.editor import register_editor_tools
from .tools.advanced import register_advanced_tools
from .tools.world_builder import register_world_builder_tools
from .tools.light import register_light_tools

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# MCP server instance creation
server = FastMCP("Unreal_Engine_MCP")

# --------------------------------------------------------------------------
# 모든 @server.tool() 호출에 structured_output=False 강제 주입.
# 이유: FastMCP 기본값은 함수 반환 타입 어노테이션에서 outputSchema를 자동
# 생성하는데, Cowork→Anthropic API 브리지가 outputSchema 포함된 도구 정의를
# 거부하여 tools/call 단계에서 400 반환. structured_output=False 로 끄면
# outputSchema 미생성 → 클라이언트 호환성 최대화.
# 부수 효과: tools/list 응답 크기 47KB → ~25KB 감소.
# --------------------------------------------------------------------------
_orig_tool_decorator = server.tool

def _tool_unstructured(*args, **kwargs):
    kwargs.setdefault("structured_output", False)
    return _orig_tool_decorator(*args, **kwargs)

server.tool = _tool_unstructured  # type: ignore[assignment]

# Phase 1: Register Actor & Scene tools
register_actor_tools(server)

# Phase 2: Register Blueprint editing tools
register_blueprint_tools(server)

# Phase 3: Register Material & Asset tools
register_material_tools(server)

# Phase 4: Register AI System tools
register_ai_tools(server)

# Phase 5: Register Editor Automation tools
register_editor_tools(server)

# Phase 6: Register Advanced Systems tools
register_advanced_tools(server)

# Phase 7: Register World Builder tools
register_world_builder_tools(server)

# Phase 8: Register Light component control tools
register_light_tools(server)


# --------------------------------------------------------------------------
# 등록된 모든 도구의 inputSchema 정규화.
# Pydantic이 tuple[float,float,float] 같은 fixed-length 튜플을 JSON Schema
# 2020-12의 `prefixItems` 키워드로 직렬화하는데, Anthropic Tool API는
# draft-07 서브셋만 받음 → tools 배열 전체가 400으로 거부.
# prefixItems → items + minItems/maxItems 로 변환해 호환성 확보.
# --------------------------------------------------------------------------
def _normalize_schema(node):
    if isinstance(node, dict):
        if "prefixItems" in node:
            items = node.pop("prefixItems") or []
            if items:
                first = items[0]
                # 모두 같은 타입이면 단일 items 스키마, 아니면 anyOf
                if all(i == first for i in items):
                    node.setdefault("items", first)
                else:
                    node.setdefault("items", {"anyOf": items})
                node.setdefault("minItems", len(items))
                node.setdefault("maxItems", len(items))
        for v in node.values():
            _normalize_schema(v)
    elif isinstance(node, list):
        for v in node:
            _normalize_schema(v)


for _tool in server._tool_manager._tools.values():
    if _tool.parameters:
        _normalize_schema(_tool.parameters)


def _start_parent_watchdog() -> None:
    """부모 프로세스(Claude Desktop / Claude Code)가 죽으면 자식도 종료.

    Windows에서 stdin EOF가 자식까지 전달 안 되어 좀비 누적되는 문제 방지.
    데몬 스레드로 2초마다 PPID 생존 확인 → 죽으면 즉시 os._exit.
    """
    try:
        ppid = os.getppid()
    except Exception:
        return
    if ppid <= 1:
        return  # 이미 init이 부모 → 워치독 의미 없음

    if sys.platform == "win32":
        import ctypes
        kernel32 = ctypes.windll.kernel32
        STILL_ACTIVE = 259
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000

        def parent_alive() -> bool:
            handle = kernel32.OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION, False, ppid
            )
            if not handle:
                return False
            exit_code = ctypes.c_ulong()
            ok = kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))
            kernel32.CloseHandle(handle)
            return bool(ok) and exit_code.value == STILL_ACTIVE
    else:
        def parent_alive() -> bool:
            try:
                os.kill(ppid, 0)
                return True
            except (ProcessLookupError, PermissionError):
                return False

    def loop() -> None:
        while True:
            time.sleep(2)
            if not parent_alive():
                logger.warning("부모 프로세스(PID=%d) 종료 감지 → MCP 서버 종료", ppid)
                os._exit(0)

    threading.Thread(target=loop, daemon=True, name="parent-watchdog").start()
    logger.info("Parent watchdog started (ppid=%d)", ppid)


def main() -> None:
    """CLI entry point."""
    _start_parent_watchdog()
    logger.info(
        "Unreal MCP server starting "
        "(Phase 1: Actor & Scene | Phase 2: Blueprint | "
        "Phase 3: Material & Asset | Phase 4: AI System | "
        "Phase 5: Editor Automation | Phase 6: Advanced Systems | Phase 7: World Builder)"
    )
    asyncio.run(server.run_stdio_async())


if __name__ == "__main__":
    main()
