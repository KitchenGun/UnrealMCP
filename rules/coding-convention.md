# coding-convention -- Python/C++ Coding Conventions

## Python (MCP Server)

### Tool Definition Pattern
```python
@server.tool("tool_name")
async def tool_name(
    param1: str,
    param2: tuple[float, float, float] = (0, 0, 0),
) -> str:
    """English docstring -- Tool description.

    Args:
        param1: Parameter description
        param2: (X, Y, Z) coordinates
    """
    command = {
        "type": "tool_name",
        "params": { "param1": param1, "param2": list(param2) }
    }
    result = await send_command(command)
    return json.dumps(result, indent=2)
```

### Requirements
- All Tool functions must have **type hints + docstring**
- Parameter names must match UE terminology (location, rotation, scale)
- Response must always return **JSON string**
- Use **async/await** asynchronous pattern
- Return errors in MCP standard error format

### File Structure
```
mcp-server/src/unreal_mcp/
+---- main.py            # MCP server entry point
+---- connection.py      # TCP socket communication
+---- tools/             # Module separated by Phase
|   +---- actor.py       # Phase 1
|   +---- blueprint.py   # Phase 2
|   +---- material.py    # Phase 3
|   +---- asset.py       # Phase 3
|   +---- ai.py          # Phase 4
|   +---- editor.py      # Phase 5
|   +---- advanced.py    # Phase 6
+---- utils/
    +---- validators.py  # Parameter validation
```

---

## C++ (UE5 Plugin)

### Epic Coding Convention
| Prefix | Meaning | Example |
|--------|---------|---------|
| `A` | Actor | `AMyActor` |
| `U` | UObject | `UMyComponent` |
| `F` | Struct | `FMyStruct` |
| `E` | Enum | `EMyEnum` |
| `I` | Interface | `IMyInterface` |
| `b` | bool variable | `bIsValid` |

### Requirements
- All `UObject*` members must be marked with `UPROPERTY()`
- Use `IsValid()` (nullptr alone misses PendingKill)
- TCP listening on separate thread, UE API calls must be on GameThread
- Use `AsyncTask(ENamedThreads::GameThread, [=]() { ... })` pattern

### Command Handling Pattern
```cpp
void FUnrealMCPModule::HandleCommand(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Command;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Command)) { SendError("Invalid JSON"); return; }

    FString Type = Command->GetStringField("type");
    AsyncTask(ENamedThreads::GameThread, [this, Type, Command]()
    {
        if (Type == "create_actor") HandleCreateActor(Command->GetObjectField("params"));
        // ...
    });
}
```

### File Structure
```
Plugins/UnrealMCP/Source/UnrealMCP/
+---- Public/
|   +---- UnrealMCPModule.h
|   +---- MCPTcpServer.h
|   +---- Handlers/{Actor,Blueprint,Material,AI,Editor}Handler.h
+---- Private/
    +---- UnrealMCPModule.cpp
    +---- MCPTcpServer.cpp
    +---- Handlers/{Actor,Blueprint,Material,AI,Editor}Handler.cpp
```

---

## Common
- **Comments**: Code comments and docstrings in English for consistency
- **Commit messages**: `feat(actor):`, `fix(blueprint):`, `docs(setup):`, `refactor(connection):`, `test(material):`
