# Runs a Java oracle from tools/oracle against the user's real 26.3 jar.
# Usage: tools/oracle/run.ps1 -Case rng|noise
param([Parameter(Mandatory = $true)][string]$Case)

$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location -LiteralPath $proj
Add-Type -AssemblyName System.IO.Compression.FileSystem
$java = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action java
$cp = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action classpath
$javac = 'C:\Users\Administrator\rea-tools\jdk\bin\javac.exe'
if (-not (Test-Path $javac)) { $javac = (Get-Command javac -ErrorAction Stop).Source }

$names = @{ 'rng' = 'RngOracle'; 'noise' = 'NoiseOracle'; 'df' = 'DfOracle' }
if (-not $names.ContainsKey($Case)) { throw "unknown oracle '$Case' (known: $($names.Keys -join ', '))" }
$className = $names[$Case]
$source = Join-Path $proj ("tools\oracle\{0}.java" -f $className)
$classes = Join-Path $proj '.local\oracle-classes'
$outDir = Join-Path $proj '.local\oracle'
New-Item -ItemType Directory -Force -Path $classes, $outDir | Out-Null

# The noise harness feeds both sides the exact JSON files from the user's jar.
if ($Case -eq 'noise') {
    $jar = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action jar
    $noiseDir = Join-Path $proj '.local\noise'
    New-Item -ItemType Directory -Force -Path $noiseDir | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [System.IO.Compression.ZipFile]::OpenRead($jar)
    try {
        foreach ($e in $z.Entries) {
            if ($e.FullName -like 'data/minecraft/worldgen/noise/*.json') {
                $name = [System.IO.Path]::GetFileName($e.FullName)
                [System.IO.Compression.ZipFileExtensions]::ExtractToFile(
                    $e, (Join-Path $noiseDir $name), $true)
            }
        }
    } finally { $z.Dispose() }
    Write-Output ("noise files: " + (Get-ChildItem $noiseDir -Filter *.json).Count)
}

# The density harness needs the overworld noise_settings as its router source.
if ($Case -eq 'df') {
    $jar = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action jar
    $dfDir = Join-Path $proj '.local\df'
    $z = [System.IO.Compression.ZipFile]::OpenRead($jar)
    try {
        $e = $z.Entries | Where-Object { $_.FullName -eq 'data/minecraft/worldgen/noise_settings/overworld.json' }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $dfDir 'overworld.json'), $true)
    } finally { $z.Dispose() }
    Write-Output "overworld.json -> .local\df\overworld.json"
}

& $javac --release 21 -d $classes (Join-Path $proj 'tools\oracle\MiniJson.java') $source
if ($LASTEXITCODE -ne 0) { throw "javac failed for $className" }

$outPath = Join-Path $outDir ("{0}.json" -f $Case)
$extra = @()
if ($Case -eq 'noise') { $extra = @((Join-Path $proj '.local\noise')) }
& $java -Xmx1G -cp ($classes + ';' + $cp) $className $outPath @extra
if ($LASTEXITCODE -ne 0) { throw "oracle $Case failed" }
Write-Output ("oracle: " + $outPath)
