function Resolve-KubEngineRoot([string]$RequestedRoot) {
    $candidates = @($RequestedRoot, $env:UE_ROOT)
    $registered = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.7' -ErrorAction SilentlyContinue
    if ($registered) { $candidates += $registered.InstalledDirectory }
    $candidates += @('C:\Program Files\Epic Games\UE_5.7', 'D:\UE_5.7')
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Unreal Engine 5.7 not found. Pass -EngineRoot "path to UE_5.7" or set UE_ROOT.'
}
