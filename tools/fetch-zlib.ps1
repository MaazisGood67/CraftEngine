$ProgressPreference = "SilentlyContinue
$ProgressPreference = "SilentlyContinue
$ErrorActionPreference = "Continue
$url = "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
$out = Join-Path $env:TEMP "zlib-1.3.1.tar.gz"
Invoke-WebRequest -Uri $url -OutFile $out
Write-Output ("Downloaded: {0:N1} MB" -f ((Get-Item $out).Length / 1MB))
Expand-Archive -Path $out -DestinationPath $env:USERPROFILE\rea-tools -Force
Get-ChildItem $env:USERPROFILE\rea-tools -Directory | Where-Object { $_.Name -like "zlib*" } | Select-Object -ExpandProperty FullName
