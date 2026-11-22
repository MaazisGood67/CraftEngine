$path = "C:\Users\Administrator\AppData\Roaming\Migurinth\meta\versions\26.3\26.3.jar"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($path)
$patterns = @(
    'assets/minecraft/textures/gui/title/',
    'assets/minecraft/textures/gui/sprites/widget/',
    'assets/minecraft/textures/gui/sprites/progress',
    'assets/minecraft/textures/gui/sprites/loading',
    'assets/minecraft/textures/font/',
    'assets/minecraft/sounds/ui/',
    'assets/minecraft/sounds/random/click',
    'assets/minecraft/sounds/music/menu/',
    'assets/minecraft/textures/gui/sprites/hud/hotbar',
    'assets/minecraft/textures/gui/sprites/hud/crosshair',
    'assets/minecraft/textures/block/dirt.png',
    'assets/minecraft/textures/block/stone.png'
)
foreach ($p in $patterns) {
    Write-Output "=== $p ==="
    $zip.Entries | Where-Object { $_.FullName.StartsWith($p) } | Select-Object -First 40 -ExpandProperty FullName
}
$zip.Dispose()
