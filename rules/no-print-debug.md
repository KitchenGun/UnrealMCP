# no-print-debug

## 발생일
2026-04-06

## 실패 상황
`print("debug ...")` 코드를 남긴 채 배포 → 프로덕션 로그 오염

## 규칙
- `print("debug` / `print("test` / `print("TODO` 등 디버그용 print 금지
- 로깅이 필요하면 `logging` 모듈 사용

## 자동 교정
`--fix` 모드에서 해당 라인을 `# REMOVED:` 주석으로 변환

## 관련 파일
- `rules/no-print-debug.sh` — 자동 검사 스크립트
