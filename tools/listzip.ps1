$path = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($path)
$cnt = 0
$first = @()
foreach ($e in $zip.Entries) {
    if ($cnt -lt 6) { $first += ("{0} | {1} | {2} | {3}" -f $e.FullName, $e.CompressionMethod, $e.Size, $e.CompressedSize) }
    $cnt++
}
Write-Output ("TOTAL ENTRIES: " + $cnt)
$first | ForEach-Object { Write-Output $_ }
$zip.Dispose()
