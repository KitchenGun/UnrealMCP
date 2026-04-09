# error-handling — 에러 코드 체계 및 메시지 원칙

## 에러 코드 체계

| 코드 | 의미 | 대응 |
|------|------|------|
| `CONNECTION_FAILED` | UE5 플러그인 연결 실패 | 에디터 실행 여부 + 플러그인 활성화 확인 안내 |
| `CONNECTION_TIMEOUT` | 명령어 응답 타임아웃 | 작업 복잡도 확인, 타임아웃 연장 고려 |
| `INVALID_PARAMS` | 파라미터 형식/값 오류 | 올바른 파라미터 형식 예시 제공 |
| `ACTOR_NOT_FOUND` | 지정 액터 없음 | 액터 이름 확인, `get_actors_in_level`로 조회 안내 |
| `ASSET_NOT_FOUND` | 지정 에셋 없음 | 에셋 경로 확인, `search_assets`로 검색 안내 |
| `BLUEPRINT_COMPILE_ERROR` | BP 컴파일 실패 | 컴파일 에러 상세 메시지 전달 |
| `PERMISSION_DENIED` | 에디터 상태로 인한 작업 불가 | PIE 중지 등 상태 전환 안내 |
| `INTERNAL_ERROR` | UE API 내부 오류 | 로그 확인 안내, 재시도 권장 |

## 사용자 친화적 에러 메시지 원칙

1. **무엇이 잘못되었는지** 명확히 설명
2. **가능한 해결 방법** 제시
3. **에디터 상태** 관련 에러는 상태 전환 방법 안내 (PIE 실행 중 등)

## 에러 응답 예시
```json
{
    "success": false,
    "error": {
        "code": "ACTOR_NOT_FOUND",
        "message": "액터 'BP_Player'를 찾을 수 없습니다. get_actors_in_level로 현재 레벨의 액터 목록을 확인하세요."
    }
}
```
