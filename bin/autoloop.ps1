param([int]$Pid2 = 0, [int]$Loops = 30, [int]$SleepSec = 4, [string]$Keys = "{ENTER}", [string]$Dir = "C:\Kodingan\fe9-captures\agent\cap1")
if (-not (Test-Path $Dir)) { New-Item -ItemType Directory -Path $Dir | Out-Null }
$src = @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public static class Cap {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  public static void Shot(IntPtr h, string path) {
    RECT rc; GetWindowRect(h, out rc);
    int w = rc.R - rc.L, hh = rc.B - rc.T;
    if (w < 10 || hh < 10) return;
    var bmp = new Bitmap(w, hh);
    using(var g = Graphics.FromImage(bmp)) { var hdc = g.GetHdc(); PrintWindow(h, hdc, 2); g.ReleaseHdc(hdc); }
    bmp.Save(path);
    bmp.Dispose();
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$d = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
if (-not $d) { 'DOLPHIN EXITED'; exit 1 }
[Cap]::Shot($d.MainWindowHandle, "$Dir\start.png")
for ($i = 1; $i -le $Loops; $i++) {
  $d = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
  if (-not $d) { "EXITED at $i"; break }
  [Cap]::Shot($d.MainWindowHandle, ("$Dir\{0:d3}.png" -f $i))
  [void][Cap]::SetForegroundWindow($d.MainWindowHandle)
  Start-Sleep -Milliseconds 250
  [System.Windows.Forms.SendKeys]::SendWait($Keys)
  Start-Sleep -Seconds $SleepSec
}
"done $Loops loops"
