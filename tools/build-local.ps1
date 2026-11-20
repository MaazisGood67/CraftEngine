$proj = "C:/Users/Administrator/Desktop/REA Minecraaft C++"
$gpp = "C:/Users/Administrator/rea-tools/w64devkit/w64devkit/bin/g++.exe"
$zl = "C:/Users/Administrator/rea-tools/zlib-1.3.1"
Set-Location -LiteralPath $proj
& $gpp -std=c++20 -O2 -mwindows -municode -DUNICODE -D_UNICODE -I $zl src/main.cpp src/chunk.cpp src/world.cpp src/renderer.cpp src/textures.cpp src/zip.cpp src/png.cpp $zl/adler32.c $zl/crc32.c $zl/deflate.c $zl/infback.c $zl/inffast.c $zl/inflate.c $zl/inftrees.c $zl/trees.c $zl/zutil.c $zl/uncompr.c -o CraftEngine.exe -ld3d10 -ldxgi -ld3dcompiler -static -s > "C:\Users\Administrator\AppData\Local\Temp\craftbuild.log" 2>&1
Write-Output ("EXIT: $LASTEXITCODE")
Get-Content "C:\Users\Administrator\AppData\Local\Temp\craftbuild.log" -Raw | Select-Object -First 300
