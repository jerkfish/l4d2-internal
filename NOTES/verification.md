# Codegen verification workflow

Rebuild: `tools\build.bat` -> `build\Release\VR.dll` (MSVC 14.38 x86, PDB at
`build\Release\VR.pdb`).

Structural comparison of one function:

1. `tools\dump_one.bat <name-substring>` runs IDA headless
   (`idat.exe -A -S"tools\dump_one.py <symbol>"`) on the rebuilt DLL. The PDB
   is loaded automatically, so recovered functions keep their source names.
   Outputs `build\Release\one.asm` (disassembly + size header) and
   `build\Release\one.c` (Hex-Rays pseudocode).
2. `tools\dump_funcs.bat` writes `build\Release\functions.txt`
   (`EA SIZE NAME`, all functions) for batch comparisons.
3. Target-side ground truth comes from the live IDA MCP database
   (`original/target.dll.i64`), e.g. `disassemble_function 0x10033900`.

`tools\dump_funcs.py` must call `ida_auto.auto_wait()` before walking
`idautils.Functions()`; without it the batch script runs before auto-analysis
finishes and only sees the CRT tail. Delete the generated `VR.i64` before
re-running after a rebuild, otherwise IDA reuses the stale database.

## Toolchain flags — corrected

Initial Phase-0 inference of `/guard:cf` was wrong for project code:

- The target's `InitInterfaces` and user code contain **no**
  `__guard_check_icall_fptr` calls.
- The target's load config has `GuardFlags = 0x100` but
  `GuardCFFunctionTable = 0`, `GuardCFFunctionCount = 0`, and no IAT/longjump
  tables. A real `/guard:cf` link produces a populated table (our test build:
  `0x17500`, 41 function-table entries, 1 IAT entry).
- The guard-check pointer in the target is referenced only from prebuilt CRT
  objects (`__scrt_dllmain_exception_filter`, `__ArrayUnwind`, ...).

Conclusion: compile and link **without** `/guard:cf`. The residual guard
pointer/flag come from the prebuilt CRT libraries. `/GS`, `/GL`+`/LTCG`,
`/O2`, `/MD` remain.

## First comparisons

### InitInterfaces (0x10033900)

- Target: `0x207` bytes, 192 instructions, no CFG checks.
- Rebuild: `0x1D2` bytes after `FindPattern` and `ConsolePrint` were recovered
  (was `0x1BC` with empty stubs). Interface-resolution sequence matches; the
  remaining 0x35-byte gap is still unexplained and is tracked as a codegen
  exception candidate (likely MSVC 33130 vs 33145 or string-temporary
  layout). Revisit with an instruction diff if a later function shows the
  same constant delta.

### FindPattern (0x100376C0)

- Target: `0x1C3`; rebuild: `0x18F` (52 bytes smaller).
- Algorithm ported faithfully: fixed-width tokens, lone `?` advances 2,
  `"??"`/hex tokens advance 3, `token[2] == 0` terminates, module range from
  `K32GetModuleInformation`. The residual delta is the same ~0x34 bytes seen
  in InitInterfaces, supporting a systematic compiler/settings difference
  rather than an algorithmic one.

### ConsolePrint (0x100042D0)

- Target: `0x30`; rebuild: `0x30` — **exact size match**. The `vprintf`
  expansion resolves to the same UCRT `__stdio_common_vfprintf` call with the
  stdio options global.

## ImGui API address map (target 0x1000E360–0x10032AD0)

Attempt 1 — ordered size alignment: only 31/234 exact-size matches; the few
address coincidences (0x1000F7A0, 0x10019960, 0x1001DAB0) are incidental.

Attempt 2 — exact address correspondence with the rebuilt DLL:
- 530 ImGui functions in the rebuild (PDB names), 10 addresses exist in the
  target, and **0 have equal sizes**. Link order/addresses are not aligned
  (LTCG layout depends on the whole program), so address mapping is invalid.

Attempt 3 — `tools/imgui_match.py`, normalized instruction hashing
(mnemonics + operands with absolute addresses masked): **34 matches**, none of
the core APIs (`NewFrame`, `Begin`, `Checkbox`, `SliderFloat`, `Render`, ...).

Attempt 4 — mnemonic-sequence-only hashing: **39 matches**, still no core
APIs. Conclusion: LTCG specialized the target's ImGui function bodies
differently than our whole-program build, so function-content matching cannot
recover the address map.

**Pivot:** identify ImGui APIs at each target call site from argument shape
and usage instead of callee identity. Reliable patterns in the decompilation:
- `bool sub_X(label, bool* value, ...)` -> `ImGui::Checkbox`
- `bool sub_X(label, bool* open, flags)` on a window string -> `ImGui::Begin`
- `bool sub_X(label, float* value, min, max, fmt)` -> `ImGui::SliderFloat`
- `bool sub_X(label)` with no other args -> `ImGui::Button`
- `void sub_X(label, ...)` printing text -> `Text`/`TextColored`
- combo/list calls take a string array in .rdata plus an int* index.
Port the menu from `Menu_Render.json` / EndScene UI section using these shapes,
then validate the rebuilt menu visually against the original.

## Build-hygiene warning

With `/GL`, an incremental MSBuild run after editing flags/sources can reuse
stale IL: a changed `FindPattern` still measured `0x7F` and appeared
unmodified until `build/` was deleted. **Always clean-build before structural
measurement** (`rm -rf build && tools\build.bat`) and delete the generated
`VR.i64` before re-running the headless IDA dump.
