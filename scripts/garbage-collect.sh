#!/bin/bash
# garbage-collect.sh — 가비지 컬렉션
# 문서/코드 불일치, 죽은 코드, 안 쓰는 규칙 탐지

set -eo pipefail

ISSUES=0

# ── 1. 문서 ↔ 코드 동기화 ──
# rules/ 에 언급된 파일이 실제 존재하는지
if [ -d "rules" ]; then
  for rule in rules/*.md 2>/dev/null; do
    [ -f "$rule" ] || continue
    # 규칙 파일 안에서 참조하는 소스 파일 추출
    grep -oP '`[^`]+\.(py|js|ts|sh)`' "$rule" 2>/dev/null | tr -d '`' | while read -r ref; do
      if [ ! -f "$ref" ]; then
        echo "STALE doc-ref: $rule → $ref (파일 없음)"
        ISSUES=$((ISSUES + 1))
      fi
    done
  done
fi

# ── 2. 죽은 코드 탐지 ──
# Python: 미사용 함수/클래스
if command -v vulture &>/dev/null; then
  find . -name "*.py" -not -path "./.venv/*" -not -path "./node_modules/*" | head -100 | while read -r pyfile; do
    vulture "$pyfile" --min-confidence 90 2>/dev/null | while read -r line; do
      echo "DEAD $line"
      ISSUES=$((ISSUES + 1))
    done
  done
fi

# ── 3. 미사용 import 탐지 ──
find . -name "*.py" -not -path "./.venv/*" -not -path "./node_modules/*" | head -100 | while read -r pyfile; do
  # 간단한 grep 기반 탐지 (autoflake 없을 때 대체)
  if command -v autoflake &>/dev/null; then
    autoflake --check --remove-all-unused-imports "$pyfile" 2>/dev/null || {
      echo "UNUSED import: $pyfile"
      ISSUES=$((ISSUES + 1))
    }
  fi
done

# ── 4. 안 쓰는 규칙 파일 탐지 ──
if [ -d "rules" ]; then
  for rule_script in rules/*.sh 2>/dev/null; do
    [ -f "$rule_script" ] || continue
    rule_name=$(basename "$rule_script" .sh)
    # AGENTS.md나 다른 문서에서 언급되지 않는 규칙
    if ! grep -rq "$rule_name" AGENTS.md rules/*.md 2>/dev/null; then
      echo "UNUSED rule: $rule_script (어디에서도 참조 안 됨)"
      ISSUES=$((ISSUES + 1))
    fi
  done
fi

# ── 5. 결과 ──
if [ $ISSUES -gt 0 ]; then
  echo ""
  echo "🗑️  가비지 컬렉션: ${ISSUES}건 발견"
  exit 1
fi

exit 0
