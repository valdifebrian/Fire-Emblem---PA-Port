param([int]$Pid2 = 0, [string]$Keys = "{ENTER}", [int]$HoldMs = 120)
$d = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
if (-not $d) { 'DOLPHIN EXITED'; exit 1 }
$src = @'
using System;
using System.Runtime.InteropServices;
public static class Fw {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  public static bool Focus(IntPtr h) {
    ShowWindow(h, 9); // SW_RESTORE
    SetForegroundWindow(h);
    return GetForegroundWindow() == h;
  }
}
'@
Add-Type -TypeDefinition $src
$focused = [Fw]::Focus($d.MainWindowHandle)
'send to ' + $d.MainWindowHandle + ' focused=' + $focused
Start-Sleep -Milliseconds 300
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait($Keys)
"sent: $Keys"
