"""Phase 2 -- Blueprint editing tools.

MCP tools to create/edit/compile/spawn UE5 Blueprint assets.
"""

import json
from mcp.server import Server
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
 """Blueprint .

 Args:
 blueprint_name: Target Blueprint asset name.
 source_node_id: ID (add_blueprint_node ).
 source_pin_name: name.
 Example: "exec" ( ), "ReturnValue", " name".
 target_node_id: ID.
 target_pin_name: name.
 Example: "execute" ( ), "InString".
 graph_name: name. default value: "EventGraph".
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
 """Blueprint .

 Args:
 blueprint_name: Target Blueprint asset name.
 node_id: ID (add_blueprint_node ).
 graph_name: name. default value: "EventGraph".
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
 """Blueprint variable .

 Args:
 blueprint_name: Target Blueprint asset name.
 variable_name: variable name.
 variable_type: variable .
 : "Boolean", "Integer", "Float",
 "String", "Name", "Text", "Vector",
 "Rotator", "Transform", "Object".
 : "Actor", "StaticMeshComponent" .
 is_exposed: ( ) .
 default_value: default value (JSON ).
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

 .

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
 """Blueprint   .

 Args:
 blueprint_name: Target Blueprint asset name.
 graph_name: name. default value: "EventGraph".
 function function .
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
 """Blueprint .

 Args:
 blueprint_name: Target Blueprint asset name.
 component_class: name.
 Example: "StaticMeshComponent", "BoxComponent",
 "CapsuleComponent", "PointLightComponent",
 "AudioComponent", "ParticleSystemComponent".
 component_name: name.
 name .
 attach_to: name.
 .
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
 """Blueprint asset .

 Args:
 blueprint_name: Blueprint asset name.
 actor_name: name.
 Blueprint name .
 location: (X, Y, Z) -- : cm.
 rotation: (Pitch, Yaw, Roll) -- : (deg).
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
