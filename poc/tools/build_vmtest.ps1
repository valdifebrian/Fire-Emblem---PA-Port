$CC = 'C:\Users\valdi\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe'
Set-Location $PSScriptRoot
& $CC -std=c++20 -O2 -static -I"$PSScriptRoot\..\app\src\main\cpp" "$PSScriptRoot\vmtest.cpp" -o "$PSScriptRoot\vmtest_new.exe"
Write-Output "BUILD_RC=$LASTEXITCODE"
