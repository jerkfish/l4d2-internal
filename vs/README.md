# Visual Studio project

Convenience solution for Visual Studio 2022 (v143, 32-bit only). CMake
(`..\CMakeLists.txt`) remains the reference build; this project uses the
same sources, include paths, compiler settings and `d3d9.lib`.

- Open `VR.sln`, select `Release | Win32`, Build. Both Release and Debug
  Win32 were verified with MSBuild: 0 errors, only the known C4244 warning.
- Output: `vs\bin\Release\VR.dll` (Debug: `vs\bin\Debug\VR.dll`).
- The project pins `VCToolsVersion` to `14.38.33130` to match the target
  toolchain. Remove that line if your VS installation uses a different
  14.x toolset.

CLI:

```
tools\build_vs.bat
```

Only Win32 is defined. The recovered code uses 32-bit Source engine ABIs
(4-byte pointers, thiscall/fastcall, CUserCmd layout) and will not build or
run as x64.
