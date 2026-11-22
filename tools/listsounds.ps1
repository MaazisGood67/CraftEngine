$root = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta"
$idx = Get-Content "$root\assets\indexes\34.json" -Raw | ConvertFrom-Json
$keys = $idx.objects.PSObject.Properties.Name
Write-Output "=== block stone/dirt/grass sounds ==="
$keys | Where-Object { $_ -match 'sounds/block/(stone|dirt|grass|wood)/' } | Select-Object -First 30
Write-Output "=== legacy dig/place ==="
$keys | Where-Object { $_ -match 'sounds/(dig|place|step)/' } | Select-Object -First 20
Write-Output "=== music/menu + ambient ==="
$keys | Where-Object { $_ -match 'sounds/(music/menu|ambient/cave)' } | Select-Object -First 10
Write-Output "=== ui click ==="
$keys | Where-Object { $_ -match 'random/(click|pop|levelup)' }
