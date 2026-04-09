"""Unreal Engine MCP server entry point.

Claude Desktop --(stdio)--> This server --(TCP:13377)--> UE5 C++ plugin
"""

import asyncio
import logging
from mcp.server import Server
from mcp.server.stdio import stdio_server

from .tools.actor import register_actor_tools
from .tools.blueprint import register_blueprint_tools

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# MCP server instance creation
server = Server("unreal-mcp")

# Phase 1: Register Actor & Scene tools
register_actor_tools(server)

# Phase 2: Register Blueprint editing tools
register_blueprint_tools(server)


async def _run() -> None:
    """Run MCP server in stdio mode."""
    logger.info("Unreal MCP server starting (Phase 1: Actor & Scene | Phase 2: Blueprint)")
    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, server.create_initialization_options())


def main() -> None:
    """CLI entry point."""
    asyncio.run(_run())


if __name__ == "__main__":
    main()
