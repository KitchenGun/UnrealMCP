#!/bin/bash
# no-print-debug.sh — 디버그용 print 금지 규칙
# 생성 배경: 2026-04-06 | print("debug") 남긴 채 배포하여 로그 오염
# 자동 교정: 가능 (--fix 모드에서 주석 처리)

FIX=${1:-false}

FOUND=0

while IFS= read -r file; do
  # print("debug 또는 print("test 같은 패턴 탐지
  matches=$(grep -n 'print\s*(\s*["\x27]\(debug\|test\|TODO\|FIXME\|xxx\)' "$file" 2>/dev/null || true)
  if [ -n "$matches" ]; then
    if [ "$FIX" = "true" ]; then
      # 주석 처리로 자동 교정
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
exit 0
