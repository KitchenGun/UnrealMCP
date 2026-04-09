"""Phase 1 -- Actor & Scene tools.

MCP tools to create/delete/transform/query/duplicate/read/write actor properties in UE5 levels.
"""

import json
from mcp.server import Server
from ..connection import send_command
from ..utils.validators import validate_actor_name, validate_vector3


def register_actor_tools(server: Server) -> None:
    """Register actor-related tools to MCP server."""

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
        """Create a new actor in UE5 level.

        Args:
            actor_class: Actor class name to create.
                         Examples: "StaticMeshActor", "PointLight", "SpotLight",
                                   "DirectionalLight", "CameraActor", "SkyLight",
                                   "RectLight", "ExponentialHeightFog"
            name: Name to assign to actor (UE auto-assigns if empty).
            location: World coordinates (X, Y, Z) -- Unit: cm.
            rotation: Rotation angles (Pitch, Yaw, Roll) -- Unit: degrees.
            scale: Scale (X, Y, Z). Default (1, 1, 1).
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
        """Delete actor with specified name from level.

        Args:
            name: Name of actor to delete (exact name or label).
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
        """Set actor location, rotation, and scale. Items that are None are not changed.

        Args:
            name: Target actor name.
            location: World coordinates (X, Y, Z) -- Unit: cm. Retained if None.
            rotation: Rotation (Pitch, Yaw, Roll) -- Unit: degrees. Retained if None.
            scale: Scale (X, Y, Z). Retained if None.
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
        """Return list of all actors in current level.

        Args:
            actor_class_filter: Class name to filter by. Example: "StaticMeshActor". Empty for all.
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
        """Search for actors in level by name pattern (partial match).

        Args:
            pattern: Name pattern to search. Case-insensitive.
                     Examples: "BP_", "Light", "Player"
        """
        if not pattern.strip():
            return json.dumps(
                {"success": False, "error": {"code": "INVALID_PARAMS", "message": "Search pattern is empty."}},
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
        """Duplicate actor.

        Args:
            name: Original actor name to duplicate.
            new_name: Name for duplicate. UE auto-assigns if empty.
            offset: Offset of duplicate position relative to original (X, Y, Z) -- Unit: cm.
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
        """Read actor properties (Transform, Tags, Hidden, etc).

        Args:
            name: Actor name to query.
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
        """Set specific actor property value.

        Args:
            name: Target actor name.
            property_name: Name of property to set.
                           Examples: "bHidden", "Tags", "CustomDepthStencilValue"
            property_value: Value to set (JSON string format).
                            Examples: "true", "42", '"MyTag"', '["Tag1","Tag2"]'
        """
        # Parse property_value as JSON string and convert to actual value
        try:
            parsed_value = json.loads(property_value)
        except json.JSONDecodeError:
            # If JSON parsing fails, pass string as-is
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
