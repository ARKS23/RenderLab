param(
    [string]$EngineRoot = 'E:\ue\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$project = Join-Path $root 'RenderLab.uproject'
$version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {
    throw 'RenderLab MCP must be built with Unreal Engine 5.8.'
}
$editors = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'"
if ($editors | Where-Object { $_.CommandLine -like "*$project*" }) {
    throw 'Close RenderLab Editor normally before building. Do not discard unsaved work.'
}
$logs = Join-Path $root 'Saved\UnrealMCP'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') RenderLabEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE "-Log=$(Join-Path $logs 'Build.log')"
if ($LASTEXITCODE -ne 0) { throw "UE 5.8 build failed with exit code $LASTEXITCODE." }
