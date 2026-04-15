"""TCP communication module with UE5 plugin.

Connects via TCP:13377 to send/receive newline-delimited JSON commands.
Max reconnect attempts: 10, interval: 3 seconds. Default timeout: 30 seconds.
"""

import asyncio
import json
import logging
import uuid
from typing import Any

logger = logging.getLogger(__name__)

# Connection configuration constants
HOST = "127.0.0.1"
PORT = 13377
RECONNECT_INTERVAL = 3.0   # seconds
MAX_RECONNECT_ATTEMPTS = 10
DEFAULT_TIMEOUT = 30.0      # seconds
HEAVY_TIMEOUT = 60.0        # For heavy operations like Blueprint compilation

# Global connection state
_reader: asyncio.StreamReader | None = None
_writer: asyncio.StreamWriter | None = None
_lock = asyncio.Lock()


async def _ensure_connected() -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
    """Reconnect to UE5 plugin if connection is lost."""
    global _reader, _writer

    if _reader is not None and _writer is not None and not _writer.is_closing():
        return _reader, _writer

    last_error: Exception | None = None
    for attempt in range(1, MAX_RECONNECT_ATTEMPTS + 1):
        try:
            logger.info("Attempting to connect to UE5 plugin %d/%d ...", attempt, MAX_RECONNECT_ATTEMPTS)
            _reader, _writer = await asyncio.open_connection(HOST, PORT)
            logger.info("Successfully connected to UE5 plugin (%s:%d)", HOST, PORT)
            return _reader, _writer
        except OSError as e:
            last_error = e
            logger.warning("Connection failed (attempt %d): %s", attempt, e)
            if attempt < MAX_RECONNECT_ATTEMPTS:
                await asyncio.sleep(RECONNECT_INTERVAL)

    raise ConnectionError(
        f"Cannot connect to UE5 plugin ({HOST}:{PORT}). "
        "Verify that UE5 editor is running and UnrealMCP plugin is enabled. "
        f"Last error: {last_error}"
    )


def _make_command(command_type: str, params: dict[str, Any]) -> dict[str, Any]:
    """Create a command object with UUID."""
    return {
        "id": str(uuid.uuid4()),
        "type": command_type,
        "params": params,
    }


async def send_command(
    command: dict[str, Any],
    timeout: float = DEFAULT_TIMEOUT,
) -> dict[str, Any]:
    """Send command to UE5 plugin and return response.

    Args:
        command: Command dict in format {"type": "...", "params": {...}}.
                 If id field is missing, it will be added automatically.
        timeout: Timeout in seconds for response. Default 30 seconds.

    Returns:
        Response dict from UE5 plugin.

    Raises:
        ConnectionError: Failed to connect to UE5 plugin.
        TimeoutError: Response timeout exceeded.
        ValueError: JSON parsing error.
    """
    # Automatically add id field if missing
    if "id" not in command:
        command = {**command, "id": str(uuid.uuid4())}

    async with _lock:
        reader, writer = await _ensure_connected()

        # Send command (newline delimiter)
        payload = json.dumps(command, ensure_ascii=False) + "\n"
        try:
            writer.write(payload.encode("utf-8"))
            await writer.drain()
        except OSError as e:
            _reset_connection()
            raise ConnectionError(f"Failed to send command: {e}") from e

        # Receive response
        try:
            raw = await asyncio.wait_for(reader.readline(), timeout=timeout)
        except asyncio.TimeoutError as e:
            _reset_connection()
            raise TimeoutError(
                f"UE5 plugin response timeout ({timeout}s). "
                "Try increasing timeout for complex operations."
            ) from e
        except OSError as e:
            _reset_connection()
            raise ConnectionError(f"Failed to receive response: {e}") from e

        if not raw:
            _reset_connection()
            raise ConnectionError("UE5 plugin closed connection.")

        # JSON parsing
        try:
            response = json.loads(raw.decode("utf-8").strip())
        except json.JSONDecodeError as e:
            raise ValueError(f"Failed to parse UE5 response JSON: {e}\nRaw: {raw!r}") from e

        return response


def _reset_connection() -> None:
    """Reset socket connection state."""
    global _reader, _writer
    if _writer is not None:
        try:
            _writer.close()
        except Exception:
            pass
    _reader = None
    _writer = None
