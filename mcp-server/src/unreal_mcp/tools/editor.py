"""Phase 5 -- Editor Automation tools.

MCP tools for automating the UE5 editor:
PIE control, viewport camera, console commands, screenshots,
actor selection, and level save/load.
"""

import json
from mcp.server.fastmcp import FastMCP as Server
from ..connection import send_command


def register_editor_tools(server: Server) -> None:
    """Register Editor Automation tools to MCP server."""

    # ------------------------------------------------------------------
    # play_in_editor
    # ------------------------------------------------------------------
    @server.tool("play_in_editor")
    async def play_in_editor(
        action: str = "play",
        mode: str = "viewport",
    ) -> str:
        """에디터 내 플레이(PIE) 세션을 제어한다.

        Args:
            action: 수행할 작업.
                    "play"   -- PIE 시작
                    "pause"  -- PIE 일시정지 (토글)
                    "stop"   -- PIE 중지
                    "resume" -- PIE 재개
            mode: PIE 모드 (action이 "play"일 때만 적용).
                  "viewport"           -- 활성 뷰포트에서 플레이 (기본값)
                  "mobile_preview"     -- 모바일 프리뷰
                  "new_editor_window"  -- 새 에디터 창에서 플레이
                  "standalone"         -- 독립 실행형
        """
        command = {
            "type": "play_in_editor",
            "params": {
                "action": action.strip().lower(),
                "mode": mode.strip().lower(),
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # set_viewport_camera
    # ------------------------------------------------------------------
    @server.tool("set_viewport_camera")
    async def set_viewport_camera(
        location: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
        viewport_index: int = 0,
    ) -> str:
        """에디터 뷰포트 카메라 위치와 회전을 설정한다.

        Args:
            location: 카메라 월드 위치 (X, Y, Z) -- 단위: cm.
            rotation: 카메라 회전 (Pitch, Yaw, Roll) -- 단위: 도.
            viewport_index: 대상 뷰포트 인덱스 (기본값 0).
        """
        command = {
            "type": "set_viewport_camera",
            "params": {
                "location": list(location),
                "rotation": list(rotation),
                "viewport_index": viewport_index,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # run_console_command
    # ------------------------------------------------------------------
    @server.tool("run_console_command")
    async def run_console_command(command_string: str) -> str:
        """UE5 에디터 콘솔 명령을 실행한다.

        Args:
            command_string: 실행할 콘솔 명령 문자열.
                            예: "r.SetRes 1920x1080"
                                "stat fps"
                                "t.MaxFPS 60"
                                "ShowFlag.Wireframe 1"
                                "DumpConsoleCommands"
                                "r.ScreenPercentage 100"
        """
        if not command_string.strip():
            return json.dumps(
                {"success": False,
                 "error": {"code": "INVALID_PARAMS",
                            "message": "콘솔 명령이 비어있습니다."}},
                indent=2, ensure_ascii=False,
            )
        command = {
            "type": "run_console_command",
            "params": {"command_string": command_string.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # take_screenshot
    # ------------------------------------------------------------------
    @server.tool("take_screenshot")
    async def take_screenshot(
        file_name: str = "",
        width: int = 1920,
        height: int = 1080,
        show_ui: bool = False,
    ) -> str:
        """에디터 뷰포트 스크린샷을 캡처한다.

        Args:
            file_name: 저장 파일 이름 (확장자 없이).
                       비어있으면 타임스탬프 기반 이름 자동 생성.
                       저장 경로: Saved/Screenshots/Editor/
            width: 스크린샷 가로 픽셀 (기본값 1920).
            height: 스크린샷 세로 픽셀 (기본값 1080).
            show_ui: True이면 에디터 UI 포함 캡처.
        """
        command = {
            "type": "take_screenshot",
            "params": {
                "file_name": file_name.strip(),
                "width": width,
                "height": height,
                "show_ui": show_ui,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # get_selected_actors
    # ------------------------------------------------------------------
    @server.tool("get_selected_actors")
    async def get_selected_actors() -> str:
        """에디터에서 현재 선택된 액터 목록을 반환한다.

        Returns:
            선택된 액터의 이름, 클래스, 트랜스폼 정보 목록.
        """
        command = {
            "type": "get_selected_actors",
            "params": {},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # select_actors
    # ------------------------------------------------------------------
    @server.tool("select_actors")
    async def select_actors(
        actor_names: list[str],
        append_to_selection: bool = False,
    ) -> str:
        """에디터에서 지정한 액터를 선택한다.

        Args:
            actor_names: 선택할 액터 이름 목록.
                         예: ["SM_Rock_01", "PointLight_3"]
            append_to_selection: True이면 기존 선택에 추가.
                                  False이면 기존 선택을 해제하고 새로 선택.
        """
        if not actor_names:
            return json.dumps(
                {"success": False,
                 "error": {"code": "INVALID_PARAMS",
                            "message": "actor_names가 비어있습니다."}},
                indent=2, ensure_ascii=False,
            )
        command = {
            "type": "select_actors",
            "params": {
                "actor_names": [n.strip() for n in actor_names],
                "append_to_selection": append_to_selection,
            },
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # save_level
    # ------------------------------------------------------------------
    @server.tool("save_level")
    async def save_level(level_path: str = "") -> str:
        """현재 레벨 (또는 지정 레벨)을 저장한다.

        Args:
            level_path: 저장할 레벨 경로.
                        비어있으면 현재 활성 레벨 저장.
                        예: "/Game/Maps/MainLevel"
        """
        command = {
            "type": "save_level",
            "params": {"level_path": level_path.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)

    # ------------------------------------------------------------------
    # load_level
    # ------------------------------------------------------------------
    @server.tool("load_level")
    async def load_level(level_path: str) -> str:
        """지정 경로의 레벨을 에디터에서 연다.

        Args:
            level_path: 열 레벨 에셋 경로.
                        예: "/Game/Maps/MainLevel",
                            "/Game/Maps/TestLevel"
        """
        if not level_path.strip():
            return json.dumps(
                {"success": False,
                 "error": {"code": "INVALID_PARAMS",
                            "message": "level_path가 비어있습니다."}},
                indent=2, ensure_ascii=False,
            )
        command = {
            "type": "load_level",
            "params": {"level_path": level_path.strip()},
        }
        result = await send_command(command)
        return json.dumps(result, indent=2, ensure_ascii=False)
