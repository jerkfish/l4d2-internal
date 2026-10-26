@echo off
rem Build and run the ImGui layout probe against one header directory.
rem Usage: imgui_probe.bat <include-dir> <build-dir>
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if not exist "%~2" mkdir "%~2"
cl /nologo /std:c++17 /EHsc /W0 /I "%~1" "%~dp0imgui_probe.cpp" /Fo:"%~2\probe.obj" /Fe:"%~2\probe.exe"
if errorlevel 1 exit /b 1
"%~2\probe.exe"
