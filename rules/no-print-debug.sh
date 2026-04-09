#!/bin/bash
# no-print-debug.sh -- Prohibit debug-related print rules
# Creation context: 2026-04-06 | Deployed with print("debug") left in code, polluting logs
# Auto-correction: Possible (comment out in --fix mode)

FIX=${1:-false}

FOUND=0

while IFS= read -r file; do
  # Detect patterns like print("debug or print("test
  matches=$(grep -n 'print\s*(\s*["\x27]\(debug\|test\|TODO\|FIXME\|xxx\)' "$file" 2>/dev/null || true)
  if [ -n "$matches" ]; then
    if [ "$FIX" = "true" ]; then
      # Auto-correct by commenting out
      sed -i 's/^\(\s*\)\(print\s*(\s*["\x27]\(debug\|test\|TODO\|FIXME\|xxx\)\)/\1# REMOVED: \2/' "$file"
      echo "FIXED debug-print: $file"
    else
      echo "ERROR debug-print: $file"
      echo "$matches" | sed 's/^/  /'
      FOUND=$((FOUND + 1))
    fi
  fi
done < <(find . -name "*.py" -not -path "./.venv/*" -not -path "./node_modules/*")

[ $FOUND -gt 0 ] && exit 1
exit