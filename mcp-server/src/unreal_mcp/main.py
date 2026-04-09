"""Unreal Engine MCP 서버 진입점.

Claude Desktop --(stdio)--> 이 서버 --(TCP:13377)--> UE5 C++ 플러그인
"""

import asyncio
import logging
from mcp.server import Server
from mcp.server.stdio import stdio_server

from .tools.actor import register_actor_tools

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# MCP 서버 인스턴스 생성
server = Server("unreal-mcp")

# Phase 1: Actor & Scene Tool 등록
register_actor_tools(server)


async def _run() -> None:
    """MCP 서버를 stdio 모드로 실행한다."""
    logger.info("Unreal MCP 서버 시작 (Phase 1: Actor & Scene)")
    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, server.create_initialization_options())


def main() -> None:
    """CLI 진입점."""
    asyncio.run(_run())


if __name__ == "__main__":
    main()
