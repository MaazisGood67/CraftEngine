Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::OpenRead('C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar')
$z.Entries | Where-Object { $_.FullName -match 'sprites/widget/|sprites/hud/|sprites/title/|options_background|menu_background|textures/gui/title/|font/ascii|lang/en_us|splashes' } | ForEach-Object { '{0}  {1}' -f $_.Length, $_.FullName } | Select-Object -First 60
$z.Dispose()
