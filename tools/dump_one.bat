@echo off
rem Usage: dump_one.bat <symbol-substring>
setlocal
set IDA=C:\Program Files\IDA Professional 9.0\idat.exe
"%IDA%" -A -S"%~dp0dump_one.py %~1" "%~dp0..\build\Release\VR.dll"
exit /b %errorlevel%
