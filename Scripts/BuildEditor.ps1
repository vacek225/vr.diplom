param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\FindEngine.ps1"
$enginePath = Resolve-KubEngineRoot $EngineRoot
$projectPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'KUB_KiberUniversitetBudushego.uproject'
& "$enginePath\Engine\Build\BatchFiles\Build.bat" KUB_KiberUniversitetBudushegoEditor Win64 Development "-Project=$projectPath" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $LASTEXITCODE" }
