# Resolves the user's Minecraft 26.3 installation: Java 25, classpath, assets, natives.
# Dot-source or call with -Action info|java|classpath|natives|assets|jardir
param([string]$Action = 'info')

$ErrorActionPreference = 'Stop'
$root = Join-Path $env:APPDATA 'Migurinth'
$meta = Join-Path $root 'meta'
$verDir = Join-Path $meta 'versions\26.3'
$verJsonPath = Join-Path $verDir '26.3.json'
$jar = Join-Path $verDir '26.3.jar'

if (-not (Test-Path $verJsonPath)) { throw "26.3.json not found: $verJsonPath" }
$vj = Get-Content $verJsonPath -Raw | ConvertFrom-Json

$java = Join-Path $meta 'java_versions\zulu25.38.109-ca-jre25.0.5.0.3-win_x64\bin\java.exe'
if (-not (Test-Path $java)) {
    $java = Get-ChildItem (Join-Path $meta 'java_versions') -Recurse -Filter java.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '25' } | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $java) { throw 'Java 25 runtime not found in Migurinth' }

$cp = New-Object System.Collections.Generic.List[string]
foreach ($lib in $vj.libraries) {
    $rel = $null
    if ($lib.downloads -and $lib.downloads.artifact) { $rel = $lib.downloads.artifact.path }
    if ($rel) {
        $p = Join-Path $meta ('libraries\' + ($rel -replace '/', '\'))
        if (Test-Path $p) { $cp.Add($p) }
    }
}
$cp.Add($jar)

$natives = Join-Path $meta 'natives\26.3'
if (-not (Test-Path $natives)) { $natives = Join-Path $meta 'natives\26.3-0.20.1' }
$assets = Join-Path $meta 'assets'

switch ($Action) {
    'java'      { $java }
    'classpath' { ($cp -join ';') }
    'natives'   { $natives }
    'assets'    { $assets }
    'jardir'    { $verDir }
    default {
        [pscustomobject]@{
            Java = $java
            Jar = $jar
            ClasspathEntries = $cp.Count
            Natives = $natives
            Assets = $assets
            VersionDir = $verDir
        } | Format-List | Out-String -Width 240
    }
}
