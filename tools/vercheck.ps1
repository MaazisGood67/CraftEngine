$j = Get-Content -Raw 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.json' | ConvertFrom-Json
Write-Output ('id=' + $j.id)
Write-Output ('assetIndex=' + $j.assetIndex.id)
Write-Output ('assets=' + $j.assets)
Write-Output ('mainClass=' + $j.mainClass)
$idx = Get-Content -Raw 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\indexes\34.json' | ConvertFrom-Json
$props = $idx.objects.PSObject.Properties
Write-Output ("objectCount=" + @($props).Count)
foreach ($n in @('minecraft/textures/gui/widgets.png','minecraft/textures/font/ascii.png','minecraft/sounds/ui/button/click.ogg','minecraft/sounds.json','minecraft/textures/gui/options_background.png','minecraft/textures/gui/title/background/panorama_0.png','minecraft/textures/gui/icons.png','minecraft/textures/gui/title/minecraft.png','minecraft/lang/en_us.json','minecraft/texts/splashes.txt')) {
  $p = $props | Where-Object { $_.Name -eq $n } | Select-Object -First 1
  if ($p) { Write-Output ($n + ' -> ' + $p.Value.hash + ' size=' + $p.Value.size) } else { Write-Output ($n + ' -> MISSING') }
}
