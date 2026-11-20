$proj = "C:/Users/Administrator/Desktop/REA Minecraaft C++"
$gpp = "C:/Users/Administrator/rea-tools/w64devkit/w64devkit/bin/g++.exe"
$zl = "C:/Users/Administrator/rea-tools/zlib-1.3.1"
Set-Location -LiteralPath $proj
& $gpp -std=c++20 -O0 -DUNICODE -D_UNICODE -I $zl -I "$proj/src" "$proj/tools/pngtest.cpp" "$proj/src/png.cpp" "$proj/src/zip.cpp" $zl/adler32.c $zl/crc32.c $zl/deflate.c $zl/infback.c $zl/inffast.c $zl/inflate.c $zl/inftrees.c $zl/trees.c $zl/zutil.c $zl/uncompr.c -o "$env:TEMP/pngtest.exe" 2>&1 | Select-Object -First 5
& "$env:TEMP/pngtest.exe"