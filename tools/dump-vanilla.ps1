# Extracts CraftEngine's vanilla reference data from the user's own 26.3 install.
# Outputs are gitignored: vanilla/ (runtime) and .local/vanilla/ (reference).
param([switch]$SkipOracle, [switch]$SkipWorldgen)

$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent $PSScriptRoot
$envInfo = & (Join-Path $PSScriptRoot 'vanilla-env.ps1') -Action info | Out-String
Write-Output $envInfo.Trim()

$java = & (Join-Path $PSScriptRoot 'vanilla-env.ps1') -Action java
$cp = & (Join-Path $PSScriptRoot 'vanilla-env.ps1') -Action classpath
$jar = Join-Path $env:APPDATA 'Migurinth\meta\versions\26.3\26.3.jar'
$javac = 'C:\Users\Administrator\rea-tools\jdk\bin\javac.exe'
if (-not (Test-Path $javac)) { $javac = (Get-Command javac -ErrorAction SilentlyContinue).Source }

New-Item -ItemType Directory -Force -Path (Join-Path $proj 'vanilla') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $proj '.local\vanilla') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $proj '.local\oracle-classes') | Out-Null

if (-not $SkipOracle) {
    Write-Output '== compiling oracle (reflection only) =='
    & $javac --release 21 -d (Join-Path $proj '.local\oracle-classes') (Join-Path $PSScriptRoot 'oracle\VanillaDump.java')
    if ($LASTEXITCODE -ne 0) { throw 'oracle compile failed' }

    Write-Output '== running oracle against the real 26.3 jar =='
    $oracleCp = (Join-Path $proj '.local\oracle-classes') + ';' + $cp
    $outJson = Join-Path $proj 'vanilla\registry.json'
    & $java -Xmx1G -cp $oracleCp VanillaDump $outJson
    if ($LASTEXITCODE -ne 0) { throw 'oracle run failed' }
    Copy-Item -Force $outJson (Join-Path $proj '.local\vanilla\registry.json')
}

if (-not $SkipWorldgen) {
    Write-Output '== extracting worldgen data for reference =='
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [System.IO.Compression.ZipFile]::OpenRead($jar)
    $targets = $z.Entries | Where-Object {
        $_.FullName -like 'data/minecraft/worldgen/noise_settings/*.json' -or
        $_.FullName -like 'data/minecraft/worldgen/noise/*.json' -or
        $_.FullName -like 'data/minecraft/worldgen/density_function/*.json' -or
        $_.FullName -like 'data/minecraft/worldgen/multi_noise_biome_source_parameter_list/*.json' -or
        $_.FullName -like 'data/minecraft/dimension_type/*.json'
    }
    $count = 0
    foreach ($e in $targets) {
        if ($e.FullName.EndsWith('/')) { continue }
        $rel = $e.FullName -replace '^data/minecraft/', ''
        $dest = Join-Path $proj ('.local\vanilla\' + ($rel -replace '/', '\'))
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest, $true)
        $count++
    }
    $z.Dispose()
    Write-Output "worldgen files: $count"
}

# Keybinds: vanilla writes every key mapping to options.txt on first run.
$opt = Get-ChildItem (Join-Path $env:APPDATA 'Migurinth') -Recurse -Filter options.txt -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($opt) {
    Copy-Item -Force $opt.FullName (Join-Path $proj '.local\vanilla\options.txt')
    Copy-Item -Force $opt.FullName (Join-Path $proj 'vanilla\options.txt')
    Write-Output ("options.txt: " + $opt.FullName)
}

$reg = Join-Path $proj 'vanilla\registry.json'
if (Test-Path $reg) {
    $j = Get-Content $reg -Raw | ConvertFrom-Json
    $regs = $j.registries.PSObject.Properties
    Write-Output ("registries: " + $regs.Count)
    foreach ($r in $regs) { Write-Output ("  {0,-42} {1}" -f $r.Name, $r.Value.Count) }
}
