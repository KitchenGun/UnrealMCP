# UnrealMCP

자연어로 UE5 에디터를 제어하는 **MCP(Model Context Protocol) 서버 + C++ 플러그인** 프로젝트.

Claude Desktop에서 대화하듯 명령하면 UE5 에디터가 움직입니다.

```
Claude Desktop ──(MCP/stdio)──▶ Python MCP Server ──(TCP:13377)──▶ UE5 C++ Plugin ──▶ Editor
```

---

## 빠른 시작 (3단계)

### 1단계 — UE5 플러그인 설치 및 빌드

`Plugins/UnrealMCP/` 폴더 전체를 본인의 UE5 프로젝트 `Plugins/` 폴더에 복사합니다.

```
MyUE5Project/
└── Plugins/
    └── UnrealMCP/   ← 이 폴더를 복사
        ├── Source/
        └── UnrealMCP.uplugin
```

UE5 에디터를 열면 "새 모듈이 감지되었습니다. 지금 빌드하시겠습니까?" 팝업이 나타납니다. **Yes** 클릭.

빌드 성공 시 출력 로그에 다음 메시지가 표시됩니다:

```
[UnrealMCP] 수신 스레드 시작. 포트 13377 대기 중.
```

> **주의:** 플러그인은 에디터 전용(`bBuildEditor = true`)입니다. 패키징 빌드에서는 자동 제외됩니다.

---

### 2단계 — Python MCP 서버 설치

Python 3.11 이상이 필요합니다.

```bash
cd mcp-server

# pip 사용
pip install -e .

# 또는 uv 사용 (권장)
uv sync
```

설치 확인:

```bash
python -m unreal_mcp
# 출력: "Unreal MCP server starting (Phase 1: Actor & Scene | ...)"
```

---

### 3단계 — Claude Desktop에 DXT 확장 설치

> Claude Desktop **1.3883.0.0 이상**은 레거시 `claude_desktop_config.json`의 `mcpServers` 필드를 지원하지 않습니다. DXT 확장으로 설치합니다.

**방법 A — 빌드된 `.dxt` 파일 사용 (권장)**

