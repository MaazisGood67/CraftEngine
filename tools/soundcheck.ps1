$idx = Get-Content -Raw 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\indexes\34.json' | ConvertFrom-Json
$props = $idx.objects.PSObject.Properties
Write-Output '--- all ui/*.ogg ---'
$props.Name | Where-Object { $_ -like 'minecraft/sounds/ui/*' } | Select-Object -First 40
Write-Output '--- random click ---'
$props.Name | Where-Object { $_ -match 'click' } | Select-Object -First 30
$s = $props | Where-Object { $_.Name -eq 'minecraft/sounds.json' } | Select-Object -First 1
$p = 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\objects\' + $s.Value.hash.Substring(0,2) + '\' + $s.Value.hash
Write-Output ('sounds.json at: ' + $p + ' exists=' + (Test-Path $p))
$t = Get-Content -Raw $p
$i = $t.IndexOf('"ui.button.click"')
Write-Output ('click idx=' + $i)
if ($i -ge 0) { Write-Output $t.Substring($i, 320) }
$i2 = $t.IndexOf('"random.click"')
if ($i2 -ge 0) { Write-Output ('random.click: ' + $t.Substring($i2, 240)) }
