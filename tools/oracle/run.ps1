# Runs a Java oracle from tools/oracle against the user's real 26.3 jar.
# Usage: tools/oracle/run.ps1 -Case rng
param([Parameter(Mandatory = $true)][string]$Case)

$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$java = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action java
$cp = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action classpath
$javac = 'C:\Users\Administrator\rea-tools\jdk\bin\javac.exe'
if (-not (Test-Path $javac)) { $javac = (Get-Command javac -ErrorAction Stop).Source }

$names = @{ 'rng' = 'RngOracle' }
if (-not $names.ContainsKey($Case)) { throw "unknown oracle '$Case' (known: $($names.Keys -join ', '))" }
$className = $names[$Case]
$source = Join-Path $proj ("tools\oracle\{0}.java" -f $className)
$classes = Join-Path $proj '.local\oracle-classes'
$outDir = Join-Path $proj '.local\oracle'
New-Item -ItemType Directory -Force -Path $classes, $outDir | Out-Null

& $javac --release 21 -d $classes $source
if ($LASTEXITCODE -ne 0) { throw "javac failed for $className" }

$outPath = Join-Path $outDir ("{0}.json" -f $Case)
& $java -Xmx1G -cp ($classes + ';' + $cp) $className $outPath
if ($LASTEXITCODE -ne 0) { throw "oracle $Case failed" }
Write-Output ("oracle: " + $outPath)
