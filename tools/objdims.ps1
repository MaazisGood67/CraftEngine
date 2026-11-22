$root = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta"
$idx = Get-Content "$root\assets\indexes\34.json" -Raw | ConvertFrom-Json
Add-Type -AssemblyName System.Drawing
$names = @(
 'minecraft/textures/gui/title/minecraft.png',
 'minecraft/textures/gui/title/edition.png',
 'minecraft/textures/gui/title/mojangstudios.png',
 'minecraft/textures/gui/title/background/panorama_0.png',
 'minecraft/textures/gui/title/background/panorama_1.png',
 'minecraft/textures/gui/title/background/panorama_overlay.png',
 'minecraft/textures/gui/sprites/widget/button.png',
 'minecraft/textures/font/ascii.png'
)
foreach ($n in $names) {
  $o = $idx.objects.$n
  if ($null -eq $o) { Write-Output "$n : NOT IN INDEX"; continue }
  $h = $o.hash
  $p = "$root\assets\objects\$($h.Substring(0,2))\$h"
  if (-not (Test-Path $p)) { Write-Output "$n : OBJECT MISSING $p"; continue }
  $len = (Get-Item $p).Length
  try {
    $img = [System.Drawing.Image]::FromFile($p)
    Write-Output ("{0} : {1}x{2} ({3} bytes)" -f $n, $img.Width, $img.Height, $len)
    $img.Dispose()
  } catch {
    Write-Output ("{0} : not an image ({1} bytes)" -f $n, $len)
  }
}
