"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" & ^
cl *.cpp /Fe: unlz2k.exe /EHsc -O2 & ^
del *.obj & ^
echo Done. & ^
pause >nul
