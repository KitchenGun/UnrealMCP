# dev-link.ps1
# DXT 설치본의 src/ 폴더를 원본 mcp-server/src/ 에 심볼릭 링크로 연결.
#
# 효과: 원본 파일 수정 → Claude Desktop 재시작만 하면 즉시 반영 (재빌드 불필요).
#
# 사용:
#   1. 최초 1회: scripts\build-dxt.ps1 실행 → .dxt 생성 → 더블클릭 설치
#   2. 설치 후: 이 스크립트를 관리자 권한 PowerShell에서 실행
#   3. 이후: 원본 수정 → Claude Desktop 재시작 → 반영
#
# 주의: Windows에서 mklink /D 는 관리자 권한 필요 (또는 개발자 모드 활성화).

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$OrigSrc = Join-Path $RepoRoot "mcp-server\src"

# Claude Desktop이 설치한 확장 경로 탐색
$ExtRoot = Join-Path $env:APPDATA "Claude\Claude Extensions"
$candidates = Get-ChildItem -Path $ExtRoot -Directory -ErrorAction SilentlyContinue | Where-Object {
    $_.Name -like "*unreal*" -or $_.Name -eq "unreal-mcp"
}

if ($candidates.Count -eq 0) {
    Write-Host "[dev-link] 설치된 unreal-mcp 확장을 찾을 수 없음."
    Write-Host "  먼저 .dxt를 설치하세요: scripts\build-dxt.ps1 실행 후 dist\*.dxt 더블클릭"
    exit 1
}
if ($candidates.Count -gt 1) {
    Write-Host "[dev-link] 후보 여러 개:"
    $candidates | ForEach-Object { Write-Host "  - $($_.FullName)" }
    exit 1
}

$installedDir = $candidates[0].FullName
$installedSrc = Join-Path $installedDir "src"

Write-Host "[dev-link] installed extension: $installedDir"
Write-Host "[dev-link] origin src          : $OrigSrc"

# 기존 src 백업 또는 제거
if (Test-Path $installedSrc) {
    $isReparse = (Get-Item $installedSrc).Attributes -band [System.IO.FileAttributes]::ReparsePoint
    if ($isReparse) {
        Write-Host "[dev-link] 이미 심볼릭 링크. 제거 후 재생성."
        cmd /c "rmdir `"$installedSrc`""
    } else {
        $backup = "$installedSrc.bak"
        if (Test-Path $backup) { Remove-Item -Recurse -Force $backup }
        Write-Host "[dev-link] 원본 폴더 백업: $backup"
        Move-Item $installedSrc $backup
    }
}

# 심볼릭 링크 생성
cmd /c "mklink /D `"$installedSrc`" `"$OrigSrc`""
if ($LASTEXITCODE -ne 0) {
    throw "mklink 실패. 관리자 권한 또는 개발자 모드(설정→개발자용)가 필요합니다."
}

Write-Host ""
Write-Host "[dev-link] 링크 완료. Claude Desktop을 재시작하면 원본 수정이 즉시 반영됩니다."
