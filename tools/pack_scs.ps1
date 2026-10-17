# Zips out/mod_stage into out/ats_reverse_overlay.scs with .NET's ZipFile
# (deflate, forward-slash entry names), matching packages the game accepts.
param(
    [Parameter(Mandatory)] [string]$Stage,
    [Parameter(Mandatory)] [string]$Destination
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Force }
$root = (Resolve-Path -LiteralPath $Stage).Path.TrimEnd('\') + '\'
$zip = [IO.Compression.ZipFile]::Open($Destination, [IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem -LiteralPath $Stage -Recurse -File | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($root.Length).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $relative,
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}
finally { $zip.Dispose() }
Write-Host "wrote $Destination"
