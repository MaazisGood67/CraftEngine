# Extracts the whole worldgen density function tree from the user's 26.3 jar to .local/df.
param([string]$Out = "$PSScriptRoot\..\.local\df")
$ErrorActionPreference = 'Stop'
$proj = Split-Path -Parent $PSScriptRoot
$jar = & (Join-Path $proj 'tools\vanilla-env.ps1') -Action jar
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::OpenRead($jar)
$count = 0
foreach ($e in $z.Entries) {
    if ($e.FullName -like 'data/minecraft/worldgen/density_function/*.json') {
        $rel = $e.FullName -replace '^data/minecraft/worldgen/density_function/', ''
        $dest = Join-Path $Out $rel
        New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($dest)) | Out-Null
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest, $true)
        $count++
    }
}
$z.Dispose()
Write-Output "extracted $count density function JSONs to $Out"
