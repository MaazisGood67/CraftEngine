# CraftEngine differential harness runner.
#
#   tools/parity.ps1 run rng        # Java oracle + C++ probe + compare
#   tools/parity.ps1 compare rng    # compare existing JSON files
#   tools/parity.ps1 status         # show every harness result
#
# A mismatch exits 1 and prints the first differing paths. Never weaken a
# harness to pass: fix the port or record the real vanilla rule.
param(
    [Parameter(Position = 0)][string]$Command = 'status',
    [Parameter(Position = 1)][string]$Case = ''
)

$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent $PSScriptRoot

function Compare-Node($a, $b, [string]$path, [System.Collections.Generic.List[string]]$diffs) {
    if ($null -eq $a -and $null -eq $b) { return }
    if ($null -eq $a -or $null -eq $b) {
        $diffs.Add("$path : null mismatch ($a vs $b)")
        return
    }
    if ($a -is [System.Management.Automation.PSCustomObject] -and
        $b -is [System.Management.Automation.PSCustomObject]) {
        $an = @($a.PSObject.Properties.Name)
        $bn = @($b.PSObject.Properties.Name)
        foreach ($n in $an) {
            if ($bn -notcontains $n) { $diffs.Add("$path.$n : only in oracle"); continue }
            Compare-Node $a.$n $b.$n "$path.$n" $diffs
        }
        foreach ($n in $bn) {
            if ($an -notcontains $n) { $diffs.Add("$path.$n : only in probe") }
        }
        return
    }
    if ($a -is [System.Array] -or $b -is [System.Array]) {
        $aa = @($a); $bb = @($b)
        if ($aa.Count -ne $bb.Count) {
            $diffs.Add("$path : length $($aa.Count) vs $($bb.Count)")
            return
        }
        for ($i = 0; $i -lt $aa.Count; $i++) { Compare-Node $aa[$i] $bb[$i] "$path[$i]" $diffs }
        return
    }
    if ("$a" -ne "$b") {
        $diffs.Add("$path : '$a' vs '$b'")
    }
}

function Compare-Case([string]$name) {
    $oracle = Join-Path $proj (".local\oracle\{0}.json" -f $name)
    $probe = Join-Path $proj (".local\probe\{0}.json" -f $name)
    if (-not (Test-Path $oracle)) { Write-Host "missing oracle: $oracle"; return $false }
    if (-not (Test-Path $probe)) { Write-Host "missing probe: $probe"; return $false }
    $a = Get-Content $oracle -Raw | ConvertFrom-Json
    $b = Get-Content $probe -Raw | ConvertFrom-Json
    $diffs = New-Object System.Collections.Generic.List[string]
    Compare-Node $a $b '$' $diffs
    if ($diffs.Count -eq 0) {
        Write-Host ("PASS {0} ({1:N0} bytes oracle, {2:N0} bytes probe)" -f
            $name, (Get-Item $oracle).Length, (Get-Item $probe).Length)
        return $true
    }
    Write-Host ("FAIL {0}: {1} difference(s)" -f $name, $diffs.Count)
    $diffs | Select-Object -First 25 | ForEach-Object { Write-Host ("  " + $_) }
    return $false
}

switch ($Command.ToLower()) {
    'run' {
        if (-not $Case) { throw 'usage: parity.ps1 run <case>' }
        & (Join-Path $PSScriptRoot 'oracle\run.ps1') -Case $Case
        if ($LASTEXITCODE -ne 0) { exit 1 }
        & (Join-Path $PSScriptRoot 'build-probe.ps1')
        if ($LASTEXITCODE -ne 0) { exit 1 }
        $probeExe = Join-Path $proj 'CraftEngineProbe.exe'
        $probeArgs = @('--probe', $Case, '--out', (Join-Path $proj (".local\probe\{0}.json" -f $Case)))
        if ($Case -eq 'noise') { $probeArgs += @('--noise-dir', (Join-Path $proj '.local\noise')) }
        & $probeExe @probeArgs
        if ($LASTEXITCODE -ne 0) { exit 1 }
        $ok = Compare-Case $Case
        if (-not $ok) { exit 1 }
    }
    'compare' {
        if (-not $Case) { throw 'usage: parity.ps1 compare <case>' }
        $ok = Compare-Case $Case
        if (-not $ok) { exit 1 }
    }
    'status' {
        $oracleDir = Join-Path $proj '.local\oracle'
        if (-not (Test-Path $oracleDir)) { Write-Host 'no harnesses have run yet'; exit 0 }
        $names = Get-ChildItem $oracleDir -Filter *.json | Select-Object -ExpandProperty BaseName
        foreach ($n in $names) { $null = Compare-Case $n }
    }
    default { throw "unknown command '$Command' (run|compare|status)" }
}
