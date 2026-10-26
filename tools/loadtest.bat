@echo off
rem Publishable copy: staging/ is not included, so default to the build output.
rem Pass a DLL path as the first argument to override.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cl /nologo /O2 /EHsc "%~dp0loadtest.cpp" /Fo:"%~dp0..\build\loadtest.obj" /Fe:"%~dp0..\build\loadtest.exe" >nul 2>&1
if errorlevel 1 exit /b 1
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=%~dp0..\build\Release\VR.dll"
"%~dp0..\build\loadtest.exe" "%TARGET%"
