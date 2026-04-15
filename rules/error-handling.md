# error-handling -- Error Code System and Message Principles

## Error Code System

| Code | Meaning | Action |
|------|---------|--------|
| `CONNECTION_FAILED` | UE5 plugin connection failed | Verify editor is running and plugin is enabled |
| `CONNECTION_TIMEOUT` | Command response timeout | Check task complexity, consider extending timeout |
| `INVALID_PARAMS` | Parameter format/value error | Provide examples of correct parameter format |
| `ACTOR_NOT_FOUND` | Specified actor not found | Verify actor name, suggest using get_actors_in_level |
| `ASSET_NOT_FOUND` | Specified asset not found | Verify asset path, suggest using search_assets |
| `BLUEPRINT_COMPILE_ERROR` | Blueprint compilation failed | Provide detailed compilation error message |
| `PERMISSION_DENIED` | Editor state prevents operation | Guide state transition (stop PIE, etc) |
| `INTERNAL_ERROR` | UE API internal error | Suggest checking logs, recommend retry |

## User-Friendly Error Message Principles

1. **Clearly describe** what went wrong
2. **Suggest possible solutions**
3. **For editor state-related errors**, guide state transition (if in PIE, etc)

## Error Response Example
```json
{
    "success": false,
    "error": {
        "code": "ACTOR_NOT_FOUND",
        "message": "Actor 'BP_Player' not found. Use get_actors_in_level to view actors in current level."
    }
}
```
