"""RequestAdapter — GPT intent → MCP Tool 호출 변환

역할:
    GPT가 출력한 자유 형식 텍스트 또는 구조화 JSON을 파싱하여
    정해진 MCP Tool 이름 + 파라미터로 변환한다.

설계 원칙 (chongdashu 레퍼런스 참고):
    - 모든 매핑은 ACTION → TOOL 테이블로 선언적으로 관리
    - 파라미터 정규화 (대소문자, 별칭, 기본값)
    - 파싱 실패 시 IntentParseError 발생 → 기존 에러 포맷으로 변환

GPT에게 기대하는 intent JSON 포맷:
    {
        "action": "create",          # 필수
        "class": "PointLight",       # actor 관련
        "name": "MyLight",
        "location": [1000, 1000, 300],
        "rotation": [0, 0, 0],
        "scale": [1, 1, 1],
        "pattern": "Light",          # find 관련
        "filter": "PointLight",      # query 관련
        "property": "bHidden",       # set_prop 관련
        "value": "true",
        "tool": "create_actor"       # 직접 tool 지정 (선택)
    }

기존 인터페이스 유지:
    이 모듈은 외부(Hermes/Discord/OpenCode)에 노출되지 않는다.
    orchestrator._build_unreal_mcp_prompt()에서 GPT에게 intent JSON 형식을
    요청하는 프롬프트를 추가하는 것으로 연결된다.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from typing import Any


class IntentParseError(ValueError):
    """GPT intent 파싱 실패."""


@dataclass
class ToolCall:
    """RequestAdapter가 반환하는 MCP Tool 호출 명세."""

    tool_name: str
    params: dict[str, Any] = field(default_factory=dict)

    def __repr__(self) -> str:
        return f"ToolCall(tool={self.tool_name!r}, params={self.params})"


# ---------------------------------------------------------------------------
# Action → Tool 매핑 테이블
# ---------------------------------------------------------------------------

# 기본 액션명 별칭 (GPT 출력이 다양할 수 있음)
_ACTION_ALIASES: dict[str, str] = {
    # create 계열
    "create":        "create",
    "spawn":         "create",
    "add":           "create",
    "place":         "create",
    "생성":          "create",
    "배치":          "create",
    "추가":          "create",
    # delete 계열
    "delete":        "delete",
    "remove":        "delete",
    "destroy":       "delete",
    "삭제":          "delete",
    # transform 계열
    "move":          "transform",
    "set_transform": "transform",
    "transform":     "transform",
    "이동":          "transform",
    "위치":          "transform",
    # query 계열
    "list":          "query",
    "get_all":       "query",
    "query":         "query",
    "목록":          "query",
    # find 계열
    "find":          "find",
    "search":        "find",
    "찾기":          "find",
    # properties 계열
    "get_props":     "get_props",
    "properties":    "get_props",
    "get":           "get_props",
    "속성":          "get_props",
    # set_prop 계열
    "set_prop":      "set_prop",
    "set_property":  "set_prop",
    "set":           "set_prop",
    "설정":          "set_prop",
    # duplicate
    "duplicate":     "duplicate",
    "copy":          "duplicate",
    "복제":          "duplicate",
}

# Actor 클래스 별칭 정규화
_CLASS_ALIASES: dict[str, str] = {
    "pointlight":          "PointLight",
    "point_light":         "PointLight",
    "point light":         "PointLight",
    "포인트라이트":         "PointLight",
    "spotlight":           "SpotLight",
    "spot_light":          "SpotLight",
    "spot light":          "SpotLight",
    "directionallight":    "DirectionalLight",
    "directional_light":   "DirectionalLight",
    "directional light":   "DirectionalLight",
    "skylight":            "SkyLight",
    "sky_light":           "SkyLight",
    "sky light":           "SkyLight",
    "rectlight":           "RectLight",
    "rect_light":          "RectLight",
    "rect light":          "RectLight",
    "staticmeshactor":     "StaticMeshActor",
    "static_mesh_actor":   "StaticMeshActor",
    "staticmesh":          "StaticMeshActor",
    "cameraactor":         "CameraActor",
    "camera":              "CameraActor",
    "fog":                 "ExponentialHeightFog",
    "heightfog":           "ExponentialHeightFog",
}


def _normalize_class(raw: str) -> str:
    """액터 클래스명 정규화."""
    return _CLASS_ALIASES.get(raw.lower().strip(), raw.strip())


def _normalize_vector(raw: Any, default: list[float] | None = None) -> list[float]:
    """위치/회전/스케일 벡터 정규화.

    입력 형식:
        [1000, 1000, 300]
        {"x": 1000, "y": 1000, "z": 300}
        "1000,1000,300"
    """
    if raw is None:
        return default or [0.0, 0.0, 0.0]

    if isinstance(raw, (list, tuple)):
        nums = [float(v) for v in raw]
        if len(nums) < 3:
            nums += [0.0] * (3 - len(nums))
        return nums[:3]

    if isinstance(raw, dict):
        return [
            float(raw.get("x", raw.get("X", 0.0))),
            float(raw.get("y", raw.get("Y", 0.0))),
            float(raw.get("z", raw.get("Z", 0.0))),
        ]

    if isinstance(raw, str):
        parts = re.split(r"[,\s]+", raw.strip())
        try:
            nums = [float(p) for p in parts if p]
            if len(nums) < 3:
                nums += [0.0] * (3 - len(nums))
            return nums[:3]
        except ValueError:
            pass

    return default or [0.0, 0.0, 0.0]


# ---------------------------------------------------------------------------
# 핵심 매핑 함수들
# ---------------------------------------------------------------------------

def _map_create(intent: dict[str, Any]) -> ToolCall:
    actor_class = _normalize_class(str(intent.get("class", intent.get("actor_class", "StaticMeshActor"))))
    return ToolCall(
        tool_name="create_actor",
        params={
            "actor_class": actor_class,
            "name": str(intent.get("name", "")),
            "location": _normalize_vector(intent.get("location")),
            "rotation": _normalize_vector(intent.get("rotation")),
            "scale": _normalize_vector(intent.get("scale"), default=[1.0, 1.0, 1.0]),
        },
    )


def _map_delete(intent: dict[str, Any]) -> ToolCall:
    name = str(intent.get("name", "")).strip()
    if not name:
        raise IntentParseError("delete 명령에 'name' 필드가 필요합니다.")
    return ToolCall(tool_name="delete_actor", params={"name": name})


def _map_transform(intent: dict[str, Any]) -> ToolCall:
    name = str(intent.get("name", "")).strip()
    if not name:
        raise IntentParseError("transform 명령에 'name' 필드가 필요합니다.")
    params: dict[str, Any] = {"name": name}
    if "location" in intent:
        params["location"] = _normalize_vector(intent["location"])
    if "rotation" in intent:
        params["rotation"] = _normalize_vector(intent["rotation"])
    if "scale" in intent:
        params["scale"] = _normalize_vector(intent["scale"], default=[1.0, 1.0, 1.0])
    return ToolCall(tool_name="set_actor_transform", params=params)


def _map_query(intent: dict[str, Any]) -> ToolCall:
    return ToolCall(
        tool_name="get_actors_in_level",
        params={"actor_class_filter": str(intent.get("filter", intent.get("class", "")))},
    )


def _map_find(intent: dict[str, Any]) -> ToolCall:
    pattern = str(intent.get("pattern", intent.get("name", ""))).strip()
    if not pattern:
        raise IntentParseError("find 명령에 'pattern' 필드가 필요합니다.")
    return ToolCall(tool_name="find_actors_by_name", params={"pattern": pattern})


def _map_get_props(intent: dict[str, Any]) -> ToolCall:
    name = str(intent.get("name", "")).strip()
    if not name:
        raise IntentParseError("get_props 명령에 'name' 필드가 필요합니다.")
    return ToolCall(tool_name="get_actor_properties", params={"name": name})


def _map_set_prop(intent: dict[str, Any]) -> ToolCall:
    name = str(intent.get("name", "")).strip()
    prop = str(intent.get("property", intent.get("property_name", ""))).strip()
    value = intent.get("value", intent.get("property_value", ""))
    if not name or not prop:
        raise IntentParseError("set_prop 명령에 'name', 'property', 'value' 필드가 필요합니다.")
    return ToolCall(
        tool_name="set_actor_property",
        params={
            "name": name,
            "property_name": prop,
            "property_value": json.dumps(value, ensure_ascii=False),
        },
    )


def _map_duplicate(intent: dict[str, Any]) -> ToolCall:
    name = str(intent.get("name", "")).strip()
    if not name:
        raise IntentParseError("duplicate 명령에 'name' 필드가 필요합니다.")
    return ToolCall(
        tool_name="duplicate_actor",
        params={
            "name": name,
            "new_name": str(intent.get("new_name", "")),
            "offset": _normalize_vector(intent.get("offset"), default=[100.0, 0.0, 0.0]),
        },
    )


# action 문자열 → 매핑 함수
_ACTION_MAP: dict[str, Any] = {
    "create":     _map_create,
    "delete":     _map_delete,
    "transform":  _map_transform,
    "query":      _map_query,
    "find":       _map_find,
    "get_props":  _map_get_props,
    "set_prop":   _map_set_prop,
    "duplicate":  _map_duplicate,
}


# ---------------------------------------------------------------------------
# RequestAdapter
# ---------------------------------------------------------------------------

class RequestAdapter:
    """GPT 출력 → MCP ToolCall 변환.

    GPT가 반환하는 텍스트에서 intent JSON을 추출하고
    대응하는 MCP Tool 호출로 변환한다.

    변환 우선순위:
        1. intent에 "tool" 키가 있으면 직접 tool 호출 (pass-through)
        2. intent에 "action" 키가 있으면 Action → Tool 매핑 수행
        3. 둘 다 없으면 IntentParseError
    """

    def parse(self, raw_input: str | dict[str, Any]) -> ToolCall:
        """GPT 출력 → ToolCall 변환.

        Args:
            raw_input: GPT가 반환한 JSON 문자열 또는 dict

        Returns:
            ToolCall 객체

        Raises:
            IntentParseError: 파싱/매핑 실패
        """
        intent = self._extract_intent(raw_input)

        # 1. 직접 tool 지정 (pass-through)
        if "tool" in intent:
            tool_name = str(intent.pop("tool")).strip()
            # tool 이름 외 나머지를 params로 사용
            params = {k: v for k, v in intent.items() if k not in ("action",)}
            return ToolCall(tool_name=tool_name, params=params)

        # 2. action → tool 매핑
        raw_action = str(intent.get("action", "")).strip().lower()
        if not raw_action:
            raise IntentParseError(
                f"intent에 'action' 또는 'tool' 필드가 없습니다. 받은 intent: {intent}"
            )

        normalized = _ACTION_ALIASES.get(raw_action)
        if not normalized:
            raise IntentParseError(
                f"알 수 없는 action '{raw_action}'. "
                f"지원 액션: {sorted(set(_ACTION_ALIASES.values()))}"
            )

        mapper = _ACTION_MAP.get(normalized)
        if not mapper:
            raise IntentParseError(f"action '{normalized}'에 대한 mapper가 없습니다.")

        return mapper(intent)

    def _extract_intent(self, raw: str | dict[str, Any]) -> dict[str, Any]:
        """raw 입력에서 intent dict 추출."""
        if isinstance(raw, dict):
            return dict(raw)

        text = str(raw).strip()

        # JSON 블록 추출 (```json ... ``` 또는 { ... })
        json_block = self._find_json_block(text)
        if json_block:
            try:
                parsed = json.loads(json_block)
                if isinstance(parsed, dict):
                    return parsed
            except json.JSONDecodeError:
                pass

        # 직접 JSON 파싱 시도
        try:
            parsed = json.loads(text)
            if isinstance(parsed, dict):
                return parsed
        except json.JSONDecodeError:
            pass

        raise IntentParseError(
            f"GPT 출력에서 intent JSON을 추출할 수 없습니다. "
            f"입력 (앞 200자): {text[:200]}"
        )

    @staticmethod
    def _find_json_block(text: str) -> str | None:
        """텍스트에서 JSON 블록 추출."""
        # ```json ... ``` 마크다운 블록
        md_match = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", text, re.DOTALL)
        if md_match:
            return md_match.group(1)

        # 첫 번째 { ... } 블록
        brace_match = re.search(r"(\{[^{}]*(?:\{[^{}]*\}[^{}]*)?\})", text, re.DOTALL)
        if brace_match:
            return brace_match.group(1)

        return None
