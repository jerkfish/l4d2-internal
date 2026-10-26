@echo off
rem Build/run the context probe for one include dir.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if not exist "%~2" mkdir "%~2"
cl /nologo /std:c++17 /EHsc /W0 /I "%~1" "%~dp0imgui_probe_ctx.cpp" /Fo:"%~2\ctx.obj" /Fe:"%~2\ctx.exe" >nul 2>&1
if errorlevel 1 exit /b 1
"%~2\ctx.exe"
