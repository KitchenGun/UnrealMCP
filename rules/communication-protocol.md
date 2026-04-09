# communication-protocol -- TCP Communication Protocol Details

## Command Format (Python -> UE5)
```json
{
    "id": "uuid-v4",
    "type": "command_name",
    "params": { "key": "value" }
}
```

## Response Format (UE5 -> Python)
```json
{
    "id": "matching-uuid",
    "success": true,
    "result": { ... },
    "error": null
}
```

## Error Response
```json
{
    "id": "matching-uuid",
    "success": false,
    "result": null,
    "error": {
        "code": "ACTOR_NOT_FOUND",
        "message": "Actor 'BP_Player' not found"
    }
}
```

## TCP Communication Rules
| Item | Value |
|------|-------|
| Default port | 13377 |
| Message delimiter | `\n` (newline-delimited JSON) |
| Reconnect interval | 3 seconds, max 10 attempts |
| Default timeout | 30 seconds |
| Heavy operation timeout | 60 seconds (Blueprint compilation, etc) |
| Concurrent commands | Sequential processing (UE GameThread constraint) |

## Claude Desktop Configuration (claude_desktop_config.json)
```json
{
    "mcpServers": {
        "unreal-mcp": {
            "command": "uv",
            "args": [
                "--directory", "C:/Projects/unreal-mcp/mcp-server",
                "run", "src/unreal_mcp/main.py"
         