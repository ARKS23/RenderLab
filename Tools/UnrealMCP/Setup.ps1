param(
    [string]$EngineRoot = 'E:\ue\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {
    throw 'RenderLab MCP requires Unreal Engine 5.8.'
}
$python = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$venv = Join-Path $PSScriptRoot '.venv'
if (-not (Test-Path -LiteralPath (Join-Path $venv 'Scripts\python.exe'))) {
    & $python -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Virtual environment creation failed.' }
}
$requirements = Join-Path $PSScriptRoot 'requirements-lock.txt'
if (-not (Test-Path -LiteralPath $requirements)) {
    $requirements = Join-Path $PSScriptRoot 'requirements.txt'
}
& (Join-Path $venv 'Scripts\python.exe') -m pip install --disable-pip-version-check -r $requirements
if ($LASTEXITCODE -ne 0) { throw 'MCP dependency installation failed.' }
& (Join-Path $venv 'Scripts\python.exe') -m pip check
if ($LASTEXITCODE -ne 0) { throw 'MCP dependency consistency check failed.' }
Write-Output 'MCP dependencies installed in the project virtual environment only.'
