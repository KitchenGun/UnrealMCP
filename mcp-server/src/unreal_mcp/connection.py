"""UE5 플러그인과의 TCP 통신 모듈.

TCP:13377 포트로 연결하여 newline-delimited JSON 명령을 송수신한다.
재연결 최대 10회, 간격 3초. 기본 타임아웃 30초.
"""

import asyncio
import json
import logging
import uuid
from typing import Any

logger = logging.getLogger(__name__)

# 연결 설정 상수
HOST = "127.0.0.1"
PORT = 13377
RECONNECT_INTERVAL = 3.0   # 초
MAX_RECONNECT_ATTEMPTS = 10
DEFAULT_TIMEOUT = 30.0      # 초
HEAVY_TIMEOUT = 60.0        # Blueprint 컴파일 등 무거운 작업용

# 전역 연결 상태
_reader: asyncio.StreamReader | None = None
_writer: asyncio.StreamWriter | None = None
_lock = asyncio.Lock()


async def _ensure_connected() -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
    """연결이 없으면 UE5 플러그인에 재연결을 시도한다."""
    global _reader, _writer

    if _reader is not None and _writer is not None and not _writer.is_closing():
        return _reader, _writer

    last_error: Exception | None = None
    for attempt in range(1, MAX_RECONNECT_ATTEMPTS + 1):
        try:
            logger.info("UE5 플러그인 연결 시도 %d/%d ...", attempt, MAX_RECONNECT_ATTEMPTS)
            _reader, _writer = await asyncio.open_connection(HOST, PORT)
            logger.info("UE5 플러그인 연결 성공 (%s:%d)", HOST, PORT)
            return _reader, _writer
        except OSError as e:
            last_error = e
            logger.warning("연결 실패 (%d회차): %s", attempt, e)
            if attempt < MAX_RECONNECT_ATTEMPTS:
                await asyncio.sleep(RECONNECT_INTERVAL)

    raise ConnectionError(
        f"UE5 플러그인({HOST}:{PORT})에 연결할 수 없습니다. "
        "UE5 에디터가 실행 중이고 UnrealMCP 플러그인이 활성화되어 있는지 확인하세요. "
        f"마지막 오류: {last_error}"
    )


def _make_command(command_type: str, params: dict[str, Any]) -> dict[str, Any]:
    """UUID를 포함한 명령 객체를 생성한다."""
    return {
        "id": str(uuid.uuid4()),
        "type": command_type,
        "params": params,
    }


async def send_command(
    command: dict[str, Any],
    timeout: float = DEFAULT_TIMEOUT,
) -> dict[str, Any]:
    """UE5 플러그인에 명령을 전송하고 응답을 반환한다.

    Args:
        command: {"type": "...", "params": {...}} 형식의 명령 딕셔너리.
                 id 필드가 없으면 자동으로 추가된다.
        timeout: 응답 대기 타임아웃(초). 기본 30초.

    Returns:
        UE5 플러그인의 응답 딕셔너리.

    Raises:
        ConnectionError: UE5 플러그인 연결 실패.
        TimeoutError: 타임아웃 초과.
        ValueError: JSON 파싱 오류.
    """
    # id 필드 자동 추가
    if "id" not in command:
        command = {**command, "id": str(uuid.uuid4())}

    async with _lock:
        reader, writer = await _ensure_connected()

        # 명령 전송 (newline 구분자)
        payload = json.dumps(command, ensure_ascii=False) + "\n"
        try:
            writer.write(payload.encode("utf-8"))
            await writer.drain()
        except OSError as e:
            _reset_connection()
            raise ConnectionError(f"명령 전송 실패: {e}") from e

        # 응답 수신
        try:
            raw = await asyncio.wait_for(reader.readline(), timeout=timeout)
        except asyncio.TimeoutError as e:
            _reset_connection()
            raise TimeoutError(
                f"UE5 플러그인 응답 타임아웃({timeout}초). "
                "작업이 복잡한 경우 타임아웃을 늘려보세요."
            ) from e
        except OSError as e:
            _reset_connection()
            raise ConnectionError(f"응답 수신 실패: {e}") from e

        if not raw:
            _reset_connection()
            raise ConnectionError("UE5 플러그인이 연결을 닫았습니다.")

        # JSON 파싱
        try:
            response = json.loads(raw.decode("utf-8").strip())
        except json.JSONDecodeError as e:
            raise ValueError(f"UE5 응답 JSON 파싱 실패: {e}\n원본: {raw!r}") from e

        return response


def _reset_connection() -> None:
    """소켓 연결 상태를 초기화한다."""
    global _reader, _writer
    if _writer is not None:
        try:
            _writer.close()
        except Exception:
            pass
    _reader = None
    _writer = None
