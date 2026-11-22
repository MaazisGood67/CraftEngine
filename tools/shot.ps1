Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$exe = "C:\Users\Administrator\Desktop\REA Minecraaft C++\CraftEngine.exe"
$proc = Start-Process $exe -PassThru
Start-Sleep -Seconds 10

$proc.Refresh()
Write-Output ("RUNNING: {0}  TITLE: {1}" -f (-not $proc.HasExited), $proc.MainWindowTitle)

$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$out = "C:\Users\Administrator\Desktop\REA Minecraaft C++\shot_title.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$gfx.Dispose()
$bmp.Dispose()
Write-Output "SAVED $out"

# Let the panorama rotate, then click Singleplayer (window-relative coords)
Start-Sleep -Seconds 2
Write-Output ("TITLE2: {0}" -f $proc.MainWindowTitle)
Write-Output "PID=$($proc.Id)"
