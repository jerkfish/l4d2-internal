# Publishing notes

This directory is a source-only snapshot of the VR.dll recovery project,
prepared for a git repository. No git repository was initialized and no
commit was made by the copy operation.

## Included

- `src/`, `include/`, `CMakeLists.txt`
- `vs/` Visual Studio 2022 solution/project (Win32, 14.38 toolset) and
  `tools/build_vs.bat`
- `third_party/imgui-1.74`, `third_party/minhook`, `third_party/kiero-1.2.9`
  (the three dependencies the build uses, with their licenses)
- `tools/` recovery, build and offline-check scripts. In this copy
  `tools/loadtest.bat` defaults to `build\Release\VR.dll` because `staging/`
  is excluded; pass a DLL path as the first argument to override.
- `README.md`, `CHANGELOG.md`, `STATE.md`, `tests.md`,
  `ledger.csv`, `ledger.json`
- `NOTES/` recovery notes, plus `NOTES/ida/functions.tsv` and
  `functions_lines.tsv` used by `tools/gen_ledger.py`
- `tests/.gitkeep` (the directory is intentionally empty)

## Excluded from this copy

| Path | Reason |
|---|---|
| `original/` (target.dll, IDA .i64/.id0/.id1/.nam/.til) | Third-party binary and database; not needed to build |
| root `VR.dll` | Target binary copy |
| `staging/` | Staged/injected builds and debug logs |
| `build/` | Local build artifacts, objects, map, test executables |
| `SKernelInjector.exe` | Third-party injector binary |
| `loadtest.obj` | Local scratch object |
| `tools/rea_tools.json` | Large local IDA/MCP tool manifest |
| `tools/vs_install_*.log` | Local install logs |
| `tools/__pycache__/` | Python cache |
| `tools/_disasm_range.bat`, `tools/_exports.bat` | Scratch scripts |
| `third_party/kiero/` | Unused duplicate; the build uses `kiero-1.2.9` |
| `NOTES/ida/Cheat_Update.c`, `*.json`, `*.asm.json`, `sub_*.c`, `target_disasm.txt`, `NOTES/ida/imgui/` | Raw decompilation/disassembly snapshots: large and derivative of the third-party target; kept in the local working tree only |

`NOTES/ida/README.md` states the same scope for the analysis directory.

## Before making the repository public

1. Decide and add a license for the recovered source. No license is
   included or implied for the recovered code.
2. Review the legal position of publishing reconstructed source from a
   third-party cheat binary; the original binary and raw decompilation are
   intentionally not in this copy.
3. Keep the third-party license files: ImGui MIT, MinHook BSD-2-Clause,
   Kiero MIT (see `THIRD_PARTY_NOTICES.md`).
4. Confirm a clean build from the copied tree:
   `tools\build.bat` (VS 2022 14.38 x86). This copied tree was built and
   checked on 2026-10-26: 0 errors, `LOAD_OK`, MD5 vectors and No-Spread scan
   PASS. The Visual Studio solution also built Release|Win32 and Debug|Win32
   with MSBuild (0 errors, known C4244 warning). `build/` and `vs/bin|obj/`
   were removed afterward so no artifacts are included.
5. Confirm no binaries or secrets are staged: `.gitignore` already excludes
   `build/`, `original/`, `staging/`, DLLs, EXEs, IDA databases, caches and
   logs.

## Oracle requirement

The live tests in `tests.md` compare against the original `VR.dll`, which is
not included. Supply it locally (outside the repository) to reproduce the
verification.
