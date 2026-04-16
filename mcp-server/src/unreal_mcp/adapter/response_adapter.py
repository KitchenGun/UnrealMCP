"""ResponseAdapter — MCP Tool 결과 → Hermes 응답 포맷 변환

역할:
    MCP Tool이 반환하는 JSON 문자열을
    Hermes / Discord가 기대하는 응답 포맷으로 변환한다.

❗ 기존 응답 포맷 절대 변경 금지 ❗
    Hermes 기대 포맷:
        {
            "ok": bool,
            "result_text": str,
            "mode": "unreal-mcp-adapter",
            "stderr": str,
            "returncode": int | None,
        }

MCP Tool 원본 포맷 (KitchenGun 표준):
    성공:
        {"id": "...", "success": true, "result": {...}, "error": null}
    실패:
        {"id": "...", "success": false, "result": null,
         "error": {"code": "ACTOR_NOT_FOUND", "message": "..."}}

chongdashu 레퍼런스 참고:
    - 두 가지 성공 포맷을 모두 처리 (status/success 혼재 케이스 포함)
    - result 내용을 사람이 읽기 쉬운 텍스트로 요약
"""

from __future__ import annotations

import json
from typing import Any


# ---------------------------------------------------------------------------
# 결과 요약 함수 — tool별 사람 친화적 텍스트 생성
# ---------------------------------------------------------------------------

def _summarize_actor(result: dict[str, Any]) -> str:
    """create_actor / get_actor_properties 결과 요약."""
    name = result.get("name", "")
    cls = result.get("actor_class", "")
    loc = result.get("location", {})
    if isinstance(loc, dict):
        x, y, z = loc.get("x", 0), loc.get("y", 0), loc.get("z", 0)
        loc_str = f"({x}, {y}, {z})"
    elif isinstance(loc, list) and len(loc) >= 3:
        loc_str = f"({loc[0]}, {loc[1]}, {loc[2]})"
    else:
        loc_str = str(loc)

    parts = []
    if name:
        parts.append(f"name={name!r}")
    if cls:
        parts.append(f"class={cls}")
    if loc_str and loc_str != "(0, 0, 0)":
        parts.append(f"location={loc_str}")
    return ", ".join(parts) if parts else json.dumps(result, ensure_ascii=False)


def _summarize_actors_list(result: dict[str, Any]) -> str:
    """get_actors_in_level / find_actors_by_name 결과 요약."""
    actors = result.get("actors", [])
    count = result.get("count", len(actors))
    if not actors:
        return f"레벨에 해당 액터 없음 (total={count})"
    names = [a.get("name", "?") for a in actors[:10]]
    suffix = f" ... 외 {count - 10}개" if count > 10 else ""
    return f"총 {count}개: {', '.join(names)}{suffix}"


def _summarize_deleted(result: dict[str, Any]) -> str:
    deleted = result.get("deleted", "")
    return f"삭제 완료: {deleted!r}" if deleted else "삭제 완료"


def _summarize_transform(result: dict[str, Any]) -> str:
    return f"트랜스폼 업데이트: {_summarize_actor(result)}"


def _summarize_generic(result: dict[str, Any]) -> str:
    return json.dumps(result, ensure_ascii=False, indent=None)


# tool 이름 → 결과 요약 함수 매핑
_SUMMARIZERS: dict[str, Any] = {
    "create_actor":          _summarize_actor,
    "get_actor_properties":  _summarize_actor,
    "duplicate_actor":       _summarize_actor,
    "set_actor_property":    _summarize_actor,
    "delete_actor":          _summarize_deleted,
    "set_actor_transform":   _summarize_transform,
    "get_actors_in_level":   _summarize_actors_list,
    "find_actors_by_name":   _summarize_actors_list,
}


# ---------------------------------------------------------------------------
# ResponseAdapter
# ---------------------------------------------------------------------------

