Add-Type -AssemblyName System.IO.Compression.FileSystem
Write-Output '=== JAR gui/sprites + fonts ==='
$z = [System.IO.Compression.ZipFile]::OpenRead('C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar')
$z.Entries | Where-Object { $_.FullName -like 'assets/*' -and $_.FullName -match 'sprites|textures/font|title/' } | ForEach-Object { '{0}  {1}' -f $_.Length, $_.FullName } | Select-Object -First 80
$z.Dispose()
Write-Output '=== INDEX sprites/button/hotbar ==='
$idx = Get-Content -Raw 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\indexes\34.json' | ConvertFrom-Json
$props = $idx.objects.PSObject.Properties
$props.Name | Where-Object { $_ -match 'sprites|button|hotbar|crosshair|options_bg|en_us' } | Select-Object -First 40
