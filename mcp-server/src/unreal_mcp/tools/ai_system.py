"""Phase 4 -- AI System tools.

MCP tools for creating and configuring UE5 AI systems:
Behavior Tree, Blackboard, EQS, AIPerception, AIController.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command


def register_ai_tools(server: Server) -> None:
    """Register AI System related tools to MCP server."""

    # ------------------------------------------------------------------
    # create_behavior_tree
    # ------------------------------------------------------------------
    @server.tool("create_behavior_tree")
    async def create_behavior_tree(
        name: str,
        save_path: str = "/Game/AI",
        blackboard_name: str = "",
    ) -> str:
        """[AI] 새 Behavior Tree 에셋을 생성한다.

        Args:
            name: Behavior Tree 에셋 이름 (예: "BT_EnemyAI").
            save_path: 저장 경로 (예: "/Game/AI").
            blackboard_name: 연결할 Blackboard 에셋 이름.
                             비어있으면 연결하지 않음.
        """
        command = {
            "type": "create_behavior_tree",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "blackboard_name": blackboard_name.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_bt_node
    # ------------------------------------------------------------------
    @server.tool("add_bt_node")
    async def add_bt_node(
        behavior_tree_name: str,
        node_type: str,
        node_name: str = "",
        parent_node_id: str = "",
        node_params: str = "{}",
    ) -> str:
        """[AI] Behavior Tree에 노드를 추가한다.

        Args:
            behavior_tree_name: 대상 Behavior Tree 에셋 이름.
            node_type: 추가할 노드 타입.
                       Composite: "Selector" | "Sequence" | "SimpleParallel"
                       Task:      "BTTask_MoveTo" | "BTTask_Wait" |
                                  "BTTask_RunBehavior" | "BTTask_PlayAnimation" |
                                  "BTTask_BlackboardBase"
                       Decorator: "BTDecorator_Blackboard" | "BTDecorator_Loop" |
                                  "BTDecorator_TimeLimit" | "BTDecorator_IsAtLocation"
                       Service:   "BTService_BlackboardBase" | "BTService_DefaultFocus"
            node_name: 노드에 표시될 이름 (비어있으면 node_type 사용).
            parent_node_id: 부모 노드 ID (add_bt_node 반환값).
                            비어있으면 루트에 연결.
            node_params: 노드별 추가 파라미터 (JSON 문자열).
                         예: '{"wait_time": 2.0}' (BTTask_Wait)
                             '{"blackboard_key": "TargetActor"}' (Decorator)
        """
        try:
            params_dict = json.loads(node_params)
        except json.JSONDecodeError:
            params_dict = {}

        command = {
            "type": "add_bt_node",
            "params": {
                "behavior_tree_name": behavior_tree_name.strip(),
                "node_type": node_type.strip(),
                "node_name": node_name.strip(),
                "parent_node_id": parent_node_id.strip(),
                "node_params": params_dict,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_blackboard
    # ------------------------------------------------------------------
    @server.tool("create_blackboard")
    async def create_blackboard(
        name: str,
        save_path: str = "/Game/AI",
    ) -> str:
        """[AI] 새 Blackboard Data 에셋을 생성한다.

        Args:
            name: Blackboard 에셋 이름 (예: "BB_EnemyAI").
            save_path: 저장 경로 (예: "/Game/AI").
        """
        command = {
            "type": "create_blackboard",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # add_blackboard_key
    # ------------------------------------------------------------------
    @server.tool("add_blackboard_key")
    async def add_blackboard_key(
        blackboard_name: str,
        key_name: str,
        key_type: str,
        key_params: str = "{}",
    ) -> str:
        """[AI] Blackboard에 키를 추가한다.

        Args:
            blackboard_name: 대상 Blackboard 에셋 이름.
            key_name: 키 이름.
                      표준 키 예: "TargetActor", "LastKnownLocation",
                                  "NoiseLocation", "CurrentState", "DetectionType"
            key_type: 키 타입.
                      "Bool" | "Class" | "Enum" | "Float" | "Int" |
                      "Name" | "Object" | "Rotator" | "String" |
                      "Vector"
            key_params: 키별 추가 파라미터 (JSON 문자열).
                        Object 타입 예: '{"base_class": "Actor"}'
                        Enum 타입 예:  '{"enum_type": "/Script/MyGame.EAIState"}'
        """
        try:
            params_dict = json.loads(key_params)
        except json.JSONDecodeError:
            params_dict = {}

        command = {
            "type": "add_blackboard_key",
            "params": {
                "blackboard_name": blackboard_name.strip(),
                "key_name": key_name.strip(),
                "key_type": key_type.strip(),
                "key_params": params_dict,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_eqs_query
    # ------------------------------------------------------------------
    @server.tool("create_eqs_query")
    async def create_eqs_query(
        name: str,
        save_path: str = "/Game/AI",
        generator_type: str = "Donut",
    ) -> str:
        """[AI] 새 EQS(Environment Query System) 쿼리 에셋을 생성한다.

        Args:
            name: EQS 쿼리 에셋 이름 (예: "EQS_FindCover", "EQS_FindPatrolPoint").
            save_path: 저장 경로 (예: "/Game/AI").
            generator_type: 기본 생성기 타입.
                            "Donut"        -- 도넛 형태 위치 생성
                            "Circle"       -- 원형 위치 생성
                            "Grid"         -- 격자 위치 생성
                            "ActorsOfClass" -- 특정 클래스 액터 위치
                            "PathingGrid"  -- 내비게이션 격자
        """
        command = {
            "type": "create_eqs_query",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "generator_type": generator_type.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # setup_ai_perception
    # ------------------------------------------------------------------
    @server.tool("setup_ai_perception")
    async def setup_ai_perception(
        ai_controller_name: str,
        sight_radius: float = 1500.0,
        lose_sight_radius: float = 2000.0,
        peripheral_vision_angle: float = 60.0,
        hearing_range: float = 1000.0,
        dominant_sense: str = "Sight",
    ) -> str:
        """[AI] AIController Blueprint에 AIPerception 컴포넌트 설정을 추가한다.

        Args:
            ai_controller_name: 대상 AIController Blueprint 이름
                                (예: "BP_EnemyAIController").
            sight_radius: 시야 감지 거리 (cm). 기본값 1500.
            lose_sight_radius: 시야 소실 거리 (cm). 기본값 2000.
            peripheral_vision_angle: 주변 시야각 (도). 기본값 60.
            hearing_range: 청각 감지 거리 (cm). 기본값 1000.
            dominant_sense: 주요 감각.
                            "Sight" | "Hearing" | "Damage" | "Team" | "Touch"
        """
        command = {
            "type": "setup_ai_perception",
            "params": {
                "ai_controller_name": ai_controller_name.strip(),
                "sight_radius": sight_radius,
                "lose_sight_radius": lose_sight_radius,
                "peripheral_vision_angle": peripheral_vision_angle,
                "hearing_range": hearing_range,
                "dominant_sense": dominant_sense.strip(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # create_ai_controller
    # ------------------------------------------------------------------
    @server.tool("create_ai_controller")
    async def create_ai_controller(
        name: str,
        save_path: str = "/Game/AI",
        behavior_tree_name: str = "",
        enable_perception: bool = True,
    ) -> str:
        """[AI] 새 AIController Blueprint를 생성하고 기본 설정을 구성한다.

        RunBehaviorTree 노드와 AIPerception 컴포넌트를 자동으로 추가한다.

        Args:
            name: AIController Blueprint 이름 (예: "BP_EnemyAIController").
            save_path: 저장 경로 (예: "/Game/AI").
            behavior_tree_name: OnPossess 시 실행할 Behavior Tree 이름.
                                비어있으면 RunBehaviorTree 노드를 추가하지 않음.
            enable_perception: True이면 UAIPerceptionComponent를 자동 추가.
        """
        command = {
            "type": "create_ai_controller",
            "params": {
                "name": name.strip(),
                "save_path": save_path.strip(),
                "behavior_tree_name": behavior_tree_name.strip(),
                "enable_perception": enable_perception,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
