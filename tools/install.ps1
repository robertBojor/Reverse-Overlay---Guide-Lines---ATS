# Installs (or removes) the ATS Reverse Overlay telemetry plugin.
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-Dll path] [-GameRoot path] [-Uninstall]
# The game only loads telemetry plugins from bin\win_x64\plugins, so that
# folder is created if it does not exist yet.
param(
    [string]$Dll = (Join-Path $PSScriptRoot 'ats_reverse_overlay.dll'),
    [string]$GameRoot,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$pluginName = 'ats_reverse_overlay.dll'

function Find-AtsGameRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
            'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 270880',
            'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 270880')) {
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) { $candidates.Add($location) }
    }
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) { $steam = 'C:\Program Files (x86)\Steam' }
    $libraries = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $vdf) {
        foreach ($line in Get-Content -LiteralPath $vdf) {
            if ($line -match '"path"\s+"([^"]+)"') { $libraries += $Matches[1].Replace('\\', '\') }
        }
    }
    foreach ($library in $libraries) {
        $candidates.Add((Join-Path $library 'steamapps\common\American Truck Simulator'))
    }
    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin\win_x64\amtrucks.exe')) { return $candidate }
    }
    throw 'American Truck Simulator was not found in the Steam libraries. Pass -GameRoot.'
}

try {
    if (Get-Process -Name amtrucks -ErrorAction SilentlyContinue) {
        throw 'American Truck Simulator is running. Exit the game first.'
    }
    if (-not $GameRoot) { $GameRoot = Find-AtsGameRoot }
    $plugins = Join-Path $GameRoot 'bin\win_x64\plugins'
    $target = Join-Path $plugins $pluginName

    if ($Uninstall) {
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
        Write-Host "Removed $target" -ForegroundColor Green
        exit 0
    }

    if (-not (Test-Path -LiteralPath $Dll)) { throw "Plugin not found: $Dll" }
    New-Item -ItemType Directory -Path $plugins -Force | Out-Null
    Copy-Item -LiteralPath $Dll -Destination $target -Force
    Write-Host "Installed $target" -ForegroundColor Green
    $docs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'American Truck Simulator'
    Write-Host "Settings: $(Join-Path $docs 'ats_reverse_overlay.ini') (created on first start)"
    Write-Host "Log:      $(Join-Path $docs 'ats_reverse_overlay.log')"
    Write-Host 'Enable "Reverse Overlay - Guide Lines - ATS" in the Mod Manager and accept the SDK features notice on launch.'
    Write-Host 'In game, Ctrl+Shift+F9 cycles: forward dots only, reverse lines only, both, nothing.'
    exit 0
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
