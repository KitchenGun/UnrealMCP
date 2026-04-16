# GitHub Copilot Instructions — UnrealMCP + UE5 슈팅게임

응답 언어: **한국어**
코드 스타일: UE5 C++ (Epic 컨벤션 A/U/F/E/I/b, UPROPERTY 필수, IsValid() 사용, GameThread에서 UE API 호출)

---

## 프로젝트 1: UnrealMCP 서버

**목표**: MCP 서버 + C++ 플러그인으로 UE5 에디터를 자연어로 제어

```
Claude Desktop --(stdio)--> Python MCP Server --(TCP:13377)--> UE5 C++ Plugin --> Editor
```

- Python: async/await, type hints + docstring 필수, `@server.tool()` 데코레이터
- C++: BeginTransaction() / EndTransaction() Undo 지원
- 통신: 개행 구분 JSON, 재연결 3s × 10회, timeout 30s

---

## 프로젝트 2: UE5 종스크롤 슈팅게임 (설계 단계)

기획서: `C:/Users/kang9/Downloads/슈팅게임_기획서.pdf` (v0.4)

### 확정 아키텍처

**GAS 미사용** — 커스텀 컴포넌트 + GameplayTag 독립 사용

**핵심 타입**
```cpp
enum class ETargetLayer  : uint8 { Universal, Air, Ground };
enum class EUnitLayer    : uint8 { Air, Ground };
enum class ESuperWeaponCategory : uint8 { Offensive, Defensive, Utility };

// 히트 판정
bool CanDamage(ETargetLayer WeaponLayer, EUnitLayer TargetLayer);
// Universal=항상true / Air=Air만 / Ground=Ground만
```

**플레이어 컴포넌트**
- `ULoadoutComponent` — 출격 전 빌드 (무기×3 + 슈퍼웨폰×1 + 보호막×1)
- `UWeaponFireComponent` — 발사 로직
- `USuperWeaponComponent` — 게이지→스톡 전환
- `UHealthComponent` — HP
- `UShieldComponent` — 보호막 선흡수, 잔여만 HP 전달
- `UPlayerMovementExt` — 통상/저속 토글

**데이터 주도**: DataAsset/DataTable 기반. UCampaignState(Subsystem)로 동적 캠페인 파라미터 관리.

**보스**: ABossBase + FBossPhaseData(DataTable) + HP% 페이즈 전환 + 3D 변신 연출

**개발 순서**: P0(판정/로드아웃/생존/기동) → P1(보스/슈퍼웨폰/브리핑/평가) → P2(스코어링/해금/캠페인)

---

## 공통 규칙

- 코드 코멘트와 docstring은 **한국어**
- 새 기능 구현 전 기존 코드 파악 우선
- 커밋: `feat(actor):`, `fix(blueprint):`, `docs(setup):` 형식
