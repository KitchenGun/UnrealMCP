"""IdempotencyGuard — UnrealMCP 명령 중복 실행 방지

문제:
    OpenCode/GPT가 타임아웃 후 동일 명령을 재시도하면
    UE5에서 actor가 중복 생성된다.

해결:
    create/spawn/place 계열 명령에 멱등성 키를 부여하여
    동일 키가 지정된 시간 내에 재시도되면 실행을 막고
    첫 번째 결과를 반환한다.

멱등성 키 구성:
    sha256(tool_name + sorted(params))[:16]
    → 동일 명령이면 항상 같은 키 생성
"""

from __future__ import annotations

import hashlib
import json
import time
from typing import Any


# 멱등성을 적용할 tool 목록 (부수효과 발생 명령)
IDEMPOTENT_TOOLS = {
    "create_actor",
    "duplicate_actor",
    "spawn_blueprint_actor",
    "create_blueprint",
    "create_material",
    "create_behavior_tree",
    "create_blackboard",
    "create_niagara_system",
    "create_animation_blueprint",
    "create_widget_blueprint",
    "create_data_table",
    "create_data_asset",
    "create_ai_controller",
    "create_eqs_query",
}

# 읽기 전용 tool — 멱등성 불필요
READ_ONLY_TOOLS = {
    "get_actors_in_level",
    "find_actors_by_name",
    "get_actor_properties",
    "get_blueprint_graph",
    "get_asset_details",
    "search_assets",
    "get_selected_actors",
    "inspect_uobject",
}

# 멱등성 윈도우 (초) — 이 시간 내 동일 키 재시도 차단
DEDUP_WINDOW_SECONDS = 60


class IdempotencyGuard:
    """멱등성 키 기반 중복 실행 방지.

    사용법:
        guard = IdempotencyGuard()
        key = guard.make_key("create_actor", {"actor_class": "PointLight", "location": [100,100,300]})

        if guard.is_duplicate(key):
            return guard.get_cached_result(key)

        result = await execute_tool(...)
        guard.record(key, result)
    """

    def __init__(self, window_seconds: int = DEDUP_WINDOW_SECONDS) -> None:
        self._window = window_seconds
        # key → (timestamp, result_str)
        self._cache: dict[str, tuple[float, str]] = {}

    def make_key(self, tool_name: str, params: dict[str, Any]) -> str:
        """멱등성 키 생성.

        동일한 tool + params 조합은 항상 동일한 키를 반환한다.
        """
        payload = json.dumps({"tool": tool_name, "params": params}, sort_keys=True, ensure_ascii=False)
        return hashlib.sha256(payload.encode()).hexdigest()[:16]

    def needs_guard(self, tool_name: str) -> bool:
        """이 tool에 멱등성 가드가 필요한지 여부."""
        return tool_name in IDEMPOTENT_TOOLS

    def is_duplicate(self, key: str) -> bool:
        """동일 키의 실행이 윈도우 내에 존재하는지 확인."""
        self._prune()
        return key in self._cache

    def get_cached_result(self, key: str) -> str | None:
        """캐시된 결과 반환 (없으면 None)."""
        entry = self._cache.get(key)
        return entry[1] if entry else None

    def record(self, key: str, result: str) -> None:
        """실행 결과를 캐시에 기록."""
        self._cache[key] = (time.time(), result)

    def _prune(self) -> None:
        """만료된 캐시 항목 제거."""
        cutoff = time.time() - self._window
        expired = [k for k, (ts, _) in self._cache.items() if ts < cutoff]
        for k in expired:
            del self._cache[k]

    def clear(self) -> None:
        """캐시 전체 초기화."""
        self._cache.clear()


# 전역 인스턴스
_global_guard: IdempotencyGuard | None = None


def get_guard() -> IdempotencyGuard:
    """전역 IdempotencyGuard 반환."""
    global _global_guard
    if _global_guard is None:
        _global_guard = IdempotencyGuard()
    return _global_guard
