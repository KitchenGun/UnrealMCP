#!/bin/bash
# setup.sh — Unreal Engine MCP 하네스 초기 설정
# 사용법: bash scripts/setup.sh
# 훅 설치, 도구 확인, rules 디렉토리 준비

set -euo pipefail

echo "⚙️  Unreal MCP 하네스 설정 중..."
echo ""

# ── 1. Git 확인 ──
if [ ! -d ".git" ]; then
  echo "git init 실행 중..."
  git init
fi

# ── 2. 디렉토리 구조 확인 ──
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 현재 디렉토리가 하네스 루트가 아닌 경우 (다른 프로젝트에 복사된 경우)
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo ".")"

# ── 3. 필요한 파일 복사 (다른 프로젝트에 설치하는 경우) ──
if [ "$HARNESS_ROOT" != "$PROJECT_ROOT" ]; then
  echo "📂 하네스 파일을 프로젝트로 복사 중..."

  mkdir -p "$PROJECT_ROOT/scripts"
  mkdir -p "$PROJECT_ROOT/hooks"
  mkdir -p "$PROJECT_ROOT/rules"

  cp "$HARNESS_ROOT/scripts/"*.sh "$PROJECT_ROOT/scripts/" 2>/dev/null || true
  cp "$HARNESS_ROOT/hooks/pre-commit" "$PROJECT_ROOT/hooks/" 2>/dev/null || true
  cp "$HARNESS_ROOT/rules/"*.md "$PROJECT_ROOT/rules/" 2>/dev/null || true
  cp "$HARNESS_ROOT/rules/"*.sh "$PROJECT_ROOT/rules/" 2>/dev/null || true

  # AGENTS.md는 없는 경우에만 복사 (기존 것 보존)
  if [ ! -f "$PROJECT_ROOT/AGENTS.md" ]; then
    cp "$HARNESS_ROOT/AGENTS.md" "$PROJECT_ROOT/AGENTS.md"
    echo "  ✅ AGENTS.md 복사"
  else
    echo "  ℹ️  AGENTS.md 이미 존재 (기존 파일 유지)"
  fi

  cd "$PROJECT_ROOT"
fi

# ── 4. pre-commit 훅 설치 ──
mkdir -p .git/hooks

if [ -f "hooks/pre-commit" ]; then
  cp hooks/pre-commit .git/hooks/pre-commit
  chmod +x .git/hooks/pre-commit
  echo "✅ pre-commit 훅 설치 완료"
else
  echo "⚠️  hooks/pre-commit 파일이 없습니다."
fi

# ── 5. 스크립트 실행 권한 ──
chmod +x scripts/*.sh 2>/dev/null || true
chmod +x rules/*.sh 2>/dev/null || true
echo "✅ 스크립트 실행 권한 설정 완료"

# ── 6. rules 디렉토리 준비 ──
mkdir -p rules
if [ ! -f "rules/README.md" ]; then
  cat > rules/README.md << 'EOF'
# Rules 디렉토리

실패할 때마다 여기에 규칙을 추가합니다.

## 규칙 파일 형식

### .md — 사람이 읽는 규칙 설명
왜 이 규칙이 생겼는지, 어떤 실패에서 왔는지 기록합니다.

### .sh — 린터가 실행하는 자동 검사
`./scripts/lint.sh`가 이 디렉토리의 모든 .sh를 실행합니다.
첫 번째 인자가 "true"이면 자동 교정 모드입니다.

```bash
#!/bin/bash
# 규칙 스크립트 템플릿
FIX=${1:-false}
FOUND=0
# 검사 로직...
[ $FOUND -gt 0 ] && exit 1
exit 0
```
EOF
  echo "✅ rules/README.md 생성 완료"
fi

# ── 7. 도구 확인 (선택) ──
echo ""
echo "📦 권장 도구 상태:"
for tool in autoflake vulture ruff; do
  if command -v "$tool" &>/dev/null; then
    echo "  ✅ $tool"
  else
    echo "  ⬜ $tool (없음 — pip install $tool --break-system-packages)"
  fi
done

echo ""
echo "🎉 Unreal MCP 하네스 설정 완료!"
echo ""
echo "   🎮 Unreal Engine MCP 개발 하네스 활성화됨:"
echo "      Python MCP 서버  — Tool 정의, TCP 통신"
echo "      UE5 C++ 플러그인 — TCP 리스닝, Native API 브릿지"
echo "      대상 엔진: UE 5.5+"
echo ""
echo "   📂 규칙 파일:"
echo "      rules/coding-convention.md     — Python/C++ 코딩 컨벤션"
echo "      rules/tool-development.md      — MCP Tool 추가 절차"
echo "      rules/communication-protocol.md — TCP 통신 프로토콜"
echo "      rules/error-handling.md        — 에러 코드 체계"
echo "      rules/ue5-api-caution.md       — UE5 API 주의사항"
echo ""
echo "   실패하면 AGENTS.md에 한 줄 추가하고, rules/에 규칙을 넣으세요."
