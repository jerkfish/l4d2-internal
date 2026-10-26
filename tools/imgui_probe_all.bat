@echo off
rem Probe every downloaded ImGui version and print one fingerprint line each.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul 2>&1
for %%v in (1.50 1.51 1.52 1.53 1.60 1.61 1.62 1.63 1.64 1.65 1.66b 1.67 1.68 1.69 1.70 1.71 1.72b 1.73 1.74 1.75 1.76 1.77 1.78 1.79 1.80 1.81 1.82 1.83 1.84.2 1.85 1.86 1.87 1.88 1.89 1.89.9 1.90 1.90.1 1.90.2) do (
    if exist "NOTES\ida\imgui\%%v\imgui.h" (
        if not exist "build\imgui\%%v" mkdir "build\imgui\%%v"
        cl /nologo /std:c++17 /EHsc /W0 /I "NOTES\ida\imgui\%%v" "tools\imgui_probe.cpp" /Fo:"build\imgui\%%v\probe.obj" /Fe:"build\imgui\%%v\probe.exe" >nul 2>&1
        if errorlevel 1 (
            echo %%v BUILD_FAIL
        ) else (
            "build\imgui\%%v\probe.exe"
        )
    ) else (
        echo %%v NO_HEADERS
    )
)
