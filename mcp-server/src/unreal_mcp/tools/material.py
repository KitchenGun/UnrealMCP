"""Phase 3 -- Material & Asset tools.

MCP tools for searching/creating/editing materials and managing assets in UE5.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command


def register_material_tools(server: Server) -> None:
    """Register Material & Asset related tools to MCP server."""

    # ------------------------------------------------------------------
    # search_assets
    # ------------------------------------------------------------------
    @server.tool("search_assets")
    async def search_assets(
        query: str,
        asset_class_filter: str = "",
        search_path: str = "/Game",
        limit: int = 50,
        offset: int = 0,
    ) -> str:
        """[Material] 에셋 레지스트리에서 에셋을 검색한다.

        Args:
            query: 검색 쿼리 문자열 (에셋 이름 부분 일치).
                   예: "Rock", "M_Ground", "T_"
            asset_class_filter: 에셋 클래스 필터.
                   예: "Material", "StaticMesh", "Texture2D", "Blueprint".
                   빈 문자열이면 전체 클래스 검색.
            search_path: 검색 루트 경로.
                   예: "/Game", "/Game/Materials", "/Engine".
            limit: 페이지 크기. 기본 50. 0=무제한(비권장).
            offset: 페이지 시작 인덱스. 기본 0.

        Returns:
            JSON with `assets` array. 페이지네이션 시 `total_count`, `returned`, `next_offset`,
            `has_more` 필드 추가.
        """
        command = {
            "type": "search_assets",
            "params": {
                "query": query.strip(),
                "asset_class_filter": asset_class_filter.strip(),
                "search_path": search_path.strip(),
                # Forward pagination so C++ doesn't ship the full match set over TCP.
                "limit": int(limit),
                "offset": int(offset),
            },
        }
        result = await send_command(command)

        if isinstance(result, dict) and result.get("success"):
            inner = result.get("result")
            if isinstance(inner, dict):
                assets = inner.get("assets")
                if isinstance(assets, list):
                    # Python fallback: trust C++ pagination if `total_count` is
                    # already populated; otherwise paginate locally for older plugins.
                    if "total_count" not in inner:
                        total = len(assets)
                        start = max(0, offset)
                        end = total if not limit else min(total, start + limit)
                        page = assets[start:end]
                        inner["assets"] = page
                        inner["total_count"] = total
                        inner["returned"] = len(page)
                        inner["next_offset"] = end if end < total else -1
                        inner["has_more"] = end < total

        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # get_asset_details
    # ------------------------------------------------------------------
    @server.tool("get_asset_details")
    async def get_asset_details(asset_path: str) -> str:
        """[Material] 에셋의 상세 정보를 반환한다.

        Args:
            asset_path: 에셋의 콘텐츠 브라우저 경로.
                        예: "/Game/Materials/M_Rock",
                            "/Game/Meshes/SM_Cube"

        Returns:
            JSON with asset metadata: name, asset_class, package_path,
            object_path, dependencies. 자세한 노드/속성 조회는 inspect_uobject 사용.
        """
        command = {
            "type": "get_asset_details",
            "params": {"asset_path": asset_path.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_material
    # ------------------------------------------------------------------
    @server.tool("create_material")
    async def create_material(
        name: str,
        save_path: str = "/Game/Materials",
        material_domain: str = "Surface",
        blend_mode: str = "Opaque",
    ) -> str:
        """[Material] 새 머티리얼 에셋을 생성한다.

        Args:
            name: 머티리얼 에셋 이름 (예: "M_Rock", "M_Glass").
            save_path: 저장 경로 (예: "/Game/Materials").
            material_domain: 머티리얼 도메인.
                             "Surface" | "Deferred Decal" | "Light Function" |
                             "PostProcess" | "UI" | "Volume"
            blend_mode: 블렌드 모드.
                        "Opaque" | "Masked" | "Translucent" |
                        "Additive" | "Modulate"
        """
        command = {
            "type": "create_material",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "material_domain": material_domain.strip(),
                "blend_mode": blend_mode.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_material_expression
    # ------------------------------------------------------------------
    @server.tool("add_material_expression")
    async def add_material_expression(
        material_name: str,
        expression_type: str,
        position_x: float = 0.0,
        position_y: float = 0.0,
        expression_params: str = "{}",
    ) -> str:
        """[Material] 머티리얼 그래프에 표현식(노드)을 추가한다.

        Args:
            material_name: 대상 머티리얼 에셋 이름.
            expression_type: 추가할 표현식 타입.
                             예: "Constant", "Constant3Vector", "Constant4Vector",
                                 "TextureSample", "TextureSampleParameter2D",
                                 "ScalarParameter", "VectorParameter",
                                 "Multiply", "Add", "Lerp", "Fresnel",
                                 "VertexNormalWS", "PixelNormalWS"
            position_x: 그래프 캔버스 X 좌표.
            position_y: 그래프 캔버스 Y 좌표.
            expression_params: 표현식별 추가 파라미터 (JSON 문자열).
                                예: '{"value": 0.5}' (Constant)
                                    '{"r":1.0,"g":0.0,"b":0.0}' (Constant3Vector)
                                    '{"parameter_name":"BaseColor"}' (VectorParameter)
        """
        try:
            params_dict = json.loads(expression_params)
        except json.JSONDecodeError:
            params_dict = {}

        command = {
            "type": "add_material_expression",
            "params": {
                "material_name": material_name.strip(),
                "expression_type": expression_type.strip(),
                "position_x": position_x,
                "position_y": position_y,
                "expression_params": params_dict,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # connect_material_nodes
    # ------------------------------------------------------------------
    @server.tool("connect_material_nodes")
    async def connect_material_nodes(
        material_name: str,
        source_expression_id: str,
        source_output_name: str,
        target_expression_id: str,
        target_input_name: str,
    ) -> str:
        """[Material] 머티리얼 그래프에서 두 노드를 연결한다.

        Args:
            material_name: 대상 머티리얼 에셋 이름.
            source_expression_id: 소스 표현식 ID (add_material_expression 반환값).
                                  "__result__" 을 사용하면 머티리얼 결과 노드를 가리킨다.
            source_output_name: 소스 출력 핀 이름.
                                예: "RGB", "R", "G", "B", "A", ""(기본 출력)
            target_expression_id: 타깃 표현식 ID.
                                  "__result__" 을 사용하면 머티리얼 결과 노드를 가리킨다.
            target_input_name: 타깃 입력 핀 이름.
                               예: "BaseColor", "Metallic", "Roughness",
                                   "Normal", "EmissiveColor", "Opacity",
                                   "A", "B" (Multiply/Add 등의 입력)
        """
        command = {
            "type": "connect_material_nodes",
            "params": {
                "material_name": material_name.strip(),
                "source_expression_id": source_expression_id.strip(),
                "source_output_name": source_output_name.strip(),
                "target_expression_id": target_expression_id.strip(),
                "target_input_name": target_input_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # apply_material_to_actor
    # ------------------------------------------------------------------
    @server.tool("apply_material_to_actor")
    async def apply_material_to_actor(
        actor_name: str,
        material_path: str,
        slot_index: int = 0,
    ) -> str:
        """[Material] 액터의 StaticMeshComponent에 머티리얼을 적용한다.

        Args:
            actor_name: 대상 액터 이름.
            material_path: 적용할 머티리얼 에셋 경로.
                           예: "/Game/Materials/M_Rock",
                               "/Engine/BasicShapes/BasicShapeMaterial"
            slot_index: 머티리얼 슬롯 인덱스 (기본값 0).
        """
        command = {
            "type": "apply_material_to_actor",
            "params": {
                "actor_name": actor_name.strip(),
                "material_path": material_path.strip(),
                "slot_index": slot_index,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # set_material_parameter
    # ------------------------------------------------------------------
    @server.tool("set_material_parameter")
    async def set_material_parameter(
        material_name: str,
        parameter_name: str,
        parameter_type: str,
        value: str,
    ) -> str:
        """[Material] 머티리얼의 파라미터 기본값을 설정한다.

        Args:
            material_name: 대상 머티리얼 에셋 이름.
            parameter_name: 파라미터 이름 (VectorParameter/ScalarParameter 이름).
            parameter_type: 파라미터 타입. "Scalar" | "Vector"
            value: 파라미터 값 (JSON 문자열).
                   Scalar 예: "0.5"
                   Vector 예: '{"r":1.0,"g":0.5,"b":0.0,"a":1.0}'
        """
        try:
            parsed_value = json.loads(value)
        except json.JSONDecodeError:
            parsed_value = value

        command = {
            "type": "set_material_parameter",
            "params": {
                "material_name": material_name.strip(),
                "parameter_name": parameter_name.strip(),
                "parameter_type": parameter_type.strip(),
                "value": parsed_value,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # import_asset
    # ------------------------------------------------------------------
    @server.tool("import_asset")
    async def import_asset(
        source_file_path: str,
        destination_path: str = "/Game/Imports",
        asset_name: str = "",
    ) -> str:
        """[Material] 외부 파일을 UE5 에셋으로 임포트한다.

        Args:
            source_file_path: 임포트할 파일의 절대 경로.
                              예: "C:/Assets/rock.fbx",
                                  "C:/Textures/stone_diffuse.png"
            destination_path: 콘텐츠 브라우저 저장 경로.
                              예: "/Game/Meshes", "/Game/Textures"
            asset_name: 임포트 후 에셋 이름. 비어있으면 파일명 사용.
        """
        command = {
            "type": "import_asset",
            "params": {
                "source_file_path": source_file_path.strip(),
                "destination_path": destination_path.strip(),
                "asset_name": asset_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # duplicate_asset
    # ------------------------------------------------------------------
    @server.tool("duplicate_asset")
    async def duplicate_asset(
        source_asset_path: str,
        new_name: str,
        destination_path: str = "",
    ) -> str:
        """[Material] 에셋을 복제한다.

        Args:
            source_asset_path: 복제할 원본 에셋 경로.
                               예: "/Game/Materials/M_Rock"
            new_name: 복제본 이름.
            destination_path: 복제본 저장 경로. 비어있으면 원본과 동일 경로.
        """
        command = {
            "type": "duplicate_asset",
            "params": {
                "source_asset_path": source_asset_path.strip(),
                "new_name": new_name.strip(),
                "destination_path": destination_path.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # delete_asset
    # ------------------------------------------------------------------
    @server.tool("delete_asset")
    async def delete_asset(asset_path: str, force_delete: bool = False) -> str:
        """[Material] 에셋을 삭제한다.

        Args:
            asset_path: 삭제할 에셋의 콘텐츠 브라우저 경로.
                        예: "/Game/Materials/M_OldRock"
            force_delete: True이면 다른 에셋에서 참조 중이어도 강제 삭제.
                          False이면 참조 중인 경우 오류 반환.
        """
        command = {
            "type": "delete_asset",
            "params": {
                "asset_path": asset_path.strip(),
                "force_delete": force_delete,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