1. [Releases 페이지](https://github.com/KitchenGun/UnrealMCP/releases)에서 `unreal-mcp-<version>.dxt` 다운로드 (또는 직접 빌드: [아래 참고](#dxt-직접-빌드))
2. Claude Desktop 실행 → **설정(Settings)** → **확장(Extensions)** / **커넥터(Connectors)**
3. **"파일에서 설치"** / 드래그 앤 드롭 → `.dxt` 파일 선택
4. 설치 확인 팝업에서 **Install** 클릭
5. Claude Desktop 자동 재시작 후 `Unreal Engine MCP` 서버가 활성화됨

**방법 B — 개발 모드 (소스 직접 반영)**

툴 추가/수정을 자주 할 경우, 설치 후 아래 스크립트로 설치본을 원본에 심볼릭 링크:

```powershell
# 관리자 권한 PowerShell 또는 Windows 개발자 모드 활성화 필요
.\scripts\dev-link.ps1
```

이후 `mcp-server/src/unreal_mcp/**` 원본을 수정하면 Claude Desktop 재시작만으로 반영됩니다 (재빌드 불필요).

#### DXT 직접 빌드

```powershell
# PowerShell (Windows)
.\scripts\build-dxt.ps1
# → dist\unreal-mcp-<version>.dxt 생성
```

빌드 과정: `mcp-server/src/` 복사 → `pip install -t lib/` 의존성 벤더링 → `manifest.json` 포함 → zip 압축.

---

### 실행 순서

```
1. UE5 에디터 열기
   → 플러그인 자동 시작 → 포트 13377 리스닝

2. Claude Desktop 재시작
   → MCP 서버 자동 연결 → UE5 플러그인에 TCP 접속

3. Claude에게 자연어로 명령
```

**사용 예시:**

```
"씬에 PointLight를 (0, 0, 300) 위치에 만들어줘"
"BP_EnemyAI라는 AIController Blueprint를 /Game/AI 폴더에 생성해줘"
"현재 레벨의 모든 StaticMeshActor 목록을 알려줘"
"M_Rock 머티리얼을 만들고 SM_Rock_01 액터에 적용해줘"
"PIE 시작해줘"
"뷰포트 카메라를 (0, 0, 500) 위치로 이동해줘"
```

---

## 프로젝트 구조

```
UnrealMCP/
├── Plugins/UnrealMCP/                        # UE5 C++ 플러그인
│   └── Source/UnrealMCP/
│       ├── Public/Handlers/                  # 핸들러 헤더
│       │   ├── ActorHandler.h
│       │   ├── BlueprintHandler.h
│       │   ├── MaterialHandler.h
│       │   ├── AIHandler.h
│       │   ├── EditorHandler.h
│       │   └── AdvancedHandler.h
│       ├── Private/Handlers/                 # 핸들러 구현
│       │   ├── ActorHandler.cpp
│       │   ├── BlueprintHandler.cpp
│       │   ├── MaterialHandler.cpp
│       │   ├── AIHandler.cpp
│       │   ├── EditorHandler.cpp
│       │   └── AdvancedHandler.cpp
│       └── Private/MCPTcpServer.cpp          # TCP 서버 (포트 13377)
│
├── mcp-server/                               # Python MCP 서버
│   └── src/unreal_mcp/
│       ├── main.py                           # 서버 진입점
│       ├── connection.py                     # TCP 통신
│       └── tools/                            # MCP Tool 정의
│           ├── actor.py       (Phase 1)
│           ├── blueprint.py   (Phase 2)
│           ├── material.py    (Phase 3)
│           ├── ai_system.py   (Phase 4)
│           ├── editor.py      (Phase 5)
│           └── advanced.py    (Phase 6)
│
└── rules/                                    # 개발 규칙 문서
```

---

## 구현 Phase

| Phase | 모듈 | Tools | 주요 기능 |
|-------|------|-------|-----------|
| 1 | Actor & Scene | 8개 | create_actor, delete_actor, set_actor_transform, get_actors_in_level, find_actors_by_name, duplicate_actor, get_actor_properties, set_actor_property |
| 2 | Blueprint 편집 | 9개 | create_blueprint, add_blueprint_node, connect_blueprint_pins, remove_blueprint_node, add_blueprint_variable, compile_blueprint, get_blueprint_graph, add_blueprint_component, spawn_blueprint_actor |
| 3 | Material & Asset | 10개 | search_assets, get_asset_details, create_material, add_material_expression, connect_material_nodes, apply_material_to_actor, set_material_parameter, import_asset, duplicate_asset, delete_asset |
| 4 | AI 시스템 | 7개 | create_behavior_tree, add_bt_node, create_blackboard, add_blackboard_key, create_eqs_query, setup_ai_perception, create_ai_controller |
| 5 | 에디터 자동화 | 8개 | play_in_editor, set_viewport_camera, run_console_command, take_screenshot, get_selected_actors, select_actors, save_level, load_level |
| 6 | 고급 시스템 | 6개 | create_niagara_system, create_animation_blueprint, create_widget_blueprint, create_data_table, create_data_asset, inspect_uobject |

**총 48개 MCP Tools**

---

## 통신 프로토콜

Python MCP 서버와 UE5 플러그인은 **TCP 포트 13377**으로 통신하며, **Newline-Delimited JSON** 형식을 사용합니다.

**요청 형식:**
```json
{"id": "uuid", "type": "create_actor", "params": {"actor_class": "PointLight", "location": [0, 0, 300]}}
```

**응답 형식 (성공):**
```json
{"id": "uuid", "success": true, "result": {"name": "PointLight_1", "location": {"x": 0, "y": 0, "z": 300}}, "error": null}
```

**응답 형식 (실패):**
```json
{"id": "uuid", "success": false, "result": null, "error": {"code": "ACTOR_NOT_FOUND", "message": "액터 'BP_Enemy'를 찾을 수 없습니다."}}
```

### 에러 코드

| 코드 | 설명 |
|------|------|
| `INVALID_PARAMS` | 파라미터 누락 또는 잘못된 값 |
| `ACTOR_NOT_FOUND` | 지정한 이름의 액터가 레벨에 없음 |
| `ASSET_NOT_FOUND` | 지정한 경로의 에셋을 찾을 수 없음 |
| `INTERNAL_ERROR` | UE5 내부 처리 실패 |
| `IMPORT_FAILED` | 외부 파일 임포트 실패 |
| `DELETE_FAILED` | 에셋 삭제 실패 (참조 중일 가능성) |
| `INVALID_STATE` | PIE 미실행 등 부적절한 상태 |
| `CONNECTION_TIMEOUT` | 명령 처리 30초 초과 |

---

## Build.cs 모듈 의존성

플러그인 컴파일에 필요한 UE5 모듈 목록:

```
UnrealEd, Sockets, Networking, Json, JsonUtilities,
BlueprintGraph, KismetCompiler, AssetTools,       ← Phase 1~2
AssetRegistry, MaterialEditor,                    ← Phase 3
AIModule, GameplayTasks, EnvironmentQuery,        ← Phase 4
LevelEditor,                                      ← Phase 5
Niagara, NiagaraEditor, AnimGraph, UMG, UMGEditor ← Phase 6
```

---

## 개발 규칙

자세한 내용은 `rules/` 폴더를 참고하세요.

- `rules/coding-convention.md` — Python/C++ 코딩 컨벤션
- `rules/tool-development.md` — MCP Tool 개발 절차 및 Phase 목록
- `rules/communication-protocol.md` — TCP 통신 프로토콜 명세
- `rules/error-handling.md` — 에러 코드 시스템
- `rules/ue5-api-caution.md` — UE5 API 사용 주의사항

---

## 자주 묻는 문제

**Q. 빌드 후 "포트 13377 대기 중" 로그가 안 보여요.**

플러그인 활성화 여부를 확인하세요. 편집 → 플러그인 → UnrealMCP 검색 후 체크박스가 활성화되어 있어야 합니다. 에디터 재시작이 필요할 수 있습니다.

**Q. Claude에서 연결이 안 돼요.**

UE5 에디터가 먼저 실행되어 있어야 합니다. Claude Desktop 재시작 전에 에디터를 먼저 열어두세요.

**Q. Phase 4 BT 노드가 그래프에 안 보여요.**

`add_bt_node`는 노드 정보를 반환하지만 그래프 시각적 편집은 UE5 BT 에디터에서 직접 수행해야 합니다. 응답의 `note` 필드를 확인하세요.

**Q. Niagara 모듈 빌드 에러가 나요.**

프로젝트의 `.uproject` 파일에서 Niagara 플러그인이 활성화되어 있는지 확인하세요:
```json
{"Name": "Niagara", "Enabled": true}
```
