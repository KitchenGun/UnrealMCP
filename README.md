# unreal-mcp-harness

> Unreal Engine MCP Project Development Harness

Harness for developing an MCP server + C++ plugin that controls the UE5 editor
via natural language from Claude Desktop.

---

## Folder Structure

```
unreal-mcp-harness/
+-- AGENTS.md                          <- Project context (under 60 lines, always applied)
+-- README.md                          <- This file
+-- hooks/
|   +-- pre-commit                     <- Git pre-commit hook
+-- scripts/
|   +-- setup.sh                       <- Initial setup script
|   +-- lint.sh                        <- Rule-based linter (with auto-fix)
|   +-- garbage-collect.sh             <- Dead code / doc mismatch detection
+-- rules/
    +-- coding-convention.md           <- Python/C++ coding conventions
    +-- tool-development.md            <- MCP Tool dev procedure + Phase tool list
    +-- communication-protocol.md      <- TCP communication protocol
    +-- error-handling.md              <- Error code system and message principles
    +-- ue5-api-caution.md             <- UE5 API usage cautions
    +-- no-print-debug.sh             <- Debug print auto-detection/removal
    +-- no-print-debug.md             <- no-print-debug rule document
```

---

## Apply to Another Project

### Method 1 - Copy folder and run setup

```bash
# 1. Copy this entire folder to target project
cp -r unreal-mcp-harness/ /path/to/your-project/

# 2. Move to target project root
cd /path/to/your-project/unreal-mcp-harness

# 3. Run setup script
bash scripts/setup.sh
```

### Method 2 - Extract files only

```bash
TARGET=/path/to/your-project

mkdir -p $TARGET/{scripts,hooks,rules}
cp scripts/*.sh $TARGET/scripts/
cp hooks/pre-commit $TARGET/hooks/
cp rules/*.md $TARGET/rules/
cp rules/*.sh $TARGET/rules/
cp AGENTS.md $TARGET/AGENTS.md

cd $TARGET && bash scripts/setup.sh
```

---

## Project Architecture

```
Claude Desktop --(MCP/stdio)--> Python MCP Server --(TCP:13377)--> UE5 C++ Plugin --> Editor
```

| Component | Path | Role |
|-----------|------|------|
| Python MCP Server | `mcp-server/` | Tool definitions, JSON serialization, TCP communication |
| UE5 C++ Plugin | `Plugins/UnrealMCP/` | TCP listening, JSON->UE API, GameThread execution |

## Development Phases

| Phase | Module | Description |
|-------|--------|-------------|
| 1 | Actor & Scene | Actor CRUD, transform, search |
| 2 | Blueprint | BP creation, node/pin editing, compile |
| 3 | Material & Asset | Material creation, asset search/import |
| 4 | AI System | BehaviorTree, Blackboard, EQS |
| 5 | Editor Automation | PIE, viewport, console, screenshot |
| 6 | Advanced | Niagara, AnimBP, UMG, DataTable |

Full tool list: see `rules/tool-development.md`.

---

## Recommended Tools

```bash
pip install autoflake vulture ruff --break-system-packages
```

---

## Adding New Rules

Add rules whenever a failure occurs to evolve the harness.

1. `rules/{rule-name}.sh` - Write an automated check script
2. `rules/{rule-name}.md` - Write rule background and description
3. Add one line to the failure log at the bottom of `AGENTS.md`

```
<!-- YYYY-MM-DD | failure summary | rules/{rule-name}.md added -->
```
