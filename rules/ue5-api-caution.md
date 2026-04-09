# ue5-api-caution — UE5 API 사용 시 주의사항

## Blueprint 그래프 프로그래밍
- K2Node 타입은 UE 버전에 따라 다를 수 있음 → 대상 엔진 버전(5.5+) 확인 필수
- 노드 추가 후 반드시 `CompileBlueprint()` 호출
- Event 노드(BeginPlay, Tick)는 **EventGraph에만** 추가 가능
- Function Graph에는 FunctionEntry/FunctionResult 노드가 자동 생성됨
- 핀 이름은 **대소문자 구분** (OutputPin, ReturnValue 등)

## Actor 조작
- 에디터 모드 전용 Tool vs PIE 중에도 작동하는 Tool을 명확히 구분
- 에디터 월드 접근: `GEditor->GetEditorWorldContext().World()`
- **Undo 지원**: `GEditor->BeginTransaction()` / `EndTransaction()` 으로 트랜잭션 시스템 활용
- 트랜잭션 안에서 `Modify()` 호출 후 속성 변경

## Material 조작
- Material Expression 연결 후 반드시 `Material->PreEditChange()` / `PostEditChange()` 호출
- Material Instance는 **부모 Material의 파라미터만** 오버라이드 가능
- Static Switch Parameter는 런타임 변경 불가

## GameThread 규칙 (CRITICAL)
- TCP 리스닝은 별도 스레드에서 수행
- **모든 UE API 호출은 반드시 GameThread에서** 실행
- 패턴: `AsyncTask(ENamedThreads::GameThread, [=]() { /* UE API */ });`
- GameThread 밖에서 UE API 호출 시 → **크래시 또는 정의되지 않은 동작**

## UE 버전 대응
- 대상: Unreal Engine 5.5+
- 버전별 API 차이가 있을 수 있으므로 조건부 컴파일 고려
- `#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5`

## 참고 자료
- GenOrca/unreal-mcp: https://github.com/GenOrca/unreal-mcp
- flopperam Blueprint Guide: https://github.com/flopperam/unreal-engine-mcp/blob/main/Guides/blueprint-graph-guide.md
- UE5 API Reference: https://dev.epicgames.com/documentation
- MCP 공식 스펙: https://modelcontextprotocol.io
