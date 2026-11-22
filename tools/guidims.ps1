$jar = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar"
$tmp = "C:\Users\Administrator\AppData\Local\Temp\ce_gui"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing
$zip = [System.IO.Compression.ZipFile]::OpenRead($jar)
$names = @(
  'assets/minecraft/textures/gui/title/minecraft.png',
  'assets/minecraft/textures/gui/title/edition.png',
  'assets/minecraft/textures/gui/title/mojangstudios.png',
  'assets/minecraft/textures/gui/title/background/panorama_0.png',
  'assets/minecraft/textures/gui/title/background/panorama_overlay.png',
  'assets/minecraft/textures/gui/sprites/widget/button.png',
  'assets/minecraft/textures/gui/sprites/widget/button_highlighted.png',
  'assets/minecraft/textures/gui/sprites/widget/button_disabled.png',
  'assets/minecraft/textures/gui/sprites/widget/slider.png',
  'assets/minecraft/textures/gui/sprites/widget/slider_handle.png',
  'assets/minecraft/textures/font/ascii.png',
  'assets/minecraft/textures/block/dirt.png',
  'assets/minecraft/textures/block/oak_leaves.png',
  'assets/minecraft/textures/block/water_still.png',
  'assets/minecraft/textures/gui/sprites/hud/hotbar.png',
  'assets/minecraft/textures/gui/sprites/hud/hotbar_selection.png',
  'assets/minecraft/textures/gui/sprites/hud/crosshair.png',
  'assets/minecraft/textures/gui/sprites/widget/button.png.mcmeta'
)
foreach ($n in $names) {
  $e = $zip.Entries | Where-Object { $_.FullName -eq $n } | Select-Object -First 1
  if ($null -eq $e) { Write-Output "$n : MISSING"; continue }
  $out = Join-Path $tmp ([IO.Path]::GetFileName($n))
  [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $out, $true)
  if ($n.EndsWith('.png')) {
    $img = [System.Drawing.Image]::FromFile($out)
    Write-Output ("{0} : {1}x{2}" -f $n, $img.Width, $img.Height)
    $img.Dispose()
  } else {
    Write-Output ("{0} : {1}" -f $n, (Get-Content $out -Raw))
  }
}
$zip.Dispose()
