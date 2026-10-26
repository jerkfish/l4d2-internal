@echo off
rem Headless IDA analysis of the rebuilt DLL; writes build\Release\functions.txt
setlocal
set IDA=C:\Program Files\IDA Professional 9.0\idat.exe
"%IDA%" -A -S"%~dp0dump_funcs.py" "%~dp0..\build\Release\VR.dll"
exit /b %errorlevel%
