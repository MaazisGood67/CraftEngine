$path = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($path)
$e = $zip.Entries | Where-Object { $_.FullName -eq "assets/minecraft/textures/block/stone.png" } | Select-Object -First 1
if ($null -eq $e) { Write-Output "NOT FOUND"; $zip.Dispose(); exit }
Write-Output ("Name: {0} Size: {1} CompressedSize: {2} Compression: {3}" -f $e.FullName, $e.Size, $e.CompressedSize, $e.CompressionMethod)
$out = "C:\Users\Administrator\Desktop\REA Minecraaft C++\stone.png"
$fs = New-Object System.IO.FileStream($out, [System.IO.FileMode]::Create)
$e.Open().CopyTo($fs)
$fs.Dispose()
$bytes = [System.IO.File]::ReadAllBytes($out)
$s = -join ($bytes[0..31] | ForEach-Object { "{0:X2}" -f $_ })
Write-Output ("First 32 bytes: {0}" -f $s)
$zip.Dispose()
