# tool-development — MCP Tool 추가 절차 및 Phase별 Tool 목록

## 새 Tool 추가 4단계

1. **Python 측**: `tools/` 디렉토리에 Tool 함수 추가 (데코레이터, 타입 힌트, docstring)
2. **C++ 측**: 해당 Handler에 명령어 처리 함수 추가
3. **테스트**: 성공 케이스 1개 + 에러 케이스 1개 최소 작성, UE5 에디터 통합 테스트
4. **문서**: `docs/tools-reference.md`에 Tool 사양 추가

---

## Phase 1: Actor & Scene (기본)
| Tool | 설명 |
|------|------|
| `create_actor` | 액터 생성 (StaticMesh, Light, Camera 등) |
| `delete_actor` | 액터 삭제 |
| `set_actor_transform` | 위치/회전/스케일 설정 |
| `get_actors_in_level` | 레벨 내 액터 목록 조회 |
| `find_actors_by_name` | 이름으로 액터 검색 |
| `duplicate_actor` | 액터 복제 |
| `get_actor_properties` | 액터 속성 읽기 |
| `set_actor_property` | 액터 속성 쓰기 |

## Phase 2: Blueprint 편집
| Tool | 설명 |
|------|------|
| `create_blueprint` | 새 Blueprint 클래스 생성 |
| `add_blueprint_node` | BP 그래프에 노드 추가 |
| `connect_blueprint_pins` | 노드 핀 연결 |
| `remove_blueprint_node` | 노드 제거 |
| `add_blueprint_variable` | 변수 추가 |
| `compile_blueprint` | Blueprint 컴파일 |
| `get_blueprint_graph` | 그래프 구조 읽기 |
| `add_blueprint_component` | 컴포넌트 추가 |
| `spawn_blueprint_actor` | BP 기반 액터 스폰 |

## Phase 3: Material & Asset
| Tool | 설명 |
|------|------|
| `search_assets` | 에셋 검색 (이름/타입 필터) |
| `get_asset_details` | 에셋 상세 정보 |
| `create_material` | 머티리얼 생성 |
| `add_material_expression` | 머티리얼 노드 추가 |
| `connect_material_nodes` | 머티리얼 노드 연결 |
| `apply_material_to_actor` | 액터에 머티리얼 적용 |
| `set_material_parameter` | 머티리얼 인스턴스 파라미터 설정 |
| `import_asset` | 외부 에셋 임포트 |
| `duplicate_asset` | 에셋 복제 |
| `delete_asset` | 에셋 삭제 |

## Phase 4: AI 시스템
| Tool | 설명 |
|------|------|
| `create_behavior_tree` | Behavior Tree 에셋 생성 |
| `add_bt_node` | BT에 Task/Decorator/Service 노드 추가 |
| `create_blackboard` | Blackboard 에셋 생성 |
| `add_blackboard_key` | Blackboard 키 추가 |
| `create_eqs_query` | EQS 쿼리 생성 |
| `setup_ai_perception` | AIPerception 컴포넌트 설정 |
| `create_ai_controller` | AIController Blueprint 생성 |

## Phase 5: 에디터 자동화
| Tool | 설명 |
|------|------|
| `play_in_editor` | PIE 시작/중지 |
| `set_viewport_camera` | 뷰포트 카메라 위치/방향 설정 |
| `run_console_command` | 콘솔 명령어 실행 |
| `take_screenshot` | 뷰포트 스크린샷 |
| `get_selected_actors` | 현재 선택된 액터 조회 |
| `select_actors` | 액터 선택 |
| `save_level` | 레벨 저장 |
| `load_level` | 레벨 로드 |

## Phase 6: 고급 시스템 (확장)
| Tool | 설명 |
|------|------|
| `create_niagara_system` | Niagara 파티클 시스템 생성 |
| `create_animation_blueprint` | 애니메이션 BP 생성 |
| `create_widget_blueprint` | UMG 위젯 BP 생성 |
| `create_data_table` | DataTable 에셋 생성 |
| `create_data_asset` | DataAsset 생성 |
| `inspect_uobject` | 임의 UObject 속성 조회 |

---

## 테스트 원칙
- 모든 Tool은 최소 **성공 1 + 에러 1** 테스트
- Blueprint Tool은 **생성 → 수정 → 컴파일 → 스폰** 전체 플로우 테스트
- UE5 에디터 미실행 시 적절한 에러 메시지 반환 확인
