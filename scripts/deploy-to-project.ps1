# deploy-to-project.ps1
# UnrealMCP 본체(E:\UnrealMCP)를 대상 UE5 프로젝트에 심볼릭 링크로 연결.
#
# 동작:
#   1. 대상 프로젝트의 Plugins\UnrealMCP\Source 를 본체의 Source 로 symlink
#   2. 대상 프로젝트의 Plugins\UnrealMCP\UnrealMCP.uplugin 도 symlink
#   3. 대상 프로젝트 루트에 .mcp.json 생성 (Claude Code용)
#
# 효과:
#   - UnrealMCP 본체의 플러그인/Python 서버를 수정하면 대상 프로젝트에 즉시 반영
#   - 대상 프로젝트는 Binaries/Intermediate만 로컬에 보유 (UE5 컴파일 산출물)
#
# 사용:
#   powershell -ExecutionPolicy Bypass -File scripts\deploy-to-project.ps1 -ProjectPath E:\DualFire
#
# 주의: Windows 심볼릭 링크는 관리자 권한 또는 개발자 모드(설정→개발자용) 필요.

param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectPath
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$PluginSrc = Join-Path $RepoRoot "Plugins\UnrealMCP"
$McpServerSrc = Join-Path $RepoRoot "mcp-server\src"

if (-not (Test-Path $ProjectPath)) {
    throw "프로젝트 경로가 존재하지 않음: $ProjectPath"
}

$uprojectFiles = Get-ChildItem -Path $ProjectPath -Filter "*.uproject" -File
if ($uprojectFiles.Count -eq 0) {
    throw "UE5 프로젝트가 아님 (.uproject 없음): $ProjectPath"
}

Write-Host "[deploy] source       : $RepoRoot"
Write-Host "[deploy] target       : $ProjectPath"
Write-Host "[deploy] uproject     : $($uprojectFiles[0].Name)"

# 1) Plugins\UnrealMCP 디렉토리 준비
$targetPluginsDir = Join-Path $ProjectPath "Plugins"
$targetPluginDir = Join-Path $targetPluginsDir "UnrealMCP"

if (-not (Test-Path $targetPluginsDir)) {
    New-Item -ItemType Directory -Path $targetPluginsDir | Out-Null
}
if (-not (Test-Path $targetPluginDir)) {
    New-Item -ItemType Directory -Path $targetPluginDir | Out-Null
}

# 2) Source\ 심볼릭 링크
$targetSrcDir = Join-Path $targetPluginDir "Source"
$sourceSrcDir = Join-Path $PluginSrc "Source"

if (Test-Path $targetSrcDir) {
    $isReparse = (Get-Item $targetSrcDir).Attributes -band [System.IO.FileAttributes]::ReparsePoint
    if ($isReparse) {
        Write-Host "[deploy] 기존 Source symlink 제거 후 재생성"
        cmd /c "rmdir `"$targetSrcDir`""
    } else {
        $backup = "$targetSrcDir.bak"
        if (Test-Path $backup) { Remove-Item -Recurse -Force $backup }
        Write-Host "[deploy] 기존 Source 폴더 백업: $backup"
        Move-Item $targetSrcDir $backup
    }
}

cmd /c "mklink /J `"$targetSrcDir`" `"$sourceSrcDir`""
if ($LASTEXITCODE -ne 0) {
    throw "Source junction 실패."
}
Write-Host "[deploy] Source junction: $targetSrcDir -> $sourceSrcDir"

# 3) UnrealMCP.uplugin 하드링크 (파일, 관리자 권한 불필요)
$targetUplugin = Join-Path $targetPluginDir "UnrealMCP.uplugin"
$sourceUplugin = Join-Path $PluginSrc "UnrealMCP.uplugin"

if (Test-Path $targetUplugin) {
    Remove-Item $targetUplugin -Force
}
cmd /c "mklink /H `"$targetUplugin`" `"$sourceUplugin`""
if ($LASTEXITCODE -ne 0) {
    throw "uplugin hard link 실패."
}
Write-Host "[deploy] uplugin hard link: $targetUplugin -> $sourceUplugin"

# 4) .mcp.json 생성 (Claude Code용, 프로젝트 루트)
$mcpJsonPath = Join-Path $ProjectPath ".mcp.json"

$mcpConfig = [ordered]@{
    mcpServers = [ordered]@{
        "Unreal_Engine_MCP" = [ordered]@{
            command = "python"
            args = @("-m", "unreal_mcp.main")
            env = [ordered]@{
                PYTHONPATH = $McpServerSrc
            }
        }
    }
}

$mcpJson = $mcpConfig | ConvertTo-Json -Depth 10
Set-Content -Path $mcpJsonPath -Value $mcpJson -Encoding UTF8
Write-Host "[deploy] .mcp.json written: $mcpJsonPath"

Write-Host ""
Write-Host "[deploy] DONE."
Write-Host ""
Write-Host "다음 단계:"
Write-Host "  1. UE5 에디터에서 프로젝트 열기 → 필요시 플러그인 리빌드"
Write-Host "  2. Claude Code: 프로젝트 디렉토리에서 실행 → /mcp 로 unreal-mcp 확인"
Write-Host "  3. Claude Desktop: scripts\build-dxt.ps1 실행 후 .dxt 설치 (별도)"
