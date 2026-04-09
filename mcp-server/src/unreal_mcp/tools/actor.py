"""Phase 1 -- Actor & Scene Tool 모음.

UE5 레벨의 액터를 생성.삭제.변환.조회.복제.속성 읽기/쓰기하는 MCP Tool들.
"""

import json
from mcp.server import Server
from ..connection import send_command
from ..utils.validators import validate_actor_name, validate_vector3


def register_actor_tools(server: Server) -> None:
    """액터 관련 Tool들을 MCP 서버에 등록한다."""

    # --------------------------------------------------------------
    # create_actor
    # --------------------------------------------------------------
    @server.tool("create_actor")
    async def create_actor(
        actor_class: str,
        name: str = "",
        location: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
        scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    ) -> str:
        """UE5 레벨에 새 액터를 생성한다.

        Args:
            actor_class: 생성할 액터 클래스 이름.
                         예: "StaticMeshActor", "PointLight", "SpotLight",
                             "DirectionalLight", "CameraActor", "SkyLight",
                             "RectLight", "ExponentialHeightFog"
            name: 액터에 부여할 이름 (비어있으면 UE가 자동 지정).
            location: 월드 좌표 (X, Y, Z) -- 단위: cm.
            rotation: 회전 각도 (Pitch, Yaw, Roll) -- 단위: 도( deg).
            scale: 스케일 (X, Y, Z). 기본값 (1, 1, 1).
        """
        command = {
            "type": "create_actor",
            "params": {
                "actor_class": actor_class.strip(),
                "name": name.strip(),
                "location": validate_vector3(location, "location"),
                "rotation": validate_vector3(rotation, "rotation"),
                "scale": validate_vector3(scale, "scale"),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # delete_actor
    # --------------------------------------------------------------
    @server.tool("delete_actor")
    async def delete_actor(name: str) -> str:
        """레벨에서 지정한 이름의 액터를 삭제한다.

        Args:
            name: 삭제할 액터의 이름 (정확한 이름 또는 레이블).
        """
        command = {
            "type": "delete_actor",
            "params": {"name": validate_actor_name(name)},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # set_actor_transform
    # --------------------------------------------------------------
    @server.tool("set_actor_transform")
    async def set_actor_transform(
        name: str,
        location: tuple[float, float, float] | None = None,
        rotation: tuple[float, float, float] | None = None,
        scale: tuple[float, float, float] | None = None,
    ) -> str:
        """액터의 위치.회전.스케일을 설정한다. None인 항목은 변경하지 않는다.

        Args:
            name: 대상 액터 이름.
            location: 월드 좌표 (X, Y, Z) -- 단위: cm. None이면 유지.
            rotation: 회전 (Pitch, Yaw, Roll) -- 단위: 도( deg). None이면 유지.
            scale: 스케일 (X, Y, Z). None이면 유지.
        """
        params: dict = {"name": validate_actor_name(name)}
        if location is not None:
            params["location"] = validate_vector3(location, "location")
        if rotation is not None:
            params["rotation"] = validate_vector3(rotation, "rotation")
        if scale is not None:
            params["scale"] = validate_vector3(scale, "scale")

        result = await send_command({"type": "set_actor_transform", "params": params})
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # get_actors_in_level
    # --------------------------------------------------------------
    @server.tool("get_actors_in_level")
    async def get_actors_in_level(
        actor_class_filter: str = "",
    ) -> str:
        """현재 레벨에 있는 모든 액터 목록을 반환한다.

        Args:
            actor_class_filter: 특정 클래스로 필터링할 경우 클래스 이름 입력.
                                 예: "StaticMeshActor". 비어있으면 전체 조회.
        """
        command = {
            "type": "get_actors_in_level",
            "params": {"actor_class_filter": actor_class_filter.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # find_actors_by_name
    # --------------------------------------------------------------
    @server.tool("find_actors_by_name")
    async def find_actors_by_name(pattern: str) -> str:
        """이름 패턴으로 레벨의 액터를 검색한다 (부분 일치).

        Args:
            pattern: 검색할 이름 패턴. 대소문자 구분 없음.
                     예: "BP_", "Light", "Player"
        """
        if not pattern.strip():
            return json.dumps(
                {"success": False, "error": {"code": "INVALID_PARAMS", "message": "검색 패턴이 비어있습니다."}},
                indent=2, ensure_ascii=False,
            )
        command = {
            "type": "find_actors_by_name",
            "params": {"pattern": pattern.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # duplicate_actor
    # --------------------------------------------------------------
    @server.tool("duplicate_actor")
    async def duplicate_actor(
        name: str,
        new_name: str = "",
        offset: tuple[float, float, float] = (100.0, 0.0, 0.0),
    ) -> str:
        """액터를 복제한다.

        Args:
            name: 복제할 원본 액터 이름.
            new_name: 복제본에 부여할 이름. 비어있으면 UE가 자동 지정.
            offset: 원본 위치 대비 복제본 위치 오프셋 (X, Y, Z) -- 단위: cm.
        """
        command = {
            "type": "duplicate_actor",
            "params": {
                "name": validate_actor_name(name),
                "new_name": new_name.strip(),
                "offset": validate_vector3(offset, "offset"),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # get_actor_properties
    # --------------------------------------------------------------
    @server.tool("get_actor_properties")
    async def get_actor_properties(name: str) -> str:
        """액터의 주요 속성(Transform, Tags, Hidden 등)을 읽어온다.

        Args:
            name: 조회할 액터 이름.
        """
        command = {
            "type": "get_actor_properties",
            "params": {"name": validate_actor_name(name)},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # set_actor_property
    # --------------------------------------------------------------
    @server.tool("set_actor_property")
    async def set_actor_property(
        name: str,
        property_name: str,
        property_value: str,
    ) -> str:
        """액터의 특정 속성 값을 설정한다.

        Args:
            name: 대상 액터 이름.
            property_name: 설정할 속성 이름.
                           예: "bHidden", "Tags", "CustomDepthStencilValue"
            property_value: 설정할 값 (JSON 문자열 형식).
                            예: "true", "42", '"MyTag"', '["Tag1","Tag2"]'
        """
        # property_value는 JSON 문자열로 파싱하여 실제 값으로 변환
        try:
            parsed_value = json.loads(property_value)
        except json.JSONDecodeError:
            # JSON 파싱 실패 시 문자열 그대로 전달
            parsed_value = property_value

        command = {
            "type": "set_actor_property",
            "params": {
                "name": validate_actor_name(name),
                "property_name": property_name.strip(),
                "property_value": parsed_value,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
