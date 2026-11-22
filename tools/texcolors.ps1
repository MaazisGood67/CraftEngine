Add-Type -AssemblyName System.Drawing
$tmp = "C:\Users\Administrator\AppData\Local\Temp\ce_gui"
$jar = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($jar)
foreach ($n in @('grass_block_top','grass_block_side','oak_leaves','dirt','stone')) {
  $name = "assets/minecraft/textures/block/$n.png"
  $e = $zip.Entries | Where-Object { $_.FullName -eq $name } | Select-Object -First 1
  $out = "$tmp\$n.png"
  [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $out, $true)
  $bmp = [System.Drawing.Bitmap]::FromFile($out)
  $sumR=0;$sumG=0;$sumB=0;$sumA=0;$cnt=0
  for ($y=0; $y -lt $bmp.Height; $y++) {
    for ($x=0; $x -lt $bmp.Width; $x++) {
      $p = $bmp.GetPixel($x,$y)
      $sumR += $p.R; $sumG += $p.G; $sumB += $p.B; $sumA += $p.A; $cnt++
    }
  }
  Write-Output ("{0}: avg R={1} G={2} B={3} A={4}" -f $n, [int]($sumR/$cnt), [int]($sumG/$cnt), [int]($sumB/$cnt), [int]($sumA/$cnt))
  $bmp.Dispose()
}
$zip.Dispose()
