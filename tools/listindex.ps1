$root = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta"
$versionJson = Get-Content "$root\versions\26.3\26.3.json" -Raw | ConvertFrom-Json
$indexId = $versionJson.assetIndex.id
Write-Output "assetIndex id: $indexId"
$idx = Get-Content "$root\assets\indexes\$indexId.json" -Raw | ConvertFrom-Json
$keys = $idx.objects.PSObject.Properties.Name
Write-Output "total index entries: $($keys.Count)"
Write-Output "=== sounds matching ui/click/hover/levelup/pop ==="
$keys | Where-Object { $_ -match 'sounds/(ui|random)/(click|button|hover|levelup|pop)' } | Select-Object -First 40
Write-Output "=== menu music ==="
$keys | Where-Object { $_ -match 'sounds/music/menu' } | Select-Object -First 20
Write-Output "=== progress/loading gui textures ==="
$keys | Where-Object { $_ -match 'textures/gui/.*(progress|loading|bars)' } | Select-Object -First 40
Write-Output "=== title panorama overlay ==="
$keys | Where-Object { $_ -match 'panorama_overlay|title/(minecraft|edition|mojangstudios)' } | Select-Object -First 10
Write-Output "=== check objects exist on disk ==="
foreach ($k in @('minecraft/sounds/random/click.ogg','minecraft/sounds/ui/button/click.ogg','minecraft/textures/gui/title/background/panorama_0.png')) {
    $o = $idx.objects.$k
    if ($null -eq $o) { Write-Output "$k -> NOT IN INDEX"; continue }
    $h = $o.hash
    $p = "$root\assets\objects\$($h.Substring(0,2))\$h"
    Write-Output "$k -> $(Test-Path $p) $p"
}
