# Builds the console probe used by the differential harnesses (no DX10 dependency).
$proj = "C:/Users/Administrator/Desktop/REA Minecraaft C++"
$gpp = "C:/Users/Administrator/rea-tools/w64devkit/w64devkit/bin/g++.exe"
Set-Location -LiteralPath $proj
New-Item -ItemType Directory -Force -Path (Join-Path $proj '.local\probe') | Out-Null
& $gpp -std=c++20 -O2 -I src tools/probe/probe.cpp src/rng.cpp -o CraftEngineProbe.exe -static -s
Write-Output ("PROBE EXIT: $LASTEXITCODE")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
