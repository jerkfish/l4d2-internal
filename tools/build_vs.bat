@echo off
rem Build the Visual Studio solution (Release|Win32) with MSBuild.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if errorlevel 1 exit /b 1
msbuild "%~dp0..\vs\VR.sln" /m /p:Configuration=Release /p:Platform=Win32
exit /b %errorlevel%
