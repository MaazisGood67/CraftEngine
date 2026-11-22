Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Cap {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref POINT pt);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@

$proc = Get-Process CraftEngine -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $proc) { Write-Output "NOT RUNNING"; exit }
$hwnd = $proc.MainWindowHandle
Write-Output "HWND=$hwnd TITLE=$($proc.MainWindowTitle)"
$rect = New-Object Win32Cap+RECT
[Win32Cap]::GetClientRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top
Write-Output "CLIENT ${w}x${h}"
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $gfx.GetHdc()
$ok = [Win32Cap]::PrintWindow($hwnd, $hdc, 3)  # PW_CLIENTONLY|PW_RENDERFULLCONTENT
$gfx.ReleaseHdc($hdc)
$gfx.Dispose()
$out = "C:\Users\Administrator\Desktop\REA Minecraaft C++\shot_window.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "PRINTWINDOW=$ok SAVED $out"
