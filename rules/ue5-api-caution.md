# ue5-api-caution -- UE5 API Usage Cautions

## Blueprint Graph Programming
- K2Node types may vary by UE version -> Target engine version (5.5+) verification essential
- Always call `CompileBlueprint()` after adding nodes
- Event nodes (BeginPlay, Tick) can **only** be added to EventGraph
- Function Graph automatically generates FunctionEntry/FunctionResult nodes
- Pin names are **case-sensitive** (OutputPin, ReturnValue, etc)

## Actor Manipulation
- Clearly distinguish editor-only Tools vs Tools that work during PIE
- Access editor world: `GEditor->GetEditorWorldContext().World()`
- **Undo Support**: Use transaction system with `GEditor->BeginTransaction()` / `EndTransaction()`
- Call `Modify()` within transaction before changing properties

## Material Manipulation
- Always call `Material->PreEditChange()` / `PostEditChange()` after connecting Material Expressions
- Material Instance can **only override parameters from parent Material**
- Static Switch Parameters cannot be changed at runtime

## GameThread Rule (CRITICAL)
- TCP listening performed in separate thread
- **All UE API calls must run on GameThread**
- Pattern: `AsyncTask(ENamedThreads::GameThread, [=]() { /* UE API */ });`
- Calling UE API outside GameThread -> **Crash or undefined behavior**

## UE Version Support
- Target: Unreal Engine 5.5+
- API differences may exist per version, consider conditional compilation
- `#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5`

## References
- GenOrca/unreal-mcp: https://github.com/GenOrca/unreal-mcp
- flopperam Blueprint Guide: https://github.com/flopperam/unreal-engine-mcp/blob/main/Guides/blueprint-graph-guide.md
- UE5 API Reference: https://dev.epicgames.com/documentation
- MCP Official Spec: https://modelcontextprotocol.io
