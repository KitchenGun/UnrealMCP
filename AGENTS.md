# AGENTS.md — Unreal Engine MCP 프로젝트 컨텍스트

## 목표
자연어로 UE5 에디터를 제어하는 MCP 서버 + C++ 플러그인 개발.
베이스: GenOrca/unreal-mcp (fork). 참고: flopperam/unreal-engine-mcp.

## 아키텍처
```
Claude Desktop ──(stdio)──▶ Python MCP Server ──(TCP:13377)──▶ UE5 C++ Plugin ──▶ Editor
```
- **Python MCP 서버** (`mcp-server/`): Tool 정의, JSON 직렬화, TCP 통신
- **UE5 C++ 플러그인** (`Plugins/UnrealMCP/`): TCP 리스닝, JSON→UE API, GameThread 실행

## 개발 Phase
| Phase | 모듈 | 핵심 Tool |
|-------|------|-----------|
| 1 | Actor & Scene | create/delete/transform/find actor |
| 2 | Blueprint | create BP, add node, connect pins, compile |
| 3 | Material & Asset | create material, search/import asset |
| 4 | AI 시스템 | BehaviorTree, Blackboard, EQS, AIPerception |
| 5 | 에디터 자동화 | PIE, viewport, console, screenshot |
| 6 | 고급 시스템 | Niagara, AnimBP, UMG, DataTable |

## 코딩 규칙
- **Python**: async/await, 타입 힌트+docstring 필수, JSON 문자열 반환, `@server.tool()` 데코레이터
- **C++**: Epic 컨벤션(A/U/F/E/I/b), `UPROPERTY()` 필수, `IsValid()` 사용, GameThread에서 UE API 호출
- **통신**: newline-delimited JSON, 재연결 3초×10회, 타임아웃 30초(무거운 작업 60초)
- **한국어 주석**: 코드 주석과 docstring은 한국어로 작성
- **Undo 지원**: `BeginTransaction()`/`EndTransaction()` 활용
- 세부 기준은 `rules/` 참조

## 작업 원칙
1. Phase·Tool 확인 → 기존 코드 파악 → 점진적 구현 (한 번에 하나)
2. 각 Tool은 독립 테스트 가능해야 함 (성공 1 + 에러 1 최소)
3. 커밋: `feat(actor):`, `fix(blueprint):`, `docs(setup):` 형식

## 하네스 규칙
- 커밋 전 `./scripts/lint.sh` 통과 필수
- 실패 → 자동교정(최대 3회) → 그래도 실패 시 알림
- 성공은 조용히, 실패만 시끄럽게. 이 파일은 **60줄 이하** 유지

## 핵심 규칙 문서 (rules/)
- `rules/coding-convention.md` — Python/C++ 상세 코딩 컨벤션
- `rules/tool-development.md` — MCP Tool 추가 절차 및 템플릿
- `rules/communication-protocol.md` — TCP 통신 프로토콜 상세
- `rules/error-handling.md` — 에러 코드 체계 및 메시지 원칙
- `rules/ue5-api-caution.md` — UE5 API 사용 시 주의사항

## 실패 로그
<!-- 예시: 2026-04-08 | GameThread 밖에서 UE API 호출 → 크래시 | rules/ue5-api-caution.md 추가 -->
<!-- 2026-04-09 | pre-commit 린트 실패 (에러 0
0건) | 검토 필요 -->
