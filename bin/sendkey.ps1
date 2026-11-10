param([int]$Pid2 = 0, [string]$Vks = "0x0D", [int]$Repeat = 1, [int]$HoldMs = 120)
$d = Get-Process -Id $Pid2 -ErrorAction SilentlyContinue
if (-not $d) { 'DOLPHIN EXITED'; exit 1 }
$src = @'
using System;
using System.Runtime.InteropServices;
public static class Kb {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  public static void Tap(IntPtr hwnd, byte vk, int holdMs) {
    SetForegroundWindow(hwnd);
    byte scan = (byte)MapKeyVkd(vk);
    keybd_event(vk, scan, 0, UIntPtr.Zero);
    System.Threading.Thread.Sleep(holdMs);
    keybd_event(vk, scan, 2, UIntPtr.Zero); // KEYUP
  }
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
  static uint MapKeyVkd(uint vk) { return MapVirtualKey(vk, 0); }
}
'@
Add-Type -TypeDefinition $src
$vkList = $Vks.Split(',') | ForEach-Object { [byte][convert]::ToInt32($_.Trim(), 16) }
for ($r = 0; $r -lt $Repeat; $r++) {
  foreach ($vk in $vkList) {
    [Kb]::Tap($d.MainWindowHandle, $vk, $HoldMs)
    Start-Sleep -Milliseconds 250
  }
}
"tapped $Repeat rounds of: $Vks"
