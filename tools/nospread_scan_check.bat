@echo off
rem Offline execution of the real No-Spread scan against installed client.dll.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if errorlevel 1 exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
pushd "%~dp0..\build"
cl /nologo /EHsc /O2 /I"%~dp0..\include" "%~dp0nospread_scan_check.cpp" "%~dp0..\src\nospread_scan.cpp" /Fe:nospread_scan_check.exe
if errorlevel 1 (popd & exit /b 1)
.\nospread_scan_check.exe
set RC=%errorlevel%
popd
exit /b %RC%
