# coding-convention — Python/C++ 코딩 컨벤션 상세

## Python (MCP 서버)

### Tool 정의 패턴
```python
@server.tool("tool_name")
async def tool_name(
    param1: str,
    param2: tuple[float, float, float] = (0, 0, 0),
) -> str:
    """한국어 docstring — Tool 설명.

    Args:
        param1: 파라미터 설명
        param2: (X, Y, Z) 좌표
    """
    command = {
        "type": "tool_name",
        "params": { "param1": param1, "param2": list(param2) }
    }
    result = await send_command(command)
    return json.dumps(result, indent=2)
```

### 필수 사항
- 모든 Tool 함수에 **타입 힌트 + docstring** 필수
- 파라미터 이름은 UE 용어와 일치 (location, rotation, scale)
- 응답은 항상 **JSON 문자열**로 반환
- **async/await** 비동기 패턴 사용
- 에러는 MCP 표준 에러 형식으로 반환

### 파일 구조
```
mcp-server/src/unreal_mcp/
├── main.py            # MCP 서버 진입점
├── connection.py      # TCP 소켓 통신
├── tools/             # Phase별 모듈 분리
│   ├── actor.py       # Phase 1
│   ├── blueprint.py   # Phase 2
│   ├── material.py    # Phase 3
│   ├── asset.py       # Phase 3
│   ├── ai.py          # Phase 4
│   ├── editor.py      # Phase 5
│   └── advanced.py    # Phase 6
└── utils/
    └── validators.py  # 파라미터 검증
```

---

## C++ (UE5 플러그인)

### Epic 코딩 컨벤션
| 접두어 | 의미 | 예시 |
|--------|------|------|
| `A` | Actor | `AMyActor` |
| `U` | UObject | `UMyComponent` |
| `F` | Struct | `FMyStruct` |
| `E` | Enum | `EMyEnum` |
| `I` | Interface | `IMyInterface` |
| `b` | bool 변수 | `bIsValid` |

### 필수 사항
- 모든 `UObject*` 멤버는 `UPROPERTY()`로 마킹
- `IsValid()` 사용 (nullptr만으로는 PendingKill 놓침)
- TCP 리스닝은 별도 스레드, UE API 호출은 반드시 GameThread
- `AsyncTask(ENamedThreads::GameThread, [=]() { ... })` 패턴

### 명령어 처리 패턴
```cpp
void FUnrealMCPModule::HandleCommand(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Command;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Command)) { SendError("Invalid JSON"); return; }

    FString Type = Command->GetStringField("type");
    AsyncTask(ENamedThreads::GameThread, [this, Type, Command]()
    {
        if (Type == "create_actor") HandleCreateActor(Command->GetObjectField("params"));
        // ...
    });
}
```

### 파일 구조
```
Plugins/UnrealMCP/Source/UnrealMCP/
├── Public/
│   ├── UnrealMCPModule.h
│   ├── MCPTcpServer.h
│   └── Handlers/{Actor,Blueprint,Material,AI,Editor}Handler.h
└── Private/
    ├── UnrealMCPModule.cpp
    ├── MCPTcpServer.cpp
    └── Handlers/{Actor,Blueprint,Material,AI,Editor}Handler.cpp
```

---

## 공통
- **한국어 주석**: 코드 주석과 docstring은 한국어로 작성
- **커밋 메시지**: `feat(actor):`, `fix(blueprint):`, `docs(setup):`, `refactor(connection):`, `test(material):`
