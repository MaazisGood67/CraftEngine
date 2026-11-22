# Queries tools/port_map.tsv: which vanilla 26.3 method a C++ function ports.
# Usage: port-ledger.ps1 status | where <Class.method> | class <Class> | gaps | missing | check
param([string]$Command = 'status', [string]$Term = '')

$ErrorActionPreference = 'Stop'
$mapPath = Join-Path $PSScriptRoot 'port_map.tsv'
$proj = Split-Path -Parent $PSScriptRoot
$rows = Get-Content $mapPath | Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object {
    $f = $_ -split "`t"
    if ($f.Count -lt 6 -or $f[4] -eq 'status') { return }
    [pscustomobject]@{ Class = $f[0]; Method = $f[1]; File = $f[2]; Symbol = $f[3]; Status = $f[4]; Notes = $f[5] }
}

function Show-Row($r) {
    '{0,-6} {1}.{2}' -f $r.Status, $r.Class, $r.Method
    '       -> {0}::{1}' -f $r.File, $r.Symbol
    if ($r.Notes) { '       {0}' -f $r.Notes }
}

switch ($Command.ToLower()) {
    'status' {
        Write-Output ("port ledger: {0} rows" -f $rows.Count)
        $rows | Group-Object Status | Sort-Object Name | ForEach-Object {
            Write-Output ("  {0,-8} {1}" -f $_.Name, $_.Count)
        }
        $missing = @($rows | Where-Object { $_.Status -in @('missing', 'partial') })
        Write-Output ("unfinished: {0} (use 'port-ledger.ps1 gaps')" -f $missing.Count)
    }
    'where' {
        if (-not $Term) { throw 'usage: port-ledger.ps1 where <Class.method>' }
        $hits = @($rows | Where-Object { ("$($_.Class).$($_.Method)") -like "*$Term*" })
        if ($hits.Count -eq 0) { Write-Output "no ledger row matches '$Term' (try 'class <Name>')"; exit 0 }
        foreach ($r in $hits) { Show-Row $r; '' }
    }
    'class' {
        if (-not $Term) { throw 'usage: port-ledger.ps1 class <ClassName>' }
        $hits = @($rows | Where-Object { $_.Class -like "*$Term*" })
        if ($hits.Count -eq 0) { Write-Output "no ledger row for class '$Term'"; exit 0 }
        foreach ($r in $hits) { Show-Row $r; '' }
    }
    'gaps' {
        $hits = @($rows | Where-Object { $_.Status -in @('partial', 'seam') })
        Write-Output ("gaps: {0}" -f $hits.Count)
        foreach ($r in $hits) { Show-Row $r; '' }
    }
    'missing' {
        $hits = @($rows | Where-Object { $_.Status -eq 'missing' })
        Write-Output ("missing ports: {0}" -f $hits.Count)
        foreach ($r in $hits) { Show-Row $r; '' }
    }
    'check' {
        $bad = 0
        foreach ($r in $rows) {
            $p = Join-Path $proj ($r.File -replace '/', '\')
            if (-not (Test-Path $p)) { Write-Output ("MISSING FILE {0} ({1}.{2})" -f $r.File, $r.Class, $r.Method); $bad++ }
        }
        Write-Output ("checked {0} rows, {1} broken file references" -f $rows.Count, $bad)
        if ($bad -gt 0) { exit 1 }
    }
    default { throw "unknown command '$Command' (status|where|class|gaps|missing|check)" }
}