class ResponseAdapter:
    """MCP Tool JSON 결과 → Hermes 응답 포맷 변환.

    기존 normalize_discord_execution() / quality_check()가
    기대하는 정확한 포맷을 반환한다.

    외부 인터페이스:
        adapter = ResponseAdapter()
        hermes_result = adapter.to_hermes(tool_name, raw_json_str)
        # → {"ok": bool, "result_text": str, "mode": "unreal-mcp-adapter", ...}
    """

    def to_hermes(
        self,
        tool_name: str,
        raw_json_str: str,
        *,
        idempotency_skipped: bool = False,
    ) -> dict[str, Any]:
        """MCP tool 반환 JSON → Hermes 포맷 변환.

        Args:
            tool_name:           실행된 MCP tool 이름
            raw_json_str:        tool이 반환한 JSON 문자열
            idempotency_skipped: 멱등성 가드에 의해 실행이 스킵됐는지 여부

        Returns:
            Hermes 기대 포맷:
                {"ok": bool, "result_text": str, "mode": str, "stderr": str, ...}
        """
        parsed = self._parse_raw(raw_json_str)

        if parsed is None:
            # JSON 파싱 완전 실패 → raw 텍스트 fallback
            return self._make_result(
                ok=bool(raw_json_str.strip()),
                result_text=raw_json_str.strip()[:2000] if raw_json_str.strip() else "응답을 파싱할 수 없습니다",
                stderr="JSON parse failed",
            )

        # KitchenGun 표준 포맷: {"success": bool, "result": {...}, "error": {...}}
        if "success" in parsed:
            return self._from_standard(tool_name, parsed, idempotency_skipped)

        # chongdashu 호환 포맷: {"status": "success"|"error", ...}
        if "status" in parsed:
            return self._from_legacy(tool_name, parsed)

        # 알 수 없는 포맷 → 텍스트 그대로 반환
        return self._make_result(
            ok=True,
            result_text=raw_json_str.strip()[:2000],
            stderr="",
        )

    def _from_standard(
        self,
        tool_name: str,
        parsed: dict[str, Any],
        idempotency_skipped: bool,
    ) -> dict[str, Any]:
        """KitchenGun 표준 포맷 처리."""
        success = bool(parsed.get("success", False))
        result = parsed.get("result") or {}
        error = parsed.get("error") or {}

        if success:
            summarizer = _SUMMARIZERS.get(tool_name, _summarize_generic)
            summary = summarizer(result) if isinstance(result, dict) else str(result)
            prefix = "[중복 방지: 캐시 결과] " if idempotency_skipped else ""
            return self._make_result(ok=True, result_text=f"{prefix}{summary}")

        # 실패 케이스
        if isinstance(error, dict):
            code = error.get("code", "UNKNOWN")
            message = error.get("message", "알 수 없는 오류")
            error_text = f"[{code}] {message}"
        else:
            error_text = str(error) or "요청을 실행할 수 없습니다"

        return self._make_result(ok=False, result_text=error_text, stderr=error_text)

    def _from_legacy(self, tool_name: str, parsed: dict[str, Any]) -> dict[str, Any]:
        """chongdashu 레거시 포맷 처리 (status 필드 기반)."""
        status = str(parsed.get("status", "")).lower()
        if status == "success":
            result = parsed.get("result", parsed)
            summarizer = _SUMMARIZERS.get(tool_name, _summarize_generic)
            summary = summarizer(result) if isinstance(result, dict) else str(result)
            return self._make_result(ok=True, result_text=summary)

        error_msg = str(parsed.get("error", parsed.get("message", "요청을 실행할 수 없습니다")))
        return self._make_result(ok=False, result_text=error_msg, stderr=error_msg)

    @staticmethod
    def _parse_raw(raw: str) -> dict[str, Any] | None:
        """JSON 문자열 파싱. 실패 시 None."""
        if not raw:
            return None
        try:
            parsed = json.loads(raw.strip())
            return parsed if isinstance(parsed, dict) else None
        except (json.JSONDecodeError, ValueError):
            return None

    @staticmethod
    def _make_result(
        ok: bool,
        result_text: str,
        stderr: str = "",
        returncode: int | None = None,
    ) -> dict[str, Any]:
        """Hermes 기대 포맷으로 결과 생성.

        ❗ 이 포맷은 절대 변경하지 않는다 ❗
        normalize_discord_execution() / quality_check()가 이 구조를 파싱한다.
        """
        return {
            "ok": ok,
            "result_text": result_text,
            "mode": "unreal-mcp-adapter",
            "stderr": stderr,
            "returncode": returncode,
            # 하위 호환: opencode_backend와 동일한 키 제공
            "stdout": result_text,
        }

    def error_result(self, message: str) -> dict[str, Any]:
        """오류 결과 생성 (기존 포맷 유지)."""
        return self._make_result(ok=False, result_text=message, stderr=message)
