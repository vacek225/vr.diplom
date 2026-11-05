param([string]$EngineRoot, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\FindEngine.ps1"
$enginePath = Resolve-KubEngineRoot $EngineRoot
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectPath = Join-Path $projectRoot 'KUB_KiberUniversitetBudushego.uproject'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'Builds\Windows' }
& "$enginePath\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$projectPath" -noP4 -platform=Win64 -clientconfig=Development -build -nocompileeditor -cook -stage -pak -iostore -archive "-archivedirectory=$OutputDirectory" '-AdditionalCookerOptions=-ini:Engine:[/Script/WindowsTargetPlatform.WindowsTargetSettings]:-D3D11TargetedShaderFormats=PCD3D_SM5' -unattended -utf8output
if ($LASTEXITCODE -ne 0) { throw "Packaging failed: $LASTEXITCODE" }
