param([string]$Extra = "")
Set-Location $PSScriptRoot
$dir = 'C:\Kodingan\fe9-re\out\vmtest'
$files = Get-ChildItem "$dir\*.cmb" | Sort-Object Name
foreach ($f in $files) {
  $out = & .\vmtest.exe $dir $f.Name '--entry' 'Opening0' '--frames' '60000' 2>$null | Select-String -Pattern '\[stop\]|\[done\]|vm ERR|bad cmb' | ForEach-Object { $_.Line }
  $last = ($out | Select-Object -Last 2) -join ' | '
  Write-Output ("{0}: {1}" -f $f.Name, $last)
}
