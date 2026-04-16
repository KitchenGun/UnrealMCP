# UE5 Vertical Shooting Game — 프로젝트 컨텍스트

> 이 파일은 세션 시작 시 자동 로드됩니다. PDF를 재독하지 마세요.
> 기획서 원본: `C:/Users/kang9/Downloads/슈팅게임_기획서.pdf` (11페이지, v0.4)

---

## 게임 개요

- **장르**: 3D 그래픽 기반 2D 종스크롤 슈팅
- **핵심 컨셉**: 지상/공중 판정 분리 + 출격 전 로드아웃 + 피탄 허용형 설계
- **레퍼런스**: Raptor, Strikers 1945, Raystorm, Radiant Silvergun, Ace Combat X

---

## 확정된 아키텍처 결정사항

### GAS 사용 여부
**미사용 확정.** 이유: 능력 수 적음(슈퍼웨폰 ~9종), 로드아웃 고정(런타임 동적 부여 불필요), 로컬 코옵만, HP/Shield/Gauge 3개 속성은 커스텀 컴포넌트로 충분. **GameplayTag는 GAS 없이 독립 사용.**

### 핵심 열거형
```
ETargetLayer  : Universal / Air / Ground   (무장 속성)
EUnitLayer    : Air / Ground               (유닛 속성)
ESuperWeaponCategory : Offensive / Defensive / Utility
```

### 히트 판정 원칙
```
CanDamage(WeaponLayer, TargetLayer)
  Universal → 항상 true
  Air       → TargetLayer == Air 일 때만 true
  Ground    → TargetLayer == Ground 일 때만 true
```

### 플레이어 기체 컴포넌트 구성
| 컴포넌트 | 역할 |
|---|---|
| ULoadoutComponent | 출격 전 빌드 세팅 (무기×3 + 슈퍼웨폰×1 + 보호막×1) |
| UWeaponFireComponent | 기본/특수 무기 발사 |
| USuperWeaponComponent | 게이지→스톡 전환, 발동 |
| UHealthComponent | HP 관리, 사망 처리 |
| UShieldComponent | 보호막 선흡수 → 잔여 HP 전달 |
| UPlayerMovementExt | 통상/저속 모드 토글 |

### 데이터 주도 원칙
- 무기/보스 페이즈/웨이브/해금 조건 → **DataAsset / DataTable**
- 웨이브 조절 → UCampaignState (GameInstanceSubsystem) → FWaveRowData 파라미터 수정
- 보스 → FBossPhaseData (DataTable), HP% 트리거, ActiveUnitLayer 전환

### 슈퍼웨폰 게이지
```
충전 경로: 적 격파(메인) / 시간 자연충전(서브) / 아이템 드롭(보너스)
게이지 MAX → StockCurrent++
강한 슈퍼웨폰 = 낮은 MaxStock / 약한 슈퍼웨폰 = 높은 MaxStock
```

### 스코어링 3축
1. **격파 속도 보너스** — 등장~격파 시간이 짧을수록 ↑ (특화 빌드 유리)
2. **연속 격파 체인** — 끊기지 않게 연속 격파 (범용 빌드 유리)
3. **슈퍼웨폰 미사용 보너스** — 리스크-보상 판단 요소

---

## 개발 우선순위

| 단계 | 항목 |
|---|---|
| **P0 코어** | 지상/공중 판정 · 로드아웃 · Shield+HP · 플레이어 기동 |
| **P1 중요** | 보스 페이즈 · 슈퍼웨폰 게이지/스톡 · 미션 브리핑 · 클리어 평가 · 아이템 드롭 · 스테이지 스크롤 |
| **P2 확장** | 스코어링 · 장비 해금 · 동적 캠페인 · 난이도별 정보 차등 |
| **P3 후순위** | 난이도 커브 · 로컬 2P 코옵 |

---

## 응답 규칙

- 응답 언어: **한국어**
- 코드 불필요 시: 구조/설계 다이어그램 중심으로 답변
- 코드 필요 시: UE5 C++ 스타일 (UPROPERTY, UFUNCTION 등 UE 매크로 포함)
- 기획서 재독 금지: 이 파일의 정보를 우선 참조
