@echo off
rem Configure and build VR.dll with the matched MSVC 14.38 x86 toolset.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
if errorlevel 1 exit /b 1
cmake -S "%~dp0.." -B "%~dp0..\build" -G "Visual Studio 17 2022" -A Win32 -T "v143,version=14.38" >nul
if errorlevel 1 (
    echo CMake configure failed
    exit /b 1
)
cmake --build "%~dp0..\build" --config Release -- /m
exit /b %errorlevel%
