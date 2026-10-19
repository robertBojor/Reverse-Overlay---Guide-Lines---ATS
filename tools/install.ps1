# Installs (or removes) the Backing & Lane Prediction telemetry plugin for
# American Truck Simulator and/or Euro Truck Simulator 2. One DLL serves both.
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-Dll path] [-Game all|ats|ets2] [-GameRoot path] [-Uninstall]
# Without -Game it installs into every game it finds. The games only load
# telemetry plugins from bin\win_x64\plugins, so that folder is created if it
# does not exist yet.
param(
    [string]$Dll = (Join-Path $PSScriptRoot 'ats_reverse_overlay.dll'),
    [ValidateSet('all', 'ats', 'ets2')]
    [string]$Game = 'all',
    [string]$GameRoot,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$pluginName = 'ats_reverse_overlay.dll'

$games = @(
    [pscustomobject]@{ Key = 'ats'; Name = 'American Truck Simulator'; AppId = 270880; Exe = 'amtrucks'
        Mod = 'Backing & Lane Prediction - ATS' },
    [pscustomobject]@{ Key = 'ets2'; Name = 'Euro Truck Simulator 2'; AppId = 227300; Exe = 'eurotrucks2'
        Mod = 'Backing & Lane Prediction - ETS2' }
)

function Get-SteamLibraries {
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) { $steam = 'C:\Program Files (x86)\Steam' }
    $libraries = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $vdf) {
        foreach ($line in Get-Content -LiteralPath $vdf) {
            if ($line -match '"path"\s+"([^"]+)"') { $libraries += $Matches[1].Replace('\\', '\') }
        }
    }
    return $libraries
}

function Find-GameRoot($g) {
    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($key in @(
            "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App $($g.AppId)",
            "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Steam App $($g.AppId)")) {
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) { $candidates.Add($location) }
    }
    foreach ($library in Get-SteamLibraries) {
        $candidates.Add((Join-Path $library "steamapps\common\$($g.Name)"))
    }
    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path -LiteralPath (Join-Path $candidate "bin\win_x64\$($g.Exe).exe")) { return $candidate }
    }
    return $null
}

try {
    # Work out which games to touch and where they are.
    $targets = @()
    if ($GameRoot) {
        $g = $games | Where-Object { Test-Path -LiteralPath (Join-Path $GameRoot "bin\win_x64\$($_.Exe).exe") } |
            Select-Object -First 1
        if (-not $g) { throw "No amtrucks.exe or eurotrucks2.exe under $GameRoot\bin\win_x64." }
        $targets += [pscustomobject]@{ Game = $g; Root = $GameRoot }
    }
    else {
        foreach ($g in $games | Where-Object { $Game -eq 'all' -or $_.Key -eq $Game }) {
            $root = Find-GameRoot $g
            if ($root) { $targets += [pscustomobject]@{ Game = $g; Root = $root } }
        }
        if (-not $targets) {
            throw 'Neither American Truck Simulator nor Euro Truck Simulator 2 was found in the Steam libraries. Pass -GameRoot.'
        }
    }
    foreach ($t in $targets) {
        if (Get-Process -Name $t.Game.Exe -ErrorAction SilentlyContinue) {
            throw "$($t.Game.Name) is running. Exit the game first."
        }
    }
    if (-not $Uninstall -and -not (Test-Path -LiteralPath $Dll)) { throw "Plugin not found: $Dll" }

    foreach ($t in $targets) {
        $plugins = Join-Path $t.Root 'bin\win_x64\plugins'
        $target = Join-Path $plugins $pluginName
        if ($Uninstall) {
            if (Test-Path -LiteralPath $target) {
                Remove-Item -LiteralPath $target -Force
                Write-Host "$($t.Game.Name): removed $target" -ForegroundColor Green
            }
            else {
                Write-Host "$($t.Game.Name): not installed"
            }
            continue
        }
        New-Item -ItemType Directory -Path $plugins -Force | Out-Null
        Copy-Item -LiteralPath $Dll -Destination $target -Force
        $docs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) $t.Game.Name
        Write-Host "$($t.Game.Name): installed $target" -ForegroundColor Green
        Write-Host "  Settings: $(Join-Path $docs 'ats_reverse_overlay.ini') (created on first start)"
        Write-Host "  Log:      $(Join-Path $docs 'ats_reverse_overlay.log')"
        Write-Host "  Enable `"$($t.Game.Mod)`" in the Mod Manager and accept the SDK features notice on launch."
    }
    if (-not $Uninstall) {
        Write-Host 'In game, Ctrl+Shift+F9 cycles: forward line only, reverse lines only, both, nothing.'
    }
    exit 0
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
