#!/bin/bash
# garbage-collect.sh - Garbage collection
# Detects doc/code mismatch, dead code, unused rules

set -eo pipefail

ISSUES=0

# --- 1. Doc <-> code sync ---
if [ -d "rules" ]; then
  find rules -maxdepth 1 -name "*.md" -type f | while read -r rule; do
    { grep -oE '`[^`]+\.(py|js|ts|sh)`' "$rule" 2>/dev/null || true; } | tr -d '`' | while read -r ref; do
      if [ ! -f "$ref" ]; then
        echo "STALE doc-ref: $rule -> $ref (file not found)"
        ISSUES=$((ISSUES + 1))
      fi
    done
  done
fi

# --- 2. Dead code detection ---
if command -v vulture &>/dev/null; then
  find . -name "*.py" -not -path "./.venv/*" -not -path "./node_modules/*" | head -100 | while read -r pyfile; do
    vulture "$pyfile" --min-confidence 90 2>/dev/null | while read -r line; do
      echo "DEAD $line"
      ISSUES=$((ISSUES + 1))
    done
  done
fi

# --- 3. Unused import detection ---
find . -name "*.py" -not -path "./.venv/*" -not -path "./node_modules/*" | head -100 | while read -r pyfile; do
  if command -v autoflake &>/dev/null; then
    autoflake --check --remove-all-unused-imports "$pyfile" 2>/dev/null || {
      echo "UNUSED import: $pyfile"
      ISSUES=$((ISSUES + 1))
    }
  fi
done

# --- 4. Unused rule file detection ---
if [ -d "rules" ]; then
  find rules -maxdepth 1 -name "*.sh" -type f | while read -r rule_script; do
    rule_name=$(basename "$rule_script" .sh)
    if ! grep -rq "$rule_name" AGENTS.md rules/*.md 2>/dev/null; then
      echo "UNUSED rule: $rule_script (not referenced anywhere)"
      ISSUES=$((ISSUES + 1))
    fi
  done
fi

# --- 5. Result ---
if [ $ISSUES -gt 0 ]; then
  echo ""
  echo "[GC] Garbage collection: ${ISSUES} issue(s) found"
  exit 1
fi

exit 0
