param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\FindEngine.ps1"
$enginePath = Resolve-KubEngineRoot $EngineRoot
& "$PSScriptRoot\BuildEditor.ps1" -EngineRoot $enginePath
$projectPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'KUB_KiberUniversitetBudushego.uproject'
& "$enginePath\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath -log
