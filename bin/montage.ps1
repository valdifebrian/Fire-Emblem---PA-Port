param([string]$Dir = "C:\Kodingan\fe9-captures\agent\cap1", [string]$Out = "C:\Kodingan\fe9-captures\agent\cap1\montage.png", [int]$Cols = 4, [int]$Scale = 2, [int]$Every = 1)
$files = Get-ChildItem "$Dir\*.png" | Where-Object { $_.Name -notmatch "montage|start" } | Sort-Object Name
$sel = @(); for ($i = 0; $i -lt $files.Count; $i += $Every) { $sel += $files[$i] }
$rows = [math]::Ceiling($sel.Count / $Cols)
Add-Type -AssemblyName System.Drawing
$first = [System.Drawing.Image]::FromFile($sel[0].FullName)
$w = [int]($first.Width / $Scale); $hh = [int]($first.Height / $Scale)
$first.Dispose()
$sheet = New-Object System.Drawing.Bitmap ($w * $Cols), ($hh * $rows)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
for ($i = 0; $i -lt $sel.Count; $i++) {
  $img = [System.Drawing.Image]::FromFile($sel[$i].FullName)
  $cx = ($i % $Cols) * $w; $cy = [int]($i / $Cols) * $hh
  $g.DrawImage($img, $cx, $cy, $w, $hh)
  $img.Dispose()
  $label = [string]$i
  $g.DrawString(("#" + $sel[$i].BaseName + " " + $label), (New-Object System.Drawing.Font("Arial", 11, [System.Drawing.FontStyle]::Bold)), [System.Drawing.Brushes]::Yellow, ($cx + 4), ($cy + 2))
}
$g.Dispose()
$sheet.Save($Out)
"montage: $($sel.Count) frames -> $Out"
