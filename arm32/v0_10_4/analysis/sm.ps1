param([string[]]$rvas, [int]$n = 300)
$env:ARM32_EXE = 'D:\repo\win32craft\0.10.4.40\Minecraft Pocket Edition v0.10.4.40 [Winphone Iz]\MCPE.Host.WindowsPhone.exe'
Set-Location D:\repo\Win32Craft-main
foreach ($r in $rvas) {
  $o = python tools/arm32/arm32re.py func $r -n $n
  $o | Select-Object -First 1
  $o | Select-String -Pattern 'bl\s|blx|IAT|str "|wstr|vtable|movw.*->' | Where-Object { $_.Line -notmatch '#0x1458|#0x1470|#0x121dc|thunk\[msvcr120_app.dll!\?\?3' } | ForEach-Object { '   ' + $_.Line.Trim() }
}
