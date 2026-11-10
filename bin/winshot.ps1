param([int]$Pid2 = 0, [string]$Out = "C:\Kodingan\fe9-captures\agent\shot.png")
$d = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
if (-not $d) { 'DOLPHIN EXITED'; exit 1 }
'hwnd: ' + $d.MainWindowHandle + ' | ' + $d.MainWindowTitle
$src = @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public static class WCap {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  public static Bitmap Shot(IntPtr h, int w, int hh) {
    var bmp = new Bitmap(w, hh);
    using(var g = Graphics.FromImage(bmp)) {
      var hdc = g.GetHdc();
      PrintWindow(h, hdc, 2);
      g.ReleaseHdc(hdc);
    }
    return bmp;
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing
$rc = New-Object WCap+RECT
[void][WCap]::GetWindowRect($d.MainWindowHandle, [ref]$rc)
$w = $rc.R - $rc.L; $hh = $rc.B - $rc.T
if ($w -lt 10 -or $hh -lt 10) { 'WINDOW TOO SMALL (minimized?)'; exit 2 }
$bmp = [WCap]::Shot($d.MainWindowHandle, $w, $hh)
$bmp.Save($Out)
"saved $w x $hh -> $Out"
