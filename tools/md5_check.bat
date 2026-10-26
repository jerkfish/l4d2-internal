@echo off
rem Standalone check: MD5_PseudoRandom vs hashlib-derived expected values.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if errorlevel 1 exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
pushd "%~dp0..\build"
cl /nologo /EHsc /O2 /I"%~dp0..\include" "%~dp0md5_check.cpp" "%~dp0..\src\md5.cpp" /Fe:md5_check.exe
if errorlevel 1 (popd & exit /b 1)
.\md5_check.exe
set RC=%errorlevel%
popd
exit /b %RC%
