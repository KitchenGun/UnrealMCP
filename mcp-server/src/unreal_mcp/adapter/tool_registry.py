"""ToolRegistry — MCP Tool 중앙 등록 및 조회 시스템

chongdashu 레퍼런스의 Commands 클래스 분리 구조를 참고하여
기존 @server.tool() 데코레이터 등록 방식과 호환되도록 설계.

기존 main.py / tools/*.py 코드는 변경하지 않는다.
이 레지스트리는 Adapter Layer가 직접 tool 함수를 호출할 때 사용한다.
"""

from __future__ import annotations

import asyncio
from typing import Any, Callable, Awaitable


class ToolRegistry:
    """Adapter Layer 전용 Tool 레지스트리.

    기존 mcp.Server에 등록된 tool과 별개로,
    Adapter가 직접 호출할 수 있도록 tool 함수를 관리한다.

    등록 방식:
        registry = ToolRegistry()
        registry.register("create_actor", _create_actor_fn)

    호출 방식:
        result_str = await registry.call("create_actor", actor_class="PointLight", ...)
    """

    def __init__(self) -> None:
        self._tools: dict[str, Callable[..., Awaitable[str]]] = {}

    def register(self, name: str, fn: Callable[..., Awaitable[str]]) -> None:
        """tool 함수 등록.

        Args:
            name: tool 이름 (MCP tool 이름과 동일)
            fn:   실제 실행 함수 (async, str 반환)
        """
        self._tools[name] = fn

    def has(self, name: str) -> bool:
        """tool 등록 여부 확인."""
        return name in self._tools

    def list_tools(self) -> list[str]:
        """등록된 모든 tool 이름 목록."""
        return list(self._tools.keys())

    async def call(self, name: str, **kwargs: Any) -> str:
        """tool 호출.

        Returns:
            tool 반환 문자열 (JSON 포맷)

        Raises:
            KeyError: 미등록 tool
            Exception: tool 실행 오류
        """
        if name not in self._tools:
            raise KeyError(f"Tool '{name}'이 등록되지 않았습니다. 등록된 tools: {self.list_tools()}")
        return await self._tools[name](**kwargs)


# ---------------------------------------------------------------------------
# 전역 레지스트리 인스턴스 — main.py에서 초기화 시 등록
# ---------------------------------------------------------------------------
_global_registry: ToolRegistry | None = None


def get_registry() -> ToolRegistry:
    """전역 ToolRegistry 반환. 미초기화 시 자동 생성."""
    global _global_registry
    if _global_registry is None:
        _global_registry = ToolRegistry()
    return _global_registry


def register_tool(name: str, fn: Callable[..., Awaitable[str]]) -> None:
    """전역 레지스트리에 tool 등록 (main.py 초기화 시 호출)."""
    get_registry().register(name, fn)
