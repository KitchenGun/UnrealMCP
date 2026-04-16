"""UnrealMCP Adapter Layer

기존 GPT ↔ Hermes 응답 포맷을 유지하면서
내부적으로 MCP Tool 시스템을 경유하는 어댑터 레이어.

외부 인터페이스 변경 없음:
  - 입력:  GPT가 생성한 intent JSON 문자열
  - 출력:  {"ok": bool, "result_text": str, "mode": "unreal-mcp-adapter", ...}

사용법:
    from unreal_mcp.adapter import UnrealMCPAdapter
    adapter = UnrealMCPAdapter()
    result = await adapter.execute(intent_json_str)
"""

from .request_adapter import RequestAdapter, IntentParseError
from .response_adapter import ResponseAdapter
from .tool_registry import ToolRegistry
from .idempotency import IdempotencyGuard
from .mcp_adapter import UnrealMCPAdapter, get_adapter

__all__ = [
    "RequestAdapter",
    "ResponseAdapter",
    "ToolRegistry",
    "IdempotencyGuard",
    "IntentParseError",
    "UnrealMCPAdapter",
    "get_adapter",
]
