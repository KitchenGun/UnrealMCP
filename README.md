# unreal-mcp-harness

> 🎮 Unreal Engine MCP 프로젝트 개발 하네스

Claude Desktop에서 UE5 에디터를 자연어로 제어하는 MCP 서버 + C++ 플러그인 개발을 위한
코드 품질 자동 강제 + 프로젝트 컨텍스트 하네스입니다.

---

## 📁 폴더 구조

```
unreal-mcp-harness/
├── AGENTS.md                          ← 프로젝트 컨텍스트 (60줄 이하, 항상 적용)
├── README.md                          ← 이 파일
├── hooks/
│   └── pre-commit                     ← Git 커밋 전 자동 실행 훅
├── scripts/
│   ├── setup.sh                       ← 최초 설치 스크립트
│   ├── lint.sh                        ← 규칙 기반 린터 (자동교정 포함)
│   └── garbage-collect.sh             ← 죽은 코드/문서 불일치 탐지
└── rules/
    ├── coding-convention.md           ← Python/C++ 코딩 컨벤션 상세
    ├── tool-development.md            ← MCP Tool 추가 절차 + Phase별 Tool 목록
    ├── communication-protocol.md      ← TCP 통신 프로토콜 상세
    ├── error-handling.md              ← 에러 코드 체계 및 메시지 원칙
    ├── ue5-api-caution.md             ← UE5 API 사용 시 주의사항
    ├── no-print-debug.sh              ← 디버그 print 자동 탐지·제거
    └── no-print-debug.md              ← no-print-debug 규칙 문서
```

---

## 🚀 다른 프로젝트에 적용하기

### 방법 1 — 폴더째 복사 후 setup 실행

```bash
# 1. 이 폴더 전체를 대상 프로젝트로 복사
cp -r unreal-mcp-harness/ /path/to/your-project/

# 2. 대상 프로젝트 루트로 이동
cd /path/to/your-project/unreal-mcp-harness

# 3. 설치 스크립트 실행
bash scripts/setup.sh
```

### 방법 2 — 파일만 추출하여 적용

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

## 🏗️ 프로젝트 아키텍처

```
Claude Desktop ──(MCP/stdio)──▶ Python MCP Server ──(TCP:13377)──▶ UE5 C++ Plugin ──▶ Editor
```

| 구성 요소 | 경로 | 역할 |
|-----------|------|------|
| Python MCP 서버 | `mcp-server/` | Tool 정의, JSON 직렬화, TCP 통신 |
| UE5 C++ 플러그인 | `Plugins/UnrealMCP/` | TCP 리스닝, JSON→UE API, GameThread 실행 |

## 📋 개발 Phase

| Phase | 모듈 | 설명 |
|-------|------|------|
| 1 | Actor & Scene | 액터 CRUD, 트랜스폼, 검색 |
| 2 | Blueprint | BP 생성, 노드/핀 편집, 컴파일 |
| 3 | Material & Asset | 머티리얼 생성, 에셋 검색/임포트 |
| 4 | AI 시스템 | BehaviorTree, Blackboard, EQS |
| 5 | 에디터 자동화 | PIE, 뷰포트, 콘솔, 스크린샷 |
| 6 | 고급 시스템 | Niagara, AnimBP, UMG, DataTable |

전체 Tool 목록은 `rules/tool-development.md` 참조.

---

## 🔧 권장 도구

```bash
pip install autoflake vulture ruff --break-system-packages
```

---

## ➕ 새 규칙 추가

실패가 발생할 때마다 규칙을 추가하여 하네스를 진화시킵니다.

1. `rules/{규칙이름}.sh` — 자동 검사 스크립트 작성
2. `rules/{규칙이름}.md` — 규칙 배경 및 설명 작성
3. `AGENTS.md` 하단 실패 로그에 한 줄 추가

```
<!-- YYYY-MM-DD | 실패 요약 | rules/{규칙이름}.md 추가 -->
```
