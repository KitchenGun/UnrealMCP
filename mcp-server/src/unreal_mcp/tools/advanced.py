"""Phase 6 -- Advanced Systems tools.

MCP tools for advanced UE5 systems:
Niagara, AnimBlueprint, Widget Blueprint, DataTable, DataAsset, UObject inspection.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command


def register_advanced_tools(server: Server) -> None:
    """Register Advanced Systems tools to MCP server."""

    # ------------------------------------------------------------------
    # create_niagara_system
    # ------------------------------------------------------------------
    @server.tool("create_niagara_system")
    async def create_niagara_system(
        name: str,
        save_path: str = "/Game/VFX",
        template: str = "Empty",
    ) -> str:
        """새 Niagara System 에셋을 생성한다.

        Args:
            name: Niagara System 에셋 이름 (예: "NS_Explosion", "NS_Fire").
            save_path: 저장 경로 (예: "/Game/VFX").
            template: 시작 템플릿.
                      "Empty"         -- 빈 시스템
                      "Fountain"      -- 분수 이펙트 템플릿
                      "DirectionalBurst" -- 방향성 버스트 템플릿
        """
        command = {
            "type": "create_niagara_system",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "template": template.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_animation_blueprint
    # ------------------------------------------------------------------
    @server.tool("create_animation_blueprint")
    async def create_animation_blueprint(
        name: str,
        save_path: str = "/Game/Animations",
        skeleton_path: str = "",
        parent_class: str = "AnimInstance",
    ) -> str:
        """새 Animation Blueprint 에셋을 생성한다.

        Args:
            name: Animation Blueprint 이름 (예: "ABP_Character").
            save_path: 저장 경로 (예: "/Game/Animations").
            skeleton_path: 연결할 Skeleton 에셋 경로.
                           예: "/Game/Characters/SK_Mannequin_Skeleton"
                           비어있으면 Skeleton 없이 생성.
            parent_class: 부모 클래스. 기본값 "AnimInstance".
                          예: "AnimInstance", "VehicleAnimInstance"
        """
        command = {
            "type": "create_animation_blueprint",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "skeleton_path": skeleton_path.strip(),
                "parent_class": parent_class.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_widget_blueprint
    # ------------------------------------------------------------------
    @server.tool("create_widget_blueprint")
    async def create_widget_blueprint(
        name: str,
        save_path: str = "/Game/UI",
        parent_class: str = "UserWidget",
    ) -> str:
        """새 Widget Blueprint (UMG) 에셋을 생성한다.

        Args:
            name: Widget Blueprint 이름 (예: "WBP_HUD", "WBP_MainMenu").
            save_path: 저장 경로 (예: "/Game/UI").
            parent_class: 부모 클래스.
                          "UserWidget"               -- 기본 UMG 위젯
                          "CommonActivatableWidget"  -- CommonUI Stack 호환 위젯
                          "CommonUserWidget"          -- CommonUI 기본 위젯
        """
        command = {
            "type": "create_widget_blueprint",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "parent_class": parent_class.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_data_table
    # ------------------------------------------------------------------
    @server.tool("create_data_table")
    async def create_data_table(
        name: str,
        row_struct: str,
        save_path: str = "/Game/Data",
    ) -> str:
        """새 DataTable 에셋을 생성한다.

        Args:
            name: DataTable 에셋 이름 (예: "DT_ItemStats", "DT_EnemyConfig").
            row_struct: 각 행의 데이터 구조체 이름 또는 경로.
                        예: "FItemStatRow" (프로젝트 내 구조체)
                            "/Script/MyGame.FItemStatRow"
                            "FTableRowBase" (UE 기본 구조체)
            save_path: 저장 경로 (예: "/Game/Data").
        """
        command = {
            "type": "create_data_table",
            "params": {
                "name": name.strip(),
                "row_struct": row_struct.strip(),
                "save_path": save_path.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_data_asset
    # ------------------------------------------------------------------
    @server.tool("create_data_asset")
    async def create_data_asset(
        name: str,
        asset_class: str,
        save_path: str = "/Game/Data",
    ) -> str:
        """새 DataAsset 에셋을 생성한다.

        Args:
            name: DataAsset 이름 (예: "DA_WeaponConfig", "DA_EnemyData").
            asset_class: DataAsset 서브클래스 이름 또는 경로.
                         예: "PrimaryDataAsset"
                             "/Script/MyGame.UWeaponDataAsset"
            save_path: 저장 경로 (예: "/Game/Data").
        """
        command = {
            "type": "create_data_asset",
            "params": {
                "name": name.strip(),
                "asset_class": asset_class.strip(),
                "save_path": save_path.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # inspect_uobject
    # ------------------------------------------------------------------
    @server.tool("inspect_uobject")
    async def inspect_uobject(
        object_path: str,
        include_properties: bool = True,
        include_functions: bool = False,
        property_filter: str = "",
    ) -> str:
        """UObject의 리플렉션 정보를 검사한다.

        클래스 계층, UPROPERTY 목록, UFUNCTION 목록을 반환한다.

        Args:
            object_path: 검사할 UObject 경로 또는 클래스 이름.
                         에셋 경로 예: "/Game/Blueprints/BP_MyActor"
                         클래스 이름 예: "StaticMeshActor", "Character"
            include_properties: True이면 UPROPERTY 목록 포함.
            include_functions: True이면 UFUNCTION 목록 포함.
            property_filter: 속성 이름 필터 (부분 일치).
                             빈 문자열이면 전체 속성 반환.
        """
        command = {
            "type": "inspect_uobject",
            "params": {
                "object_path": object_path.strip(),
                "include_properties": include_properties,
                "include_functions": include_functions,
                "property_filter": property_filter.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
