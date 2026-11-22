Add-Type -AssemblyName System.IO.Compression.FileSystem
$idx = 'C:\Users\Administrator\AppData\Roaming\Migurinth\meta\assets\indexes\34.json'
$j = Get-Content -Raw $idx | ConvertFrom-Json
$props = $j.objects.PSObject.Properties
Write-Output ("objects: " + $props.Count)
$props.Name | Where-Object { $_ -match 'sounds/ui|gui/widgets|gui/options_background|panorama_0|sounds/random/click|/sounds\.json|texts/splashes|font/ascii' } | Select-Object -First 15
Write-Output "--- top-level keys of index:"
$j.PSObject.Properties.Name
