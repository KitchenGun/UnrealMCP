"""Phase 2 -- Blueprint editing tools.

MCP tools to create/edit/compile/spawn UE5 Blueprint assets.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command
from ..utils.validators import validate_actor_name


def register_blueprint_tools(server: Server) -> None:
    """Register Blueprint-related tools to MCP server."""

    # ------------------------------------------------------------------
    # create_blueprint
    # ------------------------------------------------------------------
    @server.tool("create_blueprint")
    async def create_blueprint(
        name: str,
        parent_class: str = "Actor",
        save_path: str = "/Game/Blueprints",
    ) -> str:
        """Create a new Blueprint asset.

        Args:
            name: Blueprint asset name (Example: "BP_MyCharacter").
            parent_class: Parent class name.
                Example: "Actor", "Character", "Pawn",
                "ActorComponent", "SceneComponent".
            save_path: Content browser save path.
                Examples: "/Game/Blueprints", "/Game/Characters".
        """
        command = {
            "type": "create_blueprint",
            "params": {
                "name": name.strip(),
                "parent_class": parent_class.strip(),
                "save_path": save_path.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_blueprint_node
    # ------------------------------------------------------------------
    @server.tool("add_blueprint_node")
    async def add_blueprint_node(
        blueprint_name: str,
        node_type: str,
        graph_name: str = "EventGraph",
        position_x: float = 0.0,
        position_y: float = 0.0,
        node_params: str = "{}",
    ) -> str:
        """Add node to Blueprint graph.

        Args:
            blueprint_name: Target Blueprint asset name.
            node_type: Node type to add.
                Example: "Event_BeginPlay", "Event_Tick",
                "CallFunction", "VariableGet", "VariableSet",
                "Branch", "Sequence", "ForEachLoop",
                "PrintString", "SpawnActor".
            graph_name: Name of graph to add node to.
                Usually "EventGraph". Use function name for function graphs.
            position_x: Graph canvas X coordinate (pixels).
            position_y: Graph canvas Y coordinate (pixels).
            node_params: Additional parameters per node (JSON string).
                Example: '{"function_name": "PrintString", "target": "self"}'
        """
        try:
            params_dict = json.loads(node_params)
        except json.JSONDecodeError:
            params_dict = {}

        command = {
            "type": "add_blueprint_node",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "node_type": node_type.strip(),
                "graph_name": graph_name.strip(),
                "position_x": position_x,
                "position_y": position_y,
                "node_params": params_dict,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # connect_blueprint_pins
    # ------------------------------------------------------------------
    @server.tool("connect_blueprint_pins")
    async def connect_blueprint_pins(
        blueprint_name: str,
        source_node_id: str,
        source_pin_name: str,
        target_node_id: str,
        target_pin_name: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Blueprint 핀 연결.

        Args:
            blueprint_name: Target Blueprint asset name.
            source_node_id: 소스 노드 ID (add_blueprint_node 반환값).
            source_pin_name: 소스 핀 이름.
                Example: "exec" (실행 핀), "ReturnValue", "변수 이름".
            target_node_id: 타겟 노드 ID.
            target_pin_name: 타겟 핀 이름.
                Example: "execute" (실행 핀), "InString".
            graph_name: 그래프 이름. default value: "EventGraph".
        """
        command = {
            "type": "connect_blueprint_pins",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "source_node_id": source_node_id.strip(),
                "source_pin_name": source_pin_name.strip(),
                "target_node_id": target_node_id.strip(),
                "target_pin_name": target_pin_name.strip(),
                "graph_name": graph_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # remove_blueprint_node
    # ------------------------------------------------------------------
    @server.tool("remove_blueprint_node")
    async def remove_blueprint_node(
        blueprint_name: str,
        node_id: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Blueprint 노드 삭제.

        Args:
            blueprint_name: Target Blueprint asset name.
            node_id: 삭제할 노드 ID (add_blueprint_node 반환값).
            graph_name: 그래프 이름. default value: "EventGraph".
        """
        command = {
            "type": "remove_blueprint_node",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "node_id": node_id.strip(),
                "graph_name": graph_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_blueprint_variable
    # ------------------------------------------------------------------
    @server.tool("add_blueprint_variable")
    async def add_blueprint_variable(
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False,
        default_value: str = "",
    ) -> str:
        """Blueprint 변수 추가.

        Args:
            blueprint_name: Target Blueprint asset name.
            variable_name: 변수 이름.
            variable_type: 변수 타입.
                기본형: "Boolean", "Integer", "Float",
                "String", "Name", "Text", "Vector",
                "Rotator", "Transform", "Object".
                오브젝트형: "Actor", "StaticMeshComponent" 등.
            is_exposed: 이벤트 그래프에 노출 여부.
            default_value: 기본값 (JSON 형식).
                Example: "true", "42", '"Hello"', '{"X":0,"Y":0,"Z":100}'.
        """
        try:
            parsed_default = json.loads(default_value) if default_value.strip() else None
        except json.JSONDecodeError:
            parsed_default = default_value if default_value.strip() else None

        command = {
            "type": "add_blueprint_variable",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "variable_name": variable_name.strip(),
                "variable_type": variable_type.strip(),
                "is_exposed": is_exposed,
                "default_value": parsed_default,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # compile_blueprint
    # ------------------------------------------------------------------
    @server.tool("compile_blueprint")
    async def compile_blueprint(blueprint_name: str) -> str:
        """Compile Blueprint.

        Args:
            blueprint_name: Blueprint asset name to compile.
        """
        command = {
            "type": "compile_blueprint",
            "params": {"blueprint_name": blueprint_name.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # get_blueprint_graph
    # ------------------------------------------------------------------
    @server.tool("get_blueprint_graph")
    async def get_blueprint_graph(
        blueprint_name: str,
        graph_name: str = "EventGraph",
    ) -> str:
        """Blueprint 그래프 노드 목록 조회.

        Args:
            blueprint_name: Target Blueprint asset name.
            graph_name: 그래프 이름. default value: "EventGraph".
                function 그래프는 function 이름 사용.
        """
        command = {
            "type": "get_blueprint_graph",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "graph_name": graph_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_blueprint_component
    # ------------------------------------------------------------------
    @server.tool("add_blueprint_component")
    async def add_blueprint_component(
        blueprint_name: str,
        component_class: str,
        component_name: str = "",
        attach_to: str = "",
    ) -> str:
        """Blueprint 컴포넌트 추가.

        Args:
            blueprint_name: Target Blueprint asset name.
            component_class: 컴포넌트 클래스 이름.
                Example: "StaticMeshComponent", "BoxComponent",
                "CapsuleComponent", "PointLightComponent",
                "AudioComponent", "ParticleSystemComponent".
            component_name: 컴포넌트 인스턴스 이름.
                생략 시 클래스 이름 사용.
            attach_to: 부모 컴포넌트 이름.
                생략 시 루트에 부착.
        """
        command = {
            "type": "add_blueprint_component",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "component_class": component_class.strip(),
                "component_name": component_name.strip(),
                "attach_to": attach_to.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # spawn_blueprint_actor
    # ------------------------------------------------------------------
    @server.tool("spawn_blueprint_actor")
    async def spawn_blueprint_actor(
        blueprint_name: str,
        actor_name: str = "",
        location: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
        scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    ) -> str:
        """Blueprint asset 스폰.

        Args:
            blueprint_name: Blueprint asset name.
            actor_name: 액터 이름.
                생략 시 Blueprint 이름 사용.
            location: (X, Y, Z) -- 단위: cm.
            rotation: (Pitch, Yaw, Roll) -- 단위: 도(deg).
            scale: (X, Y, Z). default value (1, 1, 1).
        """
        command = {
            "type": "spawn_blueprint_actor",
            "params": {
                "blueprint_name": blueprint_name.strip(),
                "actor_name": actor_name.strip(),
                "location": list(location),
                "rotation": list(rotation),
                "scale": list(scale),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
