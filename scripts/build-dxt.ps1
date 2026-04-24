# build-dxt.ps1
# UnrealMCP .dxt 패키지 빌드 스크립트
#
# 동작:
#   1. mcp-server/src 소스 복사
#   2. pip install -t lib/  로 의존성 벤더링
#   3. manifest.json 배치
#   4. dist/unreal-mcp-<version>.dxt 로 zip 생성
#
# 사용:
#   powershell -ExecutionPolicy Bypass -File scripts\build-dxt.ps1

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$McpServer = Join-Path $RepoRoot "mcp-server"
$ManifestSrc = Join-Path $McpServer "dxt\manifest.json"
$SrcDir = Join-Path $McpServer "src"

# 스테이징 / 출력 폴더
$BuildDir = Join-Path $RepoRoot "build\dxt-staging"
$DistDir = Join-Path $RepoRoot "dist"

# 매니페스트에서 버전 읽기
$manifest = Get-Content $ManifestSrc -Raw | ConvertFrom-Json
$version = $manifest.version
$name = $manifest.name
$dxtFile = Join-Path $DistDir "$name-$version.dxt"

Write-Host "[build-dxt] $name v$version"
Write-Host "[build-dxt] staging: $BuildDir"
Write-Host "[build-dxt] output : $dxtFile"

# 1) 스테이징 초기화
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

# 2) 소스 복사 (__pycache__ 제외)
Write-Host "[build-dxt] copying src..."
$destSrc = Join-Path $BuildDir "src"
Copy-Item -Recurse -Path $SrcDir -Destination $destSrc
Get-ChildItem -Path $destSrc -Include "__pycache__" -Recurse -Directory | Remove-Item -Recurse -Force

# 3) 의존성 벤더링 (mcp, anyio, 그 하위 deps 포함)
Write-Host "[build-dxt] vendoring dependencies to lib/..."
$libDir = Join-Path $BuildDir "lib"
# PowerShell 5.1: 2>&1 금지 - pip stderr가 NativeCommandError로 래핑되어 Stop 트리거됨
& python -m pip install --target $libDir --no-compile --quiet "mcp>=1.0.0" "anyio>=4.0.0"
if ($LASTEXITCODE -ne 0) {
    throw "pip install failed (exit $LASTEXITCODE)"
}
Get-ChildItem -Path $libDir -Include "__pycache__" -Recurse -Directory | Remove-Item -Recurse -Force
Get-ChildItem -Path $libDir -Filter "*.dist-info" -Directory | Remove-Item -Recurse -Force

# 4) manifest.json 복사
Copy-Item $ManifestSrc (Join-Path $BuildDir "manifest.json")

# 5) .dxt 생성 (zip)
if (Test-Path $dxtFile) {
    Remove-Item $dxtFile
}
Write-Host "[build-dxt] creating .dxt..."
# PS 5.1 Compress-Archive는 .dxt 확장자 거부 → .zip 생성 후 rename
$tmpZip = "$dxtFile.zip"
if (Test-Path $tmpZip) { Remove-Item $tmpZip }
Compress-Archive -Path "$BuildDir\*" -DestinationPath $tmpZip -CompressionLevel Optimal
Move-Item $tmpZip $dxtFile -Force

$size = (Get-Item $dxtFile).Length / 1MB
Write-Host ""
Write-Host "[build-dxt] DONE: $dxtFile ($([math]::Round($size,2)) MB)"
Write-Host ""
Write-Host "설치: 파일 탐색기에서 $dxtFile 더블클릭 → Claude Desktop이 설치 확인창 표시"
