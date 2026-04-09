#!/bin/bash
# setup.sh - Unreal Engine MCP harness initial setup
# Usage: bash scripts/setup.sh

set -euo pipefail

echo "[SETUP] Unreal MCP harness setup starting..."
echo ""

# --- 1. Git check ---
if [ ! -d ".git" ]; then
  echo "Running git init..."
  git init
fi

# --- 2. Directory structure ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo ".")"

# --- 3. Copy files (when installing to another project) ---
if [ "$HARNESS_ROOT" != "$PROJECT_ROOT" ]; then
  echo "[COPY] Copying harness files to project..."

  mkdir -p "$PROJECT_ROOT/scripts"
  mkdir -p "$PROJECT_ROOT/hooks"
  mkdir -p "$PROJECT_ROOT/rules"

  cp "$HARNESS_ROOT/scripts/"*.sh "$PROJECT_ROOT/scripts/" 2>/dev/null || true
  cp "$HARNESS_ROOT/hooks/pre-commit" "$PROJECT_ROOT/hooks/" 2>/dev/null || true
  cp "$HARNESS_ROOT/rules/"*.md "$PROJECT_ROOT/rules/" 2>/dev/null || true
  cp "$HARNESS_ROOT/rules/"*.sh "$PROJECT_ROOT/rules/" 2>/dev/null || true

  if [ ! -f "$PROJECT_ROOT/AGENTS.md" ]; then
    cp "$HARNESS_ROOT/AGENTS.md" "$PROJECT_ROOT/AGENTS.md"
    echo "  [OK] AGENTS.md copied"
  else
    echo "  [INFO] AGENTS.md already exists (keeping existing)"
  fi

  cd "$PROJECT_ROOT"
fi

# --- 4. Install pre-commit hook ---
mkdir -p .git/hooks

if [ -f "hooks/pre-commit" ]; then
  cp hooks/pre-commit .git/hooks/pre-commit
  chmod +x .git/hooks/pre-commit
  echo "[OK] pre-commit hook installed"
else
  echo "[WARN] hooks/pre-commit file not found."
fi

# --- 5. Script permissions ---
chmod +x scripts/*.sh 2>/dev/null || true
chmod +x rules/*.sh 2>/dev/null || true
echo "[OK] Script permissions set"

# --- 6. Prepare rules directory ---
mkdir -p rules
if [ ! -f "rules/README.md" ]; then
  cat > rules/README.md << 'EOF'
# Rules Directory

Add rules here whenever a failure occurs.

## File format

### .md - Human-readable rule description
Records why this rule was created, what failure triggered it.

### .sh - Automated check script
`./scripts/lint.sh` runs all .sh files in this directory.
First argument "true" enables auto-fix mode.

```bash
#!/bin/bash
# Rule script template
FIX=${1:-false}
FOUND=0
# Check logic...
[ $FOUND -gt 0 ] && exit 1
exit 0
```
EOF
  echo "[OK] rules/README.md created"
fi

# --- 7. Tool check (optional) ---
echo ""
echo "[TOOLS] Recommended tool status:"
for tool in autoflake vulture ruff; do
  if command -v "$tool" &>/dev/null; then
    echo "  [OK] $tool"
  else
    echo "  [--] $tool (not found - pip install $tool --break-system-packages)"
  fi
done

echo ""
echo "[DONE] Unreal MCP harness setup complete!"
echo ""
echo "   Unreal Engine MCP dev harness activated:"
echo "      Python MCP Server  - Tool definitions, TCP communication"
echo "      UE5 C++ Plugin     - TCP listening, Native API bridge"
echo "      Target engine: UE 5.5+"
echo ""
echo "   Rule files:"
echo "      rules/coding-convention.md      - Python/C++ coding conventions"
echo "      rules/tool-development.md       - MCP Tool development procedure"
echo "      rules/communication-protocol.md - TCP communication protocol"
echo "      rules/error-handling.md         - Error code system"
echo "      rules/ue5-api-caution.md        - UE5 API cautions"
echo ""
echo "   On failure: add a line to AGENTS.md and a rule to rules/"
