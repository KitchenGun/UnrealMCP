#!/bin/bash
# lint.sh - Rule-based linter
# Runs .sh files in rules/ directory sequentially
# Usage: ./scripts/lint.sh [--fix] [--quiet]

set -eo pipefail

FIX=false
QUIET=false
ERRORS=0
FIXED=0

for arg in "$@"; do
  case $arg in
    --fix) FIX=true ;;
    --quiet) QUIET=true ;;
  esac
done

RULES_DIR="$(git rev-parse --show-toplevel 2>/dev/null || echo ".")/rules"

if [ ! -d "$RULES_DIR" ]; then
  echo "rules/ directory not found. Empty ruleset - pass."
  exit 0
fi

# Check doc sync
check_doc_sync() {
  local agents_md="AGENTS.md"
  if [ -f "$agents_md" ]; then
    lines=$(wc -l < "$agents_md")
    if [ "$lines" -gt 60 ]; then
      echo "ERROR doc-overflow: AGENTS.md is ${lines}lines (limit: 60lines)"
      ERRORS=$((ERRORS + 1))
    fi
  fi
}

# Run checks
check_doc_sync

# Changed files check
changed_files=$(git diff --cached --name-only --diff-filter=ACM 2>/dev/null || find . -name "*.py" -o -name "*.js" -o -name "*.ts" | head -50)

for file in $changed_files; do
  case "$file" in
    *.py)
      # Check unused imports
      if command -v autoflake &>/dev/null; then
        if ! autoflake --check --remove-all-unused-imports "$file" 2>&1 >/dev/null; then
          if [ "$FIX" = true ]; then
            autoflake --in-place --remove-all-unused-imports "$file"
            FIXED=$((FIXED + 1))
          else
            echo "ERROR unused-import: $file"
            ERRORS=$((ERRORS + 1))
          fi
        fi
      fi
      ;;
  esac
done

# Run custom rules
if [ -d "$RULES_DIR" ]; then
  find "$RULES_DIR" -maxdepth 1 -name "*.sh" -type f | sort | while read -r rule_file; do
    if [ -f "$rule_file" ]; then
      if ! bash "$rule_file" "$FIX" 2>&1; then
        rule_name=$(basename "$rule_file" .sh)
        echo "ERROR rule-fail: $rule_name"
        ERRORS=$((ERRORS + 1))
      fi
    fi
  done
fi

# Results
if [ "$QUIET" = true ] && [ $ERRORS -eq 0 ]; then
  exit 0
fi

if [ $ERRORS -gt 0 ]; then
  echo ""
  echo "Lint FAILED: ${ERRORS} error(s)"
  [ $FIXED -gt 0 ] && echo "  Auto-fixed: ${FIXED}"
  exit 1
fi

exit 0
