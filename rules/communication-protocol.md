# communication-protocol — TCP 통신 프로토콜 상세

## 명령어 형식 (Python → UE5)
```json
{
    "id": "uuid-v4",
    "type": "command_name",
    "params": { "key": "value" }
}
```

## 응답 형식 (UE5 → Python)
```json
{
    "id": "matching-uuid",
    "success": true,
    "result": { ... },
    "error": null
}
```

## 에러 응답
```json
{
    "id": "matching-uuid",
    "success": false,
    "result": null,
    "error": {
        "code": "ACTOR_NOT_FOUND",
        "message": "액터 'BP_Player'를 찾을 수 없습니다"
    }
}
```

## TCP 통신 규칙
| 항목 | 값 |
|------|-----|
| 기본 포트 | 13377 |
| 메시지 구분자 | `\n` (newline-delimited JSON) |
| 재연결 간격 | 3초, 최대 10회 |
| 기본 타임아웃 | 30초 |
| 무거운 작업 타임아웃 | 60초 (Blueprint 컴파일 등) |
| 동시 명령어 | 순차 처리 (UE GameThread 제약) |

## Claude Desktop 설정 (claude_desktop_config.json)
```json
{
    "mcpServers": {
        "unreal-mcp": {
            "command": "uv",
            "args": [
                "--directory", "C:/Projects/unreal-mcp/mcp-server",
                "run", "src/unreal_mcp/main.py"
            ]
        }
    }
}
```
