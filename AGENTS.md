# AGENTS.md - Unreal Engine MCP Project Context

## Goal
Build MCP server + C++ plugin to control UE5 editor via natural language.
Base: GenOrca/unreal-mcp (fork). Reference: flopperam/unreal-engine-mcp.

## Architecture
```
Claude Desktop --(stdio)--> Python MCP Server --(TCP:13377)--> UE5 C++ Plugin --> Editor
```
- **Python MCP Server** (`mcp-server/`): Tool definition, JSON serialization, TCP communication
- **UE5 C++ Plugin** (`Plugins/UnrealMCP/`): TCP listening, JSON->UE API, GameThread execution

## Development Phase
| Phase | Module | Core Tools |
|-------|--------|------------|
| 1 | Actor & Scene | create/delete/transform/find actor |
| 2 | Blueprint | create BP, add node, connect pins, compile |
| 3 | Material & Asset | create material, search/import asset |
| 4 | AI System | BehaviorTree, Blackboard, EQS, AIPerception |
| 5 | Editor Automation | PIE, viewport, console, screenshot |
| 6 | Advanced | Niagara, AnimBP, UMG, DataTable |

## Coding Rules
- **Python**: async/await, type hints + docstring required, JSON string return, `@server.tool()` decorator
- **C++**: Epic convention (A/U/F/E/I/b), `UPROPERTY()` required, `IsValid()` usage, GameThread for UE API
- **Communication**: newline-delimited JSON, reconnect 3s x 10, timeout 30s (heavy: 60s)
- **Korean comments**: Code comments and docstrings in Korean
- **Undo support**: `BeginTransaction()` / `EndTransaction()`
- See `rules/` for detailed standards

## Work Principles
1. Check Phase/Tool -> understand existing code -> incremental implementation (one at a time)
2. Each Tool must be independently testable (min: 1 success + 1 error case)
3. Commit format: `feat(actor):`, `fix(blueprint):`, `docs(setup):`

## Harness Rules
- Must pass `./scripts/lint.sh` before commit
- Failure -> auto-fix (max 3 retries) -> alert on continued failure
- Success is silent, failure is loud. This file stays **under 60 lines**

## Core Rule Documents (rules/)
- `rules/coding-convention.md` - Python/C++ detailed coding conventions
- `rules/tool-development.md` - MCP Tool development procedure and templates
- `rules/communication-protocol.md` - TCP communication protocol details
- `rules/error-handling.md` - Error code system and message principles
- `rules/ue5-api-caution.md` - UE5 API usage cautions

## Failure Log
<!-- Example: 2026-04-08 | UE API called outside GameThread -> crash | rules/ue5-api-caution.md added -->
