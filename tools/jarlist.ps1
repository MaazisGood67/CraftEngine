param([string]$Pattern = "font|gui|panorama|splash|lang/en_us")
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::OpenRead('C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar')
$z.Entries | Where-Object { $_.FullName -like 'assets/*' -and $_.FullName -match $Pattern } | ForEach-Object { '{0}  {1}' -f $_.Length, $_.FullName } | Select-Object -First 120
$z.Dispose()
