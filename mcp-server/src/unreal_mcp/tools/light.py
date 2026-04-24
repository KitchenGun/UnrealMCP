"""Light component control tools.

MCP tools to read/write light component properties on DirectionalLight,
PointLight, SpotLight, RectLight actors in UE5 levels.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command
from ..utils.validators import validate_actor_name


def register_light_tools(server: Server) -> None:
    """Register light component control tools to MCP server."""

    # --------------------------------------------------------------
    # set_light_property
    # --------------------------------------------------------------
    @server.tool("set_light_property")
    async def set_light_property(
        name: str,
        property_name: str,
        value: str,
    ) -> str:
        """Set a light component property on a light actor.

        Args:
            name: Light actor name (e.g. "DirectionalLight", "PointLight_0").
            property_name: Property to set. Supported values:
                - "intensity"           float  — light brightness (모든 라이트)
                - "light_color"         str    — JSON array "[r, g, b]", 0.0~1.0 (모든 라이트)
                - "cast_shadows"        str    — "true" or "false" (모든 라이트)
                - "temperature"         float  — color temperature in Kelvin, 1700~12000 (모든 라이트)
                - "use_temperature"     str    — "true" or "false" (모든 라이트)
                - "attenuation_radius"  float  — falloff radius in cm (PointLight / SpotLight)
                - "inner_cone_angle"    float  — degrees (SpotLight)
                - "outer_cone_angle"    float  — degrees (SpotLight)
            value: Property value as a JSON string.
                   Examples: "0", "3.14", "true", "[1.0, 0.5, 0.0]"
        """
        prop = property_name.strip().lower()

        try:
            parsed = json.loads(value)
        except json.JSONDecodeError:
            parsed = value

        params: dict = {
            "name": validate_actor_name(name),
            "property_name": prop,
        }

        # light_color는 배열, bool은 bool, 나머지는 숫자로 전달
        if prop == "light_color":
            if not isinstance(parsed, list) or len(parsed) < 3:
                raise ValueError("light_color는 [r, g, b] 형식이어야 합니다. 예: \"[1.0, 0.5, 0.0]\"")
            params["value"] = [float(parsed[0]), float(parsed[1]), float(parsed[2])]
        elif prop in ("cast_shadows", "use_temperature"):
            if isinstance(parsed, bool):
                params["value"] = parsed
            else:
                params["value"] = str(parsed).lower() in ("true", "1", "yes")
        else:
            params["value"] = float(parsed)

        command = {"type": "set_light_property", "params": params}
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # --------------------------------------------------------------
    # get_light_property
    # --------------------------------------------------------------
    @server.tool("get_light_property")
    async def get_light_property(name: str) -> str:
        """Get all light component properties from a light actor.

        Args:
            name: Light actor name (e.g. "DirectionalLight", "PointLight_0").

        Returns:
            JSON with intensity, light_color, cast_shadows, temperature,
            use_temperature, and (if applicable) attenuation_radius,
            inner_cone_angle, outer_cone_angle.
        """
        command = {
            "type": "get_light_property",
            "params": {"name": validate_actor_name(name)},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
