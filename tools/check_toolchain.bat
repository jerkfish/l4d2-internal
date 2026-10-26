@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
echo === cl ===
cl 2>&1
echo === link ===
link 2>&1
echo === where cl ===
where cl
echo === where link ===
where link
