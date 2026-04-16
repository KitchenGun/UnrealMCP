"""UnrealMCPAdapter — 어댑터 레이어 메인 진입점

체인:
    GPT intent JSON (str)
    → RequestAdapter   → ToolCall
    → IdempotencyGuard → (캐시 히트 시 캐시 결과 반환)
    → UE5 TCP          → raw response dict
    → ResponseAdapter  → Hermes 포맷 dict

사용법:
    adapter = UnrealMCPAdapter()

    # 비동기 (MCP 서버 내부)
    result = await adapter.execute('{"action":"create","class":"PointLight","location":[0,0,300]}')

    # 동기 (외부 프로세스에서 asyncio.run 사용)
    result = adapter.execute_sync('{"action":"create","class":"PointLight"}')

반환 포맷 (Hermes 기대값):
    {"ok": bool, "result_text": str, "mode": "unreal-mcp-adapter", "stderr": str, "returncode": None}
"""

from __future__ import annotations

import asyncio
import json
import concurrent.futures
from typing import Any

from .request_adapter import RequestAdapter, IntentParseError
from .response_adapter import ResponseAdapter
from .idempotency import IdempotencyGuard, get_guard


class UnrealMCPAdapter:
    """GPT intent JSON → UE5 실행 → Hermes 포맷 변환 메인 어댑터.

    RequestAdapter, IdempotencyGuard, ResponseAdapter를 체인으로 묶어
    외부에 단일 인터페이스를 제공한다.
    """

    def __init__(self, guard: IdempotencyGuard | None = None) -> None:
        self._req = RequestAdapter()
        self._res = ResponseAdapter()
        self._guard = guard or get_guard()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    async def execute(self, raw_intent: str) -> dict[str, Any]:
        """비동기 실행.

        Args:
            raw_intent: GPT가 출력한 텍스트 (intent JSON 포함)

        Returns:
            Hermes 기대 포맷 dict
        """
        # 1. Intent 파싱
        try:
            tool_call = self._req.parse(raw_intent)
        except IntentParseError as exc:
            return self._res.error_result(f"Intent 파싱 실패: {exc}")

        # 2. 멱등성 중복 체크
        idem_key: str | None = None
        if self._guard.needs_guard(tool_call.tool_name):
            idem_key = self._guard.make_key(tool_call.tool_name, tool_call.params)
            if self._guard.is_duplicate(idem_key):
                cached = self._guard.get_cached_result(idem_key)
                if cached:
                    return self._res.to_hermes(
                        tool_call.tool_name, cached, idempotency_skipped=True
                    )

        # 3. UE5 TCP 실행
        try:
            result_dict = await self._call_ue5(tool_call.tool_name, tool_call.params)
        except ConnectionError as exc:
            return self._res.error_result(f"UE5 연결 실패: {exc}")
        except TimeoutError as exc:
            return self._res.error_result(f"UE5 응답 타임아웃: {exc}")
        except Exception as exc:  # noqa: BLE001
            return self._res.error_result(f"실행 오류: {exc}")

        # 4. 멱등성 결과 기록
        raw_json_str = json.dumps(result_dict, ensure_ascii=False)
        if idem_key is not None:
            self._guard.record(idem_key, raw_json_str)

        # 5. Hermes 포맷 변환 후 반환
        return self._res.to_hermes(tool_call.tool_name, raw_json_str)

    def execute_sync(self, raw_intent: str) -> dict[str, Any]:
        """동기 래퍼. 이벤트 루프가 없는 스레드에서 호출 가능.

        이미 실행 중인 이벤트 루프(Jupyter, 서버 등)에서는
        ThreadPoolExecutor를 통해 새 루프에서 실행한다.
        """
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            # 이벤트 루프 없음 → 직접 실행
            return asyncio.run(self.execute(raw_intent))

        # 이미 루프가 있는 경우 → 별도 스레드에서 새 루프 실행
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            future = pool.submit(_run_async, self.execute(raw_intent))
            return future.result()

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    async def _call_ue5(
        self,
        tool_name: str,
        params: dict[str, Any],
        timeout: float = 30.0,
    ) -> dict[str, Any]:
        """UE5 TCP 소켓으로 직접 명령 전송.

        connection.send_command()를 사용. 연결 실패 시 ConnectionError,
        타임아웃 시 TimeoutError를 raise한다.
        """
        from ..connection import send_command  # 지연 import (순환 참조 방지)

        command = {"type": tool_name, "params": params}
        return await send_command(command, timeout=timeout)


# ---------------------------------------------------------------------------
# 전역 싱글턴
# ---------------------------------------------------------------------------

_global_adapter: UnrealMCPAdapter | None = None


def get_adapter() -> UnrealMCPAdapter:
    """전역 UnrealMCPAdapter 반환."""
    global _global_adapter
    if _global_adapter is None:
        _global_adapter = UnrealMCPAdapter()
    return _global_adapter


def _run_async(coro: Any) -> Any:
    """새 이벤트 루프에서 코루틴 실행 (스레드 전용)."""
    return asyncio.run(coro)
