$idx = Get-Content -Raw 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\indexes\34.json' | ConvertFrom-Json
$props = $idx.objects.PSObject.Properties
Write-Output '--- ui/button ---'
$props.Name | Where-Object { $_ -like '*ui/button*' } | Select-Object -First 8
Write-Output '--- widgets ---'
$props.Name | Where-Object { $_ -like '*widgets*' } | Select-Object -First 8
Write-Output '--- font ascii ---'
$props.Name | Where-Object { $_ -like '*font/ascii*' } | Select-Object -First 8
Write-Output '--- options_background ---'
$props.Name | Where-Object { $_ -like '*options_background*' } | Select-Object -First 4
Write-Output '--- title/minecraft ---'
$props.Name | Where-Object { $_ -like '*title/minecraft*' } | Select-Object -First 4
Write-Output '--- lang ---'
$props.Name | Where-Object { $_ -like '*lang/en_us*' } | Select-Object -First 4
Write-Output '--- splashes ---'
$props.Name | Where-Object { $_ -like '*splash*' } | Select-Object -First 4
Write-Output '--- icons ---'
$props.Name | Where-Object { $_ -like '*gui/icons*' } | Select-Object -First 4
Write-Output '--- gui/title ---'
$props.Name | Where-Object { $_ -like '*gui/title/*' } | Select-Object -First 10
