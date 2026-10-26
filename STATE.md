# STATE.md

Publishable copy note: the original DLL/IDB, build outputs, staging binaries,
injector and raw IDA dumps are excluded; see `PUBLISHING.md`.

Last update: 2026-10-26
Current phase: **Phase 1 bootstrap runtime-validated; Phase 3 recovery in progress**
Rebuild status: **build skeleton green** — `build/Release/VR.dll` compiles
and links with the matched toolchain; bootstrap + `InitInterfaces` recovered,
feature bodies still stubs
Verification metrics: 3 functions compared
(`InitInterfaces` 0x1D2 vs 0x207; `FindPattern` 0x18F vs 0x1C3;
`ConsolePrint` 0x30 vs 0x2A — residual ~0x34 delta tracked in
`NOTES/verification.md`)
IDA status: connected; 509 functions, 688 strings, 61 structures; database open
IDA naming: **16 user functions + 28 globals renamed with evidence** (see
`NOTES/ida-naming.md`); IDB comments added to all hook/bootstrap functions

## TL;DR

Recon found two hard conflicts with the task brief:

1. **The target is 32-bit PE32/i386, not PE32+ x64.** The x64-specific rules file
   does not apply. 32-bit MSVC ABI (`cdecl`/`stdcall`/`thiscall`, 4-byte pointers,
   EBX/ESI/EDI callee-saved, stack args) is the correct model.
2. ~~No IDA MCP server is connected to the agent session.~~ **Resolved:** the
   direct `ida-pro-mcp` bridge is connected (IDA Python 3.12 fixed; port 13337
   live). See the sections below for the full setup trail.

Additional structural facts that change the plan:

- **The DLL has zero exports.** It is an injected L4D2 cheat module, not a
  library with a public API. Phase 1 "exports" is empty; the real interface is
  `DllMain`, engine vtable hooks (CreateInterface), and an ImGui overlay.
- Consequently the verification harness cannot be a simple GetProcAddress
  caller. Behavioral diffing will need either L4D2 + injector or a synthetic
  host that stubs the engine interfaces.
- **MinHook 1.3.3 is statically linked** (plus ImGui / Kiero backends); those
  must be identified by FLIRT/pattern and replaced with real library sources or
  includes, not recovered as user code.

## Target

- File: `VR.dll` (313,344 bytes)
- Preserved read-only copy: `original/target.dll`
- SHA-256: `fc5625237f287f59a2d9d5cd4b9202e861f1cb46f9a76bf2d14272baafa0397d`
- Machine: `0x014C` (IMAGE_FILE_MACHINE_I386)
- Optional header magic: `0x10B` (PE32, 32-bit)
- Timestamp: 2024-02-09 15:04:49 UTC (`0x65C63F11`)
- Linker: 14.38 (MSVC 14.38.33130 / VS 2022 17.8) — see toolchain below
- Characteristic: DLL | 32BIT_MACHINE | EXECUTABLE_IMAGE (`0x2102`)
- Subsystem: 2 (Windows GUI)
- ImageBase: `0x10000000`; SizeOfImage `0x50000`; entry point RVA `0x3EAB3`
- DllCharacteristics: `0x0140` = DYNAMIC_BASE | NX_COMPAT
- Sections:
  - `.text`  VA `0x1000`  vsize `0x3F6C7` (≈256 KiB)
  - `.rdata` VA `0x41000` vsize `0x08F68`
  - `.data`  VA `0x4A000` vsize `0x01650`
  - `.rsrc`  VA `0x4C000` vsize `0x00580`
  - `.reloc` VA `0x4D000` vsize `0x02F04`
- Export directory: **absent** (RVA 0 / size 0)
- TLS directory: present (`0x47840`) but `AddressOfCallBacks` (`0x100412B4`)
  points at eight zero DWORDs → **no TLS callbacks**
- Load config: RVA `0x476F0`; SecurityCookie `0x1004A040`; GuardFlags `0x100`
  (CF instrumented) → `/GS` and `/guard:cf` were used (verify details in IDA)
- Debug directory: CodeView RSDS; PDB path
  `C:\Users\void\Documents\xyz\cheats\l4d2 - UC\compiled\l4d2-x86.pdb`
- Resources: VS_VERSIONINFO only; identifies **MinHook 1.3.3.0** (Tsuda Kageyu),
  InternalName `MinHookD`, ProductName `MinHook DLL` (likely inherited MinHook
  `.rc`; not evidence that the main module was a debug build)

## Module identity (evidence-based)

Left 4 Dead 2 cheat DLL, built 2024-02-09, with MinHook detours and an ImGui
overlay. Evidence in `NOTES/phase0-recon.md`:

- L4D2 netprops: `DT_BasePlayer`, `DT_TerrorPlayer`, `m_iHealth`, `m_iTeamNum`,
  `m_flSimulationTime`, `m_vecPunchAngle`, `m_nTickBase`, `m_hActiveWeapon`,
  `DT_Witch`, `m_rage`, `DT_BaseCombatWeapon`, ...
- Engine interface names: `VClient016`, `VEngineClient013`,
  `VClientEntityList003`, `VEngineCvar007`, `VMaterialSystem080`,
  `VGUI_Panel009`, `VGUI_Surface031`, `EngineTraceClient003`,
  `VDebugOverlay003`, `CreateInterface`.
- Modules referenced: `client.dll`, `server.dll`, `engine.dll`, `vstdlib.dll`,
  `vguimatsurface.dll`, `vgui2.dll`, `materialsystem.dll`.
- Cheat menu strings: Aimbot, Triggerbot, Silent Aim, Player ESP, Chams,
  SpeedHack, Teleport, Anti-Aim, Fake Lag, Chat Spam, Server Lagger,
  Anti Smoker, etc. Tabs: Legit / Rage / Visuals / Misc.
- Config: `C:\L4D2space\legit.ini`, `rage.ini`, `hvh.ini`; `imgui.ini`,
  `imgui_log.txt`; keys via `GetPrivateProfileStringA` /
  `WritePrivateProfileStringA`.
- Anti-cheat text: "SMAC anti-cheat is installed!", "No anti-cheat detected!",
  "SMAC MODE IS DISABLED!", "SMAC Mode".
- Branding string: `[www.unknowncheats.me] @space785`.
- Overlay: ImGui (DX9/10/11/12, OpenGL, Vulkan backend strings), Kiero
  (`Kiero DirectX Window`), embedded font handling.

## Toolchain fingerprint

- Compiler: MSVC x86 (v143), linker 14.38.33130 → **VS 2022 17.8**.
- Rich header: final toolset objects at build 33130/33030 (product IDs
  `0x0109`, `0x00FF`, `0x0102`, `0x0105`, `0x0104`, `0x0103`, `0x0101`);
  older objects at builds 30729/30795/30319/21202 (`0x0093`, `0x009C`) from
  prebuilt static libraries (MinHook 1.3.3 and friends).
- CRT: **dynamic release** — imports `VCRUNTIME140.dll`, `MSVCP140.dll`, UCRT
  API sets; no `*D.dll` imports. Likely `/MD`, not `/MT` or `/MDd`.
- Exceptions: `__CxxFrameHandler3`, `_CxxThrowException` → MSVC C++ EH
  (probably `/EHsc`; check state maps in IDA).
- Hardening: `/GS` (SecurityCookie + `__security_init_cookie` at `0x1003EEE1`,
  called from the CRT startup), `/DYNAMICBASE`, `/NXCOMPAT`.
- **`/guard:cf` is NOT used for project code** (corrected 2026-10-25): the
  target has zero `__guard_check_icall_fptr` calls and an empty CFG function
  table; `GuardFlags 0x100` plus the guard pointer come from prebuilt CRT
  objects. See `NOTES/verification.md`. The rebuilt build therefore omits
  compile- and link-time CFG.
- Debug info: `/DEBUG` with RSDS CodeView record (PDB path present, no PDB
  embedded).
- **`/GL` + `/LTCG` confirmed** (INF-009): cross-TU inlining of
  `ImGui_ImplWin32_Init`/`ImGui_ImplDX9_Init` into `Hook_EndScene`, a
  specialized `MH_CreateHook` clone with constant detour/trampoline,
  merged `MH_EnableHook`/`MH_DisableHook`, and LTCG register conventions on
  Kiero's internal `init`. Rebuild must use `/GL` on all TUs and `/LTCG`.
- Optimization level `/O2` (release, inlined, frame-pointer omission on some
  functions).
- Installed on this machine: VS 2022 Community with **MSVC 14.44.35207 only**.
  Exact rebuild match likely needs the v14.38.33130 toolset installed (optional;
  14.44 can be used with documented codegen differences).

## Imports (14 modules)

- `KERNEL32.dll` (49): VirtualAlloc/Protect/Query/Free, HeapCreate/Alloc/
  ReAlloc/Free, CreateToolhelp32Snapshot/Thread32First/Next,
  SuspendThread/ResumeThread/GetThreadContext/SetThreadContext/OpenThread,
  K32GetModuleInformation, GetProcAddress/GetModuleHandleA/W,
  GetPrivateProfileStringA/WritePrivateProfileStringA/CreateDirectoryA,
  IsDebuggerPresent, CreateThread, Sleep, ...
- `USER32.dll` (30): window class/window creation, input (`GetAsyncKeyState`,
  `GetKeyState`, `GetCursorPos`), clipboard, `SetWindowLongA`/`CallWindowProcA`,
  `EnumWindows`, `SetCapture`, `SetCursorPos`, ...
- `SHELL32.dll` (1): `SHGetFolderPathA`
- `MSVCP140.dll` (3): `_Query_perf_counter/frequency`, `_Xlength_error`
- `IMM32.dll` (3): IME context/composition window
- `XINPUT1_3.dll` (2): ordinals 2/4
- `VCRUNTIME140.dll` (14): EH, memcpy/memset/memmove, `__std_*`
- UCRT API sets: stdio (12), heap (4), utility (3), time (1), runtime (12),
  string (1: `_stricmp`), convert (1: `atof`), math (10)

## IDA / MCP status (blocker)

- IDA Professional 9.0 installed at `C:\Program Files\IDA Professional 9.0`
  (`ida.exe`, `idat.exe`; idalib available).
- IDA-side plugin present:
  `C:\Users\Ahmad\AppData\Roaming\Hex-Rays\IDA Pro\plugins\mcp-plugin.py`
  → starts a JSON-RPC HTTP server at `http://localhost:13337` when IDA runs.
- IDA is **not running**; no `ida_*` MCP tools are exposed to this session.
- IDA 9's `plugins\hexx64.dll` contains `HEXCX86`/`HEXCX64` feature strings,
  suggesting x86+x64 decompilation is packaged in that plugin in IDA 9; confirm
  live once MCP is up. (`hexrays.dll` is not present, which is expected for 9.0
  if x86 was merged into `hexx64`.)

## Consuming projects

None in the workspace. Only `VR.dll` is present; no headers, caller code, or
injector. The DLL is injection-driven and has no exports.

## Workspace artifacts created

- `original/target.dll` — read-only copy of `VR.dll` (hash verified)
- `STATE.md` — this file
- `NOTES/phase0-recon.md` — full evidence log
- `NOTES/inferences.md` — inference log (`INF-*`)
- `NOTES/pe_recon_output.txt` — captured output of the recon script
- `tools/pe_recon.py` — reproducible Phase 0 recon script:
  `uv run --with pefile --with capstone tools/pe_recon.py`
- `src/`, `include/`, `tests/`, `tools/` — empty scaffolding for later phases

## Decisions (confirmed 2026-10-25)

1. **Target:** `VR.dll` is the intended target. Proceed as **32-bit x86**; the
   x64-specific rules section is not applicable.
2. **IDA access:** user will start IDA and the MCP server. Path chosen: live
   MCP (not batch mode).
3. **Toolchain:** install **MSVC v14.38.33130** (VS 2022 17.8) for closest
   codegen.
4. **Verification host:** L4D2 + original injector are available for live
   in-process behavioral comparison.

## Execution status (2026-10-25)

- `ida-pro-mcp` is **not installed globally**; the hook blocks persistent
  `uv tool install`. Verified working one-off command:
  `uvx --with "mcp<2" ida-pro-mcp --help` (the `mcp<2` pin is required — the
  package is incompatible with mcp 2.x / missing `mcp.server.fastmcp`).
- Cherry Studio stdio MCP config to use (name: `ida-pro-mcp`):
  - command: `C:\Users\Ahmad\.cherrystudio\bin\uvx.exe`
  - args: `--with`, `mcp<2`, `ida-pro-mcp`
  - The bridge defaults to the IDA RPC endpoint `http://127.0.0.1:13337`.
  - `--unsafe` gates only the debugger methods; it is **not** needed for
    static recovery (rename/type/decompile/xrefs are available without it).
- IDA plugin `mcp-plugin.py` does **not** start automatically on load. In IDA:
  **Edit → Plugins → MCP (Ctrl+Alt+M)**; the console must print
  `[MCP] Server started at http://localhost:13337`. The plugin serves custom
  JSON-RPC at `/mcp`, not the MCP protocol; the `ida-pro-mcp` bridge is
  mandatory.
- IDA has **not** been started yet; port 13337 is not listening.
- VS 14.38 install: **SUCCESS** (elevated, exit 0). Installed toolsets:
  - `14.38.33130/` (servicing 14.38.33145)
  - `14.44.35207/`
- Verified x86 compiler environment via `tools/check_toolchain.bat`:
  - `cl.exe`: Microsoft (R) C/C++ Optimizing Compiler **19.38.33145 for x86**
  - `link.exe`: **14.38.33145.0**
  - CMake 3.31.6-msvc6 and MSBuild 17.14 are present.
  - The original binary's linker reports 14.38 and Rich-header build 33130;
    the installed 14.38.33145 servicing build is the closest available.

## “rea IDA” MCP check (2026-10-25)

- Cherry Studio has an active MCP entry named **`rea IDA`**
  (`b8171688-454e-47b9-8d59-34f8aa346582`):
  - command `cmd /d /c npx -y rea-agents@6.4.0 mcp`
  - env `REA_IDA_MCP_CONFIG=C:/Users/Ahmad/.rea-ida/ida-mcp.json`,
    `REA_ANALYSIS_PROVIDER=ida`
  - the config launches
    `C:/Users/Ahmad/.rea-ida/venv/Scripts/ida-pro-mcp.exe --ida-rpc
    http://127.0.0.1:13337`
- Status: **connected** (Cherry log: `Server connected`, REA 6.4.0).
- Binding: only to agent `a1cb714d…` (“Cherry Assistant”). The current agent
  (`71cc9709…`, “Software Agent”) has **no** MCP-server bindings.
- REA tool surface: 170 tools (captured in `tools/rea_tools.json`). It has
  `open_binary`, `list_procedures`, `procedure_pseudo_code`,
  `procedure_assembly`, `procedure_info`, `xrefs`, `list_strings`,
  `search_strings`, `list_names`, `list_segments`, `read_bytes`,
  `analyze_function`, `batch_decompile`, `get_call_graph`, `compare_functions`,
  `annotate_native_function` (rename/comment), `set_address_name(s)`,
  `set_comment`, `inspect_native_data_type`, `trace_native_values`, etc.
- **Gap:** REA has no `set_function_prototype`, `declare_c_type`,
  `set_local_variable_type`, `set_global_variable_type`, stack-frame type
  editing, or struct creation. Phase 2 (types/structs) and parts of Phase 3
  require those; the raw `ida-pro-mcp` exposes them.
- **Conclusion:** “rea IDA” is IDA-backed but is the high-level REA wrapper,
  not the low-level IDA MCP the brief expects. The direct bridge is present in
  the REA venv (`ida-pro-mcp 1.4.0`, `mcp 1.30.0`) and exposes the full
  59-tool surface.

## Root cause of the missing port 13337

- IDA was configured to use **Python 3.10.5**
  (`HKCU\Software\Hex-Rays\IDA\Python3TargetDLL =
  C:\Program Files\Python310\python310.dll`).
- `mcp-plugin.py` starts with:
  `if sys.version_info < (3, 11): raise RuntimeError(...)`, so the plugin
  could not load. This is why REA’s stdio server connected but IDA’s HTTP
  server on 13337 never appeared.
- Fixed with `idapyswitch.exe -s
  "C:\Users\Ahmad\AppData\Local\Programs\Python\Python312\python3.dll"`.
  Registry now points at **Python 3.12.10**. IDA was not running during the
  switch. Reversible with the same tool.
- **Direct bridge registered:** Cherry MCP server `ida-pro-mcp`
  (`bd377630-8c95-4584-8a17-fe8c27da05c1`), stdio, command
  `C:\Users\Ahmad\.rea-ida\venv\Scripts\ida-pro-mcp.exe`,
  args `--ida-rpc http://127.0.0.1:13337`, active and bound to this Software
  Agent (`71cc9709…`). Cherry log: `Server connected` (18:36:46).
- Still needed: relaunch IDA (Python 3.12 now), open `original/target.dll`,
  press **Ctrl+Alt+M** so the plugin starts its server on 13337. The bridge
  tools will then work.

## IDA MCP capability map (from mcp-plugin.py, 59 RPC methods)

Available: `get_metadata`, `list_functions`, `decompile_function`,
`disassemble_function`, `get_function_by_name/address`, `get_xrefs_to`,
`get_xrefs_to_field`, `get_callees`, `get_callers`, `list_imports`,
`list_strings`, `list_globals`, `list_local_types`, `get_entry_points`,
`read_memory_bytes`, `data_read_*`, `search_structures`,
`get_defined_structures`, `analyze_struct_detailed`, `get_stack_frame_variables`,
`rename_function`, `rename_local_variable`, `rename_global_variable`,
`set_function_prototype`, `set_local_variable_type`, `set_global_variable_type`,
`declare_c_type`, `set_comment`, `patch_address_assembles`, plus 12 debugger
methods.

Gaps / notes:
- **No generic Python-eval / script-execution RPC** and **no FLIRT / save-DB
  RPC**. Bulk automation must use the paginated tool calls; FLIRT signature
  application and saving the IDB may need a manual IDA action (or a separate
  `idat`/`idalib` step on a copy). Plan for a hybrid if the user approves.
- Only the 12 `dbg_*` methods are marked `unsafe`. Rename/type/decompile/xrefs
  work without `--unsafe`; **omit `--unsafe` for static recovery** unless we
  need the debugger.

## Verification host and injector

- L4D2 executable (user-confirmed):
  `C:\Program Files (x86)\Steam\steamapps\common\Left 4 Dead 2\left4dead2.exe`
- Injector: `SKernelInjector.exe` in the workspace root (19,824 bytes,
  SHA-256 `085f6610…72cf`).
  - 32-bit PE32 console app, linker 14.30, timestamp 2022-04-20.
  - Manifest requires administrator (`requestedExecutionLevel
    level='requireAdministrator'`).
  - Hardcodes process name `left4dead2.exe`.
  - Loads `.\VR.dll` relative to the current working directory and calls
    `LoadLibraryA` in the target process; prints
    `Successfully injected, Please close this window!` on success.
  - On failure prints `Injection failed!`, `Failed getting %s process ID,
    Make sure your game called left4dead2.exe`, plus troubleshooting text.
  - PDB path `E:\l4d2\Win32\Release\SKernelInjector.pdb`.
- Harness implication: the rebuilt DLL must be staged as `VR.dll` next to a
  copy of the injector (never overwrite `original/target.dll`). Run the
  injector with CWD set to the staging directory. Use separate staging runs
  for original vs rebuilt, since only one `VR.dll` can be injected per game
  session.
- No PDB/source remnants found under the user profile; `C:\L4D2space\` does
  not exist yet (the cheat creates it on first run).

## IDA Phase 0/1 results (2026-10-25)

Full evidence: `NOTES/ida-recon.md`; snapshots in `NOTES/ida/`.

- **509 functions**, 688 strings, 61 structures. IDA named 89; 420 remain
  `sub_*`.
- **Library boundary map (by link order + call graph):**
  - `0x10001010–0x1000E360`: 110 funcs — user features/netprops/hook bodies.
  - `0x1000E360–0x10032AD0`: 234 funcs / 147 KiB — **ImGui core** (library).
  - `0x10032AD0–0x10033EE0`: 9 funcs — Kiero + engine bridge; `sub_10033900`
    is user interface setup, `sub_10033B10` is Kiero init.
  - `0x10033EE0–0x10035BF0`: 12 funcs — **MinHook 1.3.3 + HDE64**.
  - `0x10035BF0–0x10037B00`: 16 funcs — user utilities (pattern scanner etc.).
  - `0x10037B00–0x1003DF80`: 23 funcs — user menu/ESP/config.
  - `0x1003DF80–0x1003F4A0`: 101 funcs — **CRT/SEH/security**.
  - `0x1003F4A0+`: 4 tiny CRT/security helpers.
- **Entry chain:** `DllEntryPoint` → `__security_init_cookie` →
  `dllmain_dispatch` → `_DllMain@12` (0x1000BA00) → `CreateThread(StartAddress)`.
  `StartAddress` (0x1000B960) → Kiero init (0x10033B10) → MinHook EndScene
  hook (0x10034AE0) → EnumWindows (0x1000B920) → WndProc hook (0x1000B830) →
  wait for End key.
- **Hooks:**
  - D3D9 `EndScene` via MinHook → `sub_10009160` (ImGui + lazy init).
  - `IPanel` slot 41 (`PaintTraverse`) VMT → `sub_10004FC0`.
  - `IVEngineClient` slot 20 VMT → `sub_100076A0` → main update `sub_100055B0`.
  - `IVModelRender` slot 19 (`DrawModelExecute`) VMT → `sub_10004980` (chams).
- **12 engine interfaces resolved** via `CreateInterface` in `sub_10033900`
  (full global/interface table in `NOTES/ida-recon.md`).
- **Key user functions:** menu `sub_10037B00` (86 cheat + 20 config strings),
  main update `sub_100055B0`, config `sub_10039AD0`/`sub_10039F20`, ESP/world
  renderers `sub_1003C410`/`sub_1003B030`/`sub_1003B820`, netprop accessors
  `0x100012B0`–`0x10001EC0`.
- **Call graph** and **string→function** maps saved:
  `NOTES/ida/callgraph.json`, `NOTES/ida/string_xrefs.json`.
- **RTTI:** `.?AUtracefitlers@@` (typo preserved) at 0x10049164.

## Runtime validation in L4D2 (2026-10-25)

Rebuilt `VR.dll` was injected into the running game via `SKernelInjector.exe`
(staged in `staging/rebuilt/`). Boot log from inside `left4dead2.exe`:

```
DllMain attach
worker thread created
thread start
kiero status 0
MH_CreateHook 0
MH_EnableHook 0
window found
wndproc hooked
```

This validates the entire recovered bootstrap path against the real engine:
Kiero D3D9 init, MinHook trampoline creation + enable on EndScene slot 42,
EnumWindows discovery, and the GWL_WNDPROC subclass. `tasklist /m VR.dll`
fails to list the module (elevation/module-enumeration artifact); the DLL was
proven loaded by the exclusive file lock that disappeared exactly when L4D2
was closed.

The temporary `LogBoot()` instrumentation in `src/dllmain.cpp` (writes
`staging/rebuilt/loaded.log`) must be removed or compiled out before the final
deliverable.

## Library identification (2026-10-25)

- **ImGui v1.74** (master, stock `imgui_impl_dx9` + `imgui_impl_win32`).
  Proven by compile-time layout probes; full evidence in
  `NOTES/ida-imgui-version.md`. Vendored at `third_party/imgui-1.74/`.
- **MinHook 1.3.3** (resource version + code fingerprint), vendored at
  `third_party/minhook/` (tag `v1.3.3`).
- **Kiero 1.2.9**, commit `9f41abe`, with `KIERO_INCLUDE_D3D9=1` and
  `KIERO_USE_MINHOOK=1`; vendored at `third_party/kiero-1.2.9/`.
  The binary preserves the period typo `vulcan-1.dll` from this revision.
- Toolchain: MSVC 14.38 x86, `/O2 /MD /GS /guard:cf /GL`, `/LTCG`,
  `/DEBUG`. MinHook was linked from a prebuilt older-toolset library
  (Rich-header builds 30729/30319); the rest appears built with 14.38.

## IDA naming progress (2026-10-25)

Evidence table: `NOTES/ida-naming.md` (do not duplicate names here).

- Renamed 16 functions: `DllMain`-adjacent thread/bootstrap chain, the three VMT
  detours, `Cheat_Update`, `Menu_Render`, `InitInterfaces`, `VmtHook_Init`,
  `Kiero_Init`, `MH_CreateHook`, `MH_EnableHook`, `FindPattern`.
- Renamed 28 globals: 12 `CreateInterface` results, `g_pGlobalVars`, 3 VMT hook
  objects, 3 saved original function pointers, ImGui context, menu state,
  Kiero/D3D9/window/MinHook state.
- Key correction from naming: `dword_1004A9E0` is `g_pGlobalVars`
  (`PlayerInfoManager002` slot 1), not a local-player pointer — its only uses
  multiply tick counts by `*(float*)(g + 28)` (`interval_per_tick`).
- `g_pImGuiContext` offsets recorded as the ImGui version-matching fingerprint
  (`KeyMap` at +0x34, `MousePos` +0xE8, `MouseDown` +0xF0); stock ImGui
  assumptions must be verified by compiling candidates, not by hand.
- IDB comments added at every renamed function entry (provenance + inference
  marker).

## Next actions (current)

0AC. **Teleport + Air Stuck ported — NEW BUILD STAGED**
   - User verified Thirdperson ("expected live behaviour is verified!"); 0AB
     closed.
   - Target Cheat_Update 0x10005C8E..0x10005CFE (machine code checked with
     dumpbin; see INF-020):
       * Teleport (byte_1004AC1C, key dword_1004ABAC): while enabled and the
         key is unbound or held, writes 0x7FC00000 (quiet NaN) to
         cmd+0x18 (forwardmove) and cmd+0x1C (sidemove).
       * Air Stuck (byte_1004AC1D, key dword_1004ABA8): while enabled,
         airborne (!FL_ONGROUND via sub_100015A0 with the local entity in
         ECX) and the key is unbound or held, writes 0x7FC00000 to
         cmd+0x18 only (sidemove untouched).
     Both blocks are emitted before AutoStrafer, matching the target's
     write order (AutoStrafer may overwrite the NaN).
   - ABI note: the pseudocode's `sub_100015A0(v137)` is really
     `mov ecx, esi; call 100015A0`; sub_100015A0 is a __thiscall flags
     getter. The decompiler's v137=0/1 assignments are an unrelated temp.
   - Menu Misc tab now exposes the real widgets in target order: Bunny Hop,
     SpeedHack (PENDING logic), Air Stuck + keybind (g_AirStuckKey),
     Teleport##jfsdagkfs + keybind (g_TeleportKey), Fake Lag (PENDING
     logic). Config: Menu_Render 0x10037B00 is the settings-descriptor
     builder (called once from sub_10001050); the same descriptor is consumed
     by SaveSettings 0x10039AD0 and LoadSettings 0x10039F20, so all of
     "TeleportKey", "AirstuckKey", "Teleport", "Airstuck", "SpeedHack",
     "FakeLag" are persisted (correction of the earlier save-path note).
   - Expected live:
       * Teleport: enabling with no keybind applies NaN movement every tick
         (prediction-error jumps/rubber-banding); binding a key limits it to
         while the key is held. Compare against original/target.dll with the
         same keybind.
       * Air Stuck: jump first, then (with no keybind) horizontal movement
         freezes mid-air; with a keybind it freezes only while held. Disable
         to fall normally.
       * SpeedHack / Fake Lag checkboxes are wired but inert until their
         slot-20 stack-chain logic is ported.
   - **Revision 2 (user reported Teleport had no effect; Air Stuck close but
     ineffective).** Root cause found in the slot-20 detour, not the NaN
     writes: the target (0x100076A0 assembly) calls Cheat_Update only when
     `cmd->command_number == [caller EBP + 8]` (CInput::CreateMove's
     sequence_number), and then writes the number back. Our previous
     "command_number changed" dedup could run Cheat_Update on a stack backup
     cmd first, then skip the real command at the CreateMove call site.
     src/hooks.cpp now reproduces the target condition exactly, including
     the write-back, and logs a 24-call diagnostic burst (`s20 esi=... ebp=
     ... arg=... carg=... eq=...`) plus per-second `s20 calls/s=... match=`
     counters. Slot 20 is IVEngineClient::SetViewAngles (slot 19 is
     GetViewAngles); the stack argument is &cmd->viewangles and ESI is the
     CUserCmd.
   - Staged: staging/rebuilt/VR.dll (518.0K, sha256 b9b33d95...); clean
     rebuild with /GL; loadtest LOAD_OK; export table still empty (matches
     target, which has no exports). loaded.log rotated to loaded.log.bak so
     the next run's s20 diagnostics are isolated.
   - **CLOSED (verified by equivalence):** user tested this build and the
     original VR.dll side by side on the current game build: Teleport and
     Air Stuck are inert in BOTH. The game changed; the target's NaN
     movement no longer produces the remote teleport / air freeze. Our port
     matches the original's (dead) behavior, and the target-exact slot-20
     call-site condition is live-verified (game ran, no regression).
0AD. **SpeedHack ported (slot-20 stack-chain hack) — NEW BUILD STAGED**
   - Target Hook_EngineClient_Slot20 0x100076DD..0x1000772A, reproduced in
     EngineSlot20_Process inside the command-match branch, before
     Cheat_Update (target order):
       * gate: byte_1004B024 (SpeedHack) && dword_1004B02C (value) > 0;
       * GetAsyncKeyState(dword_1004B028) tested as the whole AX result
         (held OR pressed-since-last-call), not only 0x8000;
       * counter dword_1004A96C: if old value > 0, decrement and subtract 5
         from dword at the final stack-chain pointer +4; on key-up or when
         the old counter <= 0, reload it from dword_1004B02C. Net effect:
         `value` subtractions followed by one skipped update.
       * stack chain from the caller EBP: p1=[callerEBP], p2=[p1], p3=[p2];
         `add dword ptr [p3+4], -5` (target 0x10007711..0x1000771B).
     SAFETY (INFERRED): IsBadReadPtr/null checks on every chain step; valid
     engine pointers take the same path as the target.
   - Also ported target 0x10007738: after Cheat_Update and the command-number
     write-back, AND byte [(second chain pointer)-0x1D] with
     HIBYTE(g_MenuState). With g_MenuState byte 3 always 1 outside Fake Lag
     this is a no-op today; it is the packet-choke mask Fake Lag will use.
   - Menu Misc tab reordered to the exact target EndScene sequence:
     Bunny Hop, SpeedHack + keybind (g_SpeedHackKey), Air Stuck + keybind,
     Teleport + keybind, Fake Lag, "Fake Lag##LagValue" slider 0..16,
     "SpeedHack##SpeedValue" slider 0..14. Defaults from sub_1003A300:
     SpeedHackValue=0, SpeedHackKey=115 ('S'), FakeLagValue=14.
   - Config (descriptor consumed by both save 0x10039AD0 and load
     0x10039F20): SpeedhackVal, SpeedhackKey, FakeLagValue, SpeedHack,
     FakeLag, Teleport, Airstuck are now read and written with those exact
     keys; menu/save updated accordingly (previous "never saved" note was
     wrong).
   - Expected live: Misc -> SpeedHack on, raise the SpeedHack slider above 0,
     hold S (default key) or bind another key -> movement speed increases
     while held; releasing the key resets the counter. Compare against
     original/target.dll with the same value/key. If the original is dead on
     the current game build, both builds must stay inert (equivalence).
   - Staged: staging/rebuilt/VR.dll (519.0K, sha256 06a81270...); clean
     /GL rebuild, loadtest LOAD_OK; the s20 diagnostic burst/rate logs from
     0AC are still present for this test. loaded.log rotated to
     loaded.log.prev.
   - **CLOSED (verified live):** user confirmed the expected SpeedHack
     behaviour against original/target.dll with the same value/key.
0AE. **Fake Lag ported (counter + packet-choke mask) — NEW BUILD STAGED**
   - Target Cheat_Update 0x10005F4A..0x10005F76, reproduced after the
     AutoStrafer block and before the anti-aim block (exact target order):
       * gate byte_1004AC0E (menu "Fake Lag");
       * counter dword_1004A970 (BSS, starts 0): if <= 0, reload from
         dword_1004AC10 (slider 0..16, default 14) and do not choke; else
         decrement and clear HIBYTE(g_MenuState) = 0.
     Cheat_Update sets HIBYTE(g_MenuState) = 1 at its start, so the counter
     clears it for `value` consecutive updates and releases for one.
   - The slot-20 detour already applies the matching mask: after
     Cheat_Update it ANDs byte [(second chain pointer)-0x1D] with
     HIBYTE(g_MenuState) (target 0x10007738). With the bit cleared the
     engine's send-packet byte is masked to 0, holding the packet; on the
     release update the mask is 1 and the packet goes out.
   - Anti-aim block moved after Fake Lag so its desync test
     (`(g_MenuState >> 24) != 0`) sees the choke-cleared byte, as compiled
     in the target. Relative order now matches: Teleport, Air Stuck,
     AutoStrafer, Fake Lag, anti-aim, Bunny Hop, ...
   - The "Fake Lag (Choking)" on-screen label seen at Hook_EndScene
     0x10009AE6 belongs to the EndScene status overlay gated by
     byte_1004B06C (the "Indicators" checkbox at 0x1000B744), not to the
     Fake Lag feature; it is not drawn by this stage. Note: 0Q closed the
     unrelated PaintTraverse Indicator panel (gate dword_1004B048, dead);
     the EndScene overlay (watermark/SMAC warning/Fake Lag label) is still
     PENDING.
   - Expected live: Misc -> Fake Lag on, slider > 0 (default 14), in a
     running game -> outgoing commands are held for `value` ticks then one
     is released; visible as movement stutter/rubber-banding and choking.
     Compare against original/target.dll with the same slider value.
   - Staged: staging/rebuilt/VR.dll (519.0K, sha256 b58dfa7e...); clean
     /GL rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.speedhack.
   - **CLOSED (verified live):** user confirmed the expected Fake Lag
     behaviour against original/target.dll at the same slider value.
0AF. **Legit tab rebound + FOV Circle overlay — NEW BUILD STAGED**
   - Target Hook_EndScene 0x10009845..0x1000990E (FOV Circle), ported in
     src/hooks.cpp as DrawFovCircleOverlay:
       * gate byte_1004AB7A ("FovVisualize" config key, Legit tab
         "FOV Circle") and IVEngineClient slot 26 (in-game gate);
       * GetScreenSize via slot 5; center = (width/2, height/2);
       * radius = (float)g_LegitAimFov / (float)g_FovDesired *
         (float)(width/2) - 0.5f;
       * white closed polyline on ImGui's background draw list
         (GImGui+0x1C40). The target's inlined AddCircle constant
         0x40C25C20 is 2*pi*29/30, i.e. num_segments=30; our compiled
         sequence now matches it exactly (`push 1Dh` + 0x40C25C20).
       * Drawn regardless of the Legit aimbot Enabled flag (target
         behaviour) and before the menu, so the circle sits under it.
     Safety guards (INFERRED): require g_FovDesired > 0 and a valid
     engine/vtable/screen size; the target divides by zero unguarded.
   - Legit tab rebuilt from Hook_EndScene 0x10009DBA..0x1000A2AC with the
     real target globals instead of menu-local statics, exact group order:
       Aimbot: Enabled (byte_1004AB3C) + keybind (dword_1004AB40),
         Hitbox Auto/Head Only (dword_1004AB48), Shotgun Mode (AB4C),
         FOV 0..15 (AB50), Smooth Y 0..20 (AB58), Smooth X 0..20 (AB54);
       Accuracy: No-Recoil (AB4D), No-Spread (AB4E), Position Adjustment
         (AB5C), FOV Circle (AB7A);
       Target: Priority Special (AB60), Hitscan (AB5F), Ignore Team (AB5E),
         Ignore Idle Witch (AB5D);
       Triggerbot: Enabled (AFFC) + keybind (ABA4), Delay 0..15 (B000),
         Aim At Head (AFFD), Aim At Body (AFFE), Auto Shove (AB61),
         Shove FOV 0..15 (AB64).
   - Config descriptor keys rebound (Menu_Render 0x10037B00 table):
     ENABLEAIMLEGIT, LegitAimKey, LegitAimHitbox, LegitAimHitboxMode,
     LegitShotgunMode, LEGITNORECOIL, LEGITNOSPREAD, LEGITAIMFOV,
     LEGITSMOOTHYAW (= Smooth X), LegitLagComp (= Position Adjustment),
     LegitIGNOREWITCH, LegitIgnoreTeam, LegitbHitscanMode,
     Legitpriorityspecial, LegitAutoShove, LegitAutoShoveFov, TriggKey,
     triggerbody, FovVisualize. Target quirks preserved: Smooth Y,
     Triggerbot Enabled, Delay and Aim At Head are not persisted.
   - Expected live: Legit tab -> check "FOV Circle", raise FOV above 0 ->
     a white circle appears centered on the game screen while in a running
     game; radius grows with the FOV slider and shrinks as the Player FOV
     (Visuals -> FOV Changer value) grows. FOV 0 draws a degenerate circle.
     Compare against original/target.dll with the same FOV and Player FOV.
   - Staged: staging/rebuilt/VR.dll (522.0K, sha256 a7cb0d45...); clean
     /GL rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.fakelag.
   - **CLOSED (verified live):** user confirmed the FOV Circle appears and
     tracks the FOV/Player FOV settings like original/target.dll.
0AG. **Legit block head + No-Recoil — NEW BUILD STAGED**
   - Target Cheat_Update 0x10007222..0x100073B3, first ported part of the
     Legit block:
       * gate: byte_1004AB3C (Legit "Enabled"), local entity valid,
         team == 2 (survivor), and sub_100046B0 = active weapon
         DT_BaseCombatWeapon::m_iClip1 > 0 (active weapon from
         DT_BaseCombatCharacter::m_hActiveWeapon through
         GetClientEntityFromHandle).
       * 0x10007350 No-Recoil (byte_1004AB4D): copy
         DT_BasePlayer::m_vecPunchAngle and subtract all three components
         from cmd->viewangles with scale 1.0 (target 0x10047328 = 1.0f).
     Implementation reuses GetEntityPunchAngle (exact sub_100018B0 port) and
     the existing netprop resolver. One-shot log `legit weapon off=%d
     clip=%d` records the resolved offsets. SAFETY/INFERRED fallback: if the
     netprop offsets cannot be resolved, the clip gate passes so No-Recoil
     remains usable; the target would simply not run the block.
   - Still PENDING inside the same Legit block: Triggerbot
     (sub_100353F0, EngineTrace ray + hitgroup choice), Auto Shove
     (sub_100359D0, 82-unit/Shove FOV check + IN_ATTACK2), Legit aimbot
     (sub_10035BF0, 0xC71 bytes), No-Spread (0x100073B3, weapon-spread
     path). The aim/trigger key checks (dword_1004AB40 / dword_1004ABA4)
     and the tick-timing gate are not ported yet; with no key bound the
     target performs no aim/trigger actions, so this stage isolates
     No-Recoil.
   - Expected live (survivor, weapon with ammo, no aim/trigger key held):
     Legit tab -> Aimbot Enabled ON + Accuracy No-Recoil ON -> fire an
     automatic weapon and compare the view kick against
     original/target.dll; No-Recoil subtracts m_vecPunchAngle so the aim
     should not climb. Bullet spread remains (No-Spread is PENDING).
   - Staged: staging/rebuilt/VR.dll (523.0K, sha256 09bdb132...); clean
     /GL rebuild, loadtest LOAD_OK; loaded.log rotated to
     loaded.log.fovcircle.
   - **User report: "not working".** The test log contains no
     `legit weapon off=...` line, so the Legit block never reached the clip
     gate: either g_LegitEnabled was false, the local entity/team gate
     failed, or the checkbox state was not what we expect. Added a TEMP
     diagnostic build (sha256 2006495c...):
       * `legit gate en=%d nr=%d local=%p team=%d` (max 12 lines whenever
         either Legit flag is on, logged at the end of Cheat_Update);
       * LocalWeaponHasClip is now evaluated whenever the local entity is
         valid (still only applied to No-Recoil when team==2 && clip>0), so
         `legit weapon off=... clip=...` appears even if the team gate
         fails;
       * `norecoil punch=...,...,... va=...` (max 8 lines) while IN_ATTACK
         is held with the feature active.
     loaded.log rotated to loaded.log.norecoil1 for the next run.
   - **CLOSED (verified by equivalence):** user tested the feature against
     original/target.dll and the original is inert too; No-Recoil is a dud
     in the current game build. The diagnostic log for that run contains no
     `legit gate` lines (the flags were not both active during the run), but
     the equivalence result is conclusive. Auto Shove was analysed as
     geometrically broken in the target as compiled ((enemyEye -
     viewangles) normalized dotted with localEye) and is therefore also
     dead by equivalence; it was not staged.
0AH. **EndScene status overlay (scam text + SMAC warning) — NEW BUILD STAGED**
   - Target Hook_EndScene 0x1000980F..0x10009840, ported as
     DrawEndSceneOverlay in src/hooks.cpp:
       * permanent white outlined text at (10,10):
         "IF YOU PAID FOR THIS CHEAT YOU GOT SCAMMED, PLEASE REFUND."
         The string was XOR-decoded from 0x10047690/0x10047610/0x10047620 +
         immediates with key (i - 71);
       * when SMAC mode is off (_BYTE word_1004AC0C == 0), yellow
         (0xFF00FFFF) outlined text at (10,25): "SMAC MODE IS DISABLED!";
       * both use ImGui's background draw list (GImGui+0x1C40) like the
         verified FOV circle; the outline is four black passes at (+/-1,
         +/-1) then the main colour (target sub_10028CC0).
     The FOV circle draw is now part of the same overlay function and keeps
     its verified behaviour.
   - Expected live: with any injected build, the white anti-resale line is
     visible at the top-left of the game screen together with the yellow
     SMAC warning; toggling Misc -> SMAC Mode ON removes the yellow line,
     toggling it off restores it. Compare with original/target.dll.
   - Staged: staging/rebuilt/VR.dll (524.0K, sha256 3e326c25...); clean
     /GL rebuild, loadtest LOAD_OK; loaded.log rotated to
     loaded.log.norecoil2.
   - **CLOSED (verified live):** user confirmed the white anti-resale line
     and the yellow SMAC warning appear exactly like original/target.dll,
     and SMAC Mode toggles the warning.
0AI. **Rage tab rebuilt with real globals — CLOSED (verified live)**
   - Target Hook_EndScene Rage tab 0x1000A2B1..0x1000A749 mapped in full:
       Aimbot: Enabled (byte_1004AB7B) + keybind (dword_1004AB9C),
         Hitbox Auto/Head Only (dword_1004AB6C, not persisted by the
         target), Auto Shoot (ABC0), Silent Aim (ABC2), Perfect Silent Aim
         (ABC3), Shotgun Mode (AB79), Auto Shove (ABC4), Auto Melee (ABC5),
         FOV 0..180 (dword_1004ABB0);
       Accuracy: No-Recoil (ABBE), No-Spread (ABBF), Position Adjustment
         (AB7E), Prediction Fix (AB99);
       Target: Priority Special (ABC1), Hitscan (AB78), Ignore Team (ABBD),
         Ignore Idle Witch (ABC6);
       Hack vs Hack: Enabled (ABFC), Pitch (AC00, None/Fake Up/Fake Down/
         Fake Zero), Real Yaw (AC04, None/Left/Right/Backwards), Desync Yaw
         (AC08, same items).
   - Fixed a real wiring bug: menu.cpp had file-local statics named
     g_RagePitch/g_RageRealYaw/g_RageDesyncYaw that shadowed the cheat
     globals, so the anti-aim combos did not reach Cheat_Update. The statics
     and the dummy kRageA/B/C checkbox arrays are removed; the full Rage tab
     now binds the real globals.
   - Config keys rebound exactly: "Aimbot" (group label reused for the
     Rage Enabled flag), AimKey, AimFov, AutoFire, Silent, PSilent,
     ShotgunMode, AutoShove, AutoMelee, NoRecoil, NoSpread, LagComp,
     PredictionFix, priorityspecial, bHitscanMode, IgnoreTeam, IgnoreWitch,
     AntiAim, Pitch, Yaw, FakeYaw. Target quirks preserved: Hitbox (AB6C)
     is not persisted; the descriptor's unused "AimHitbox" maps to AB68.
   - Scope: the Hack vs Hack anti-aim block is fully implemented and is the
     only Rage group with live logic today. The Aimbot/Accuracy/Target
     checkboxes are bound to their real flags and persist, but the aimbot
     body (sub_10003380 / sub_10035BF0) is still PENDING, so they do not
     aim or fire yet.
   - Expected live: Rage tab matches the original layout/labels/ranges;
     toggling/selecting controls persists through save/load; Hack vs Hack
     Enabled + Pitch/Real Yaw/Desync Yaw still produce the verified
     anti-aim behaviour (now actually driven by the combos).
   - Staged: staging/rebuilt/VR.dll (527.0K, sha256 9a7d1978...); clean
     /GL rebuild, loadtest LOAD_OK; loaded.log rotated to
     loaded.log.overlay.
0AJ. **Legit triggerbot ported — CLOSED (verified live)**
   - Target sub_100353F0 (0x341 bytes) ported in src/cheat.cpp:
       * Ray_t (0x48) and L4D2 trace_t (0x58; hitgroup +0x44, hit entity
         +0x4C; +0x48 is physicsbone/worldSurfaceIndex) proven from the
         target stack frame;
       * ITraceFilter vftable 0x10041538 rebuilt as a 2-slot virtual class
         (slot 0: entity != local; slot 1: TRACE_EVERYTHING = 0), same
         8-byte object layout;
       * IEngineTrace slot 5 TraceRay with mask 0x46004003;
       * trace start = GetEntityEyePosition (sub_100014B0); direction from
         cmd->viewangles with 0.017453292f and forward*8192, stored as
         (start + forward) - start like the target;
       * hit filter = IsPlayerChamsEligible (sub_10001EC0) and team != 2;
       * hitgroup match: head (AFFD, hitgroup 1), body (AFFE, 2/3),
         limbs (dword_1004B004 != 0, 4..7 - the flag has no writer in the
         target, so limbs are dead); on match cmd->buttons |= 1.
   - Target quirks preserved: Delay 0 never fires; Delay 1..15 yields
     Delay/30 == 0, so the counter threshold is zero and the shot is
     immediate; the "Enabled##trigger" checkbox (byte_1004AFFC) is never
     read by the target, so the trigger key alone arms it whenever Legit is
     enabled, the local team is 2 and the active weapon has m_iClip1 > 0.
   - Config: "trighead" (AFFD) added to Load/Save (was missing); Triggerbot
     Enabled and Delay remain unpersisted in the target.
   - Size: Legit_Triggerbot 0x3A0 vs target 0x341 (extra safety guards and
     the temp hit log).
   - Expected live: Legit Enabled + survivor + weapon with clip; holding the
     Trigger key while the view ray hits an infected's selected hitgroup
     makes the gun fire. Delay 0 must not fire; 1..15 fires instantly.
     "Enabled##trigger" must have no effect. Compare with original.
   - Staged: staging/rebuilt/VR.dll (528.0K, sha256 2af5288a...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.triggerbot.
0AK. **Legit aimbot ported — CLOSED (verified live)**
   - Target sub_10035BF0 (0xC71 bytes) ported in src/cheat.cpp with its
     helpers sub_100026E0 (LOS trace), sub_100352B0 (hitbox index),
     sub_10035740 (hitbox bbox transform), sub_10001AA0 / sub_100017A0 /
     sub_100019A0 / sub_10002020 netprops, and the engine/cvar accessors.
   - Candidate loop (entity indices 1..GetHighestEntityIndex):
       * skip local, sub_10001EC0 failures, zero model, ghost infected
         (DT_TerrorPlayer m_isGhost compare set), optional Ignore Team,
         optional Ignore Idle Witch (DT_Witch m_rage != 1.0), team 4,
         dormant, m_usSolidFlags == 20;
       * SetupBones (entity+4 slot 13, 128 bones, mask 256);
       * hitbox index from group (Shotgun Mode forces group 3 for weapon
         ids 11/8/3/4; Hitscan + Auto returns hitbox 0);
       * bbox corners transformed by the bone matrix, midpoint + LOS trace
         (mask 0x46004003, skip local) required;
       * priority 2.0 for category 3, 3.0 for category 5, else 1.0;
       * FOV cone acos(clamp(dot(forward, dir),-1,1))*57.295776 compared to
         g_LegitAimFov (slider 0..15); distance compared to best distance
         (99999.9 initial, not updated when Priority Special is on);
       * per-candidate tail (even when not selected): class name, health /
         max health (100/100 for category 4), Position Adjustment tick_count
         from cl_interp / cl_interp_ratio / cl_updaterate and local
         m_flSimulationTime, and the auto hitbox mapping
         (264/0/263 -> 2, 99/265 -> 1, 270 -> 3, 276/277 -> 0).
   - Post-loop: pitch = atan((eye.z-aim.z)/horizontal), yaw =
     atan((eye.y-aim.y)/(eye.x-aim.x)) + 180 when dx >= 0; angles
     normalized to [-180,180]; optional smoothing divides the pitch delta
     by Smooth Y and the yaw delta by Smooth X; final view applied through
     IVEngineClient slot 20 (SetViewAngles), which intentionally re-enters
     our slot-20 detour and is forwarded to the original.
   - Call-site gates (Cheat_Update 0x10007222..0x1000734B): Legit enabled,
     survivor team, m_iClip1 > 0, weapon whitelist (sub_10004790) and aim
     key; shotguns {11,8,3,4} bypass the can-fire check, otherwise
     m_flNextPrimaryAttack <= (m_nTickBase+1)*interval_per_tick. The
     byte_1004B2E4 term has no writer and is always zero.
   - Size: Legit_Aimbot 0xD00 vs target 0xC71.
   - Expected live: Legit Enabled + bound Aim key + FOV > 0; holding the
     key near an infected snaps the view to its hitbox (head/body/auto),
     with Smooth X/Y > 0 producing gradual movement. Position Adjustment
     must not break aiming; Ignore Team must skip survivors.
   - Staged: staging/rebuilt/VR.dll (536.5K, sha256 12d6d449...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.aimbot.
     TEMP diagnostics: one "aimbot run" line when the call gate first
     passes, then up to 8 "aimbot tgt=... cls=... dist=... ang=..." lines.
0AL. **Rage aimbot ported — CLOSED (verified live)**
   - Target sub_10003380 (0xE08 bytes) ported in src/cheat.cpp with its
     helpers sub_10002310 (hitbox index) and sub_10002450 (hitbox aim point
     + LOS) plus the Rage runtime state:
       eye 0x1004B120, view angles 0x1004B25C, forward 0x1004B248,
       target index 0x1004B100, best distance 0x1004B104, aim point
       0x1004B108, stored point 0x1004B114, stored cmd angles 0x1004B12C,
       has-target 0x1004B058, attack-suppressed 0x1004B057,
       SMAC flags 0x1004A92C/0x1004A8E8. Health/max-health/class-name are
       the shared 0x1004B060/B064/B05C fields.
   - Candidate scan and math mirror the Legit aimbot with Rage settings:
     Ignore Team (ABBD), Ignore Idle Witch (ABC6), priority 2.0/3.0
     (ABC1), FOV (ABB0), Position Adjustment (AB7E) only when SMAC mode is
     off. Hitbox group comes from dword_1004AB68 ("AimHitbox"), driven by
     the Hitbox combo dword_1004AB6C (0 Auto / 1 Head Only) with the same
     class->group auto mapping as the Legit aimbot. Shotgun Mode is AB79,
     Hitscan is AB78.
   - Post-loop aiming: Smooth (dword_1004AB70, config key "Smooth", no UI)
     is applied to both pitch and yaw only when Silent, Perfect Silent and
     Auto Shoot are all off. SetViewAngles is called when SMAC mode is on
     or Silent is off. Auto Shoot (ABC0) presses IN_ATTACK; Prediction Fix
     (AB99) + the Speedhack key (target quirk: dword_1004B028) throttles it
     to 120 ms via the target's QPC timer; without that, SMAC mode applies
     its ground-flag (m_fFlags & 1) attack suppression state machine.
   - Cheat_Update flow ported: Rage section gated on byte_1004AB7B, clip
     and team gates, weapon whitelist + Rage AimKey, shotgun/canFire gate,
     Perfect Silent call with command-angle/movement restore when no shot
     went out, Rage No-Recoil (ABBE), and the target's skip of the whole
     Legit section when the canFire gate fails.
   - Menu quirks recovered and implemented: drawing the Legit tab with
     Legit enabled clears Rage Aimbot Enabled and vice versa (target
     0x10009DBA / 0x1000A2B1). Config keys "AimHitbox" (AB68) and "Smooth"
     (AB70) added to Load/Save.
   - Still PENDING in this section: Rage Auto Shove (0x10006489..0x100067B7),
     the melee weapon-id-19 path (0x100067B7..0x10006A02), and the SMAC-mode
     angle prediction block (0x10006C2D..0x10006D97).
   - Size: Rage_Aimbot 0xEB0 vs target 0xE08.
   - Expected live: Rage Aimbot Enabled + bound AimKey + FOV > 0; holding
     the key snaps the aim to infected hitboxes like the original. Auto
     Fire shoots while a target is acquired; Silent suppresses the local
     view push; Perfect Silent (with Fake Lag and SMAC off) restores the
     command angles after aiming; the tab exclusion quirk matches.
   - Staged: staging/rebuilt/VR.dll (541.5K, sha256 3afb35b0...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.legitaim.
     TEMP diagnostics: one "rage aimbot run" line and up to 8
     "rage aim tgt=... cls=... dist=... ang=..." lines.
0AM. **EndScene Indicators window ported — CLOSED (verified live)**
   - Target Hook_EndScene 0x1000990E..0x10009B20 ported as DrawIndicators
     in src/hooks.cpp, called after DrawEndSceneOverlay and before
     Menu_Render (target order: status text, FOV circle, Indicators,
     radar, menu).
   - Window: SetNextWindowSize(ImVec2(200,50), Always) + Begin("Indicators",
     NULL, NoResize|NoCollapse). The target pokes the same ImGui 1.74
     context fields (context+6700 NextWindowData.Flags |= HasSize,
     +6708 SizeCond = Always, +6732/+6736 SizeVal).
   - Inside, while g_Indicators: the window is always created. When
     g_FakeLag && g_FakeLagValue > 0 && IVEngineClient slot 26 passes, it
     draws Text("Fake Lag (Choking)") and a bar: Dummy(400 -
     2*style.FramePadding.x, 6), AddRectFilled base covering 70% of that
     width in ImGuiCol_Button, then AddRectFilled overlay covering
     70% * the previous frame's pulse value in ImGuiCol_ButtonHovered;
     afterwards pulse = sin(g.Time * 13.0) * 0.5 + 0.5.
   - Colors are read from the live style exactly like the target
     (style.Colors[21]/[22] at GImGui+5904/+5920, alpha scaled by
     style.Alpha at +5392); GetColorU32 produces the same packed values.
   - Quirk: the bar is 392 px wide inside a fixed 200 px window, so it is
     clipped by the window clip rect, as in the original.
   - Expected live: Misc -> Indicators ON creates a movable fixed-size
     "Indicators" window; with Fake Lag enabled and a positive Fake Lag
     value in-game, the label and pulsing two-tone bar appear. Disabling
     Fake Lag or the value leaves an empty window; the label also requires
     the in-game engine gate.
   - Staged: staging/rebuilt/VR.dll (542.5K, sha256 42cfa9bc...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.rageaim.
0AN. **Movable radar window ported — CLOSED (verified live)**
   - Target Hook_EndScene 0x10009B20..0x10009C7C ported as DrawRadarWindow
     in src/hooks.cpp, called after DrawIndicators and before Menu_Render.
     With g_Radar (byte_1004AC30) and IVEngineClient slots 27 and 26
     passing, it creates a 292x306 ImGui window "Radar"
     (NoResize|NoCollapse) with WindowBg pushed to (0.2,0.2,0.2,0) from
     0x100474C0, copies the window Pos/Size into g_RadarWindowPos/Size
     (target dword_1004A4B4/A4B8/A4BC/A4C0), then End + PopStyleColor.
   - radar.cpp now centers the frame at pos + size*0.5 with the +10 y
     offset and the dots at pos + size*0.5 without it, exactly like
     sub_1003BFB0/sub_1003C1D0, instead of centering on the game window.
     The radar thus follows the movable ImGui window - this fixes the
     earlier live note that the radar display was not movable.
   - The target's dword_1004AC28/AC2C drag-delta block is orphaned (its
     values are overwritten every EndScene) and the dword_1004A9C4/A994
     integer copies are write-only, so neither is ported (documented as
     dead in INF-031).
   - Expected live: Visuals -> Radar ON shows a draggable "Radar" window
     (title bar, 292x306, dark transparent background); the frame and dots
     follow it and keep the same screen-relative layout. Moving the window
     must persist via imgui.ini like the original.
   - Staged: staging/rebuilt/VR.dll (543.0K, sha256 868438e5...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.indicators.
0AO. **Legit movement fix + dead-feature audit — CLOSED (verified live)**
   - Target Legit tail 0x1000757C..0x1000767A ported. The section saves
     cmd->viewangles[1] at 0x1000725B; after the aimbot/No-Recoil it
     re-expresses the movement vector in the modified view frame:
       magnitude = sqrt(forwardmove^2 + sidemove^2);
       moveYaw = atan2(sidemove, forwardmove) * 57.295776, normalized to
       [0,360); when forwardmove and sidemove are both zero the target's
       sub_10036890 special case yields moveYaw 0 (its 90/270 pitch is
       unused);
       moveAngle = (viewangles[1] - savedYaw + moveYaw) * 0.017453292;
       forwardmove = cos(moveAngle) * magnitude;
       sidemove = sin(moveAngle) * magnitude.
     It runs only when the Legit block is reached and not skipped by the
     canFire gate (target 0x1000767F), matching the existing skip flag.
   - Invisible Jockey (byte_1004B014) ported faithfully: entity scan with
     the target's inverted distance guard (writes the huge 0x7CF0BDC2
     forwardmove only when 0 > distance), so it is dead by construction.
     No config key exists in the target and none was added.
   - Auto Pistol (byte_1004AB7C, key "AutoPistol") and Server Lagger
     (byte_1004B03D, key "serverlag") have no logic reads anywhere in the
     target. Their menu checkboxes now bind the real globals and persist
     under those keys; they remain no-ops, like the original.
   - PaintTraverse PENDING items confirmed dead: dword_1004AFF4 (speed
     text float) and dword_1004B048 (sub_1003AD40 indicators gate) have no
     writers, so the whole block is unreachable in the original.
   - Expected live: with Legit enabled, WASD movement keeps its world
     direction while the aimbot rotates the view; without aiming the
     direction is unchanged (float round-trip). Invisible Jockey, Auto
     Pistol and Server Lagger do nothing in both builds, and Invisible
     Jockey must not survive save/load (no key).
   - Staged: staging/rebuilt/VR.dll (544.0K, sha256 e2cd94dc...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.radar.
0AP. **Rage Auto Shove ported — CLOSED (verified live)**
   - Target sub_10002D90 (Rage Auto Shove) ported in src/cheat.cpp and called
     from the Rage section before the clip check when g_RageAutoShove
     (ABC4) is on and the active weapon passes the sub_10004790 whitelist,
     matching target 0x10006489.
   - Preconditions: local DT_TerrorPlayer::m_isIncapacitated == 0 and
     cmd->buttons does not contain IN_JUMP (0x20); the call is skipped while
     the Rage Aim key (AB9C) is held.
   - Entity scan (first matching filters win; every qualifying entity is
     processed, so the last index wins): skip local/null,
     sub_10001EC0 failures, zero model, team 2 or 4, dormant, SetupBones
     failure, ghost infected, m_usSolidFlags == 20, and categories outside
     {3,4,5}. Eye-to-eye distance must be < 92.0 units.
   - Aim/write: pitch = atan((localEye.z-entityEye.z)/horizontal),
     yaw = atan((localEye.y-entityEye.y)/(localEye.x-entityEye.x)) + 180
     when dx >= 0, both normalized to [-180,180]; requires finite pitch,
     yaw and cmd->forwardmove; then cmd->buttons |= IN_ATTACK2 (0x800),
     viewangles[0] = pitch, [1] = yaw, [2] = 0.
   - Still PENDING: the target's SMAC-mode prediction branch
     (0x100064C9..0x100066B5), the g_RageEnabled anti-aim movement
     adjustment after the shove (0x100066C9..0x100067AA) and the
     sub_10004190 movement normalization used when anti-aim is off.
   - Expected live: with Rage Aimbot enabled, Auto Shove checked and a
     whitelisted weapon, stand within ~92 units of an infected without
     holding the aim key (and without jumping / being incapacitated) - the
     view snaps to it and a shove is triggered. Holding the aim key
     disables the auto shove. Compare with original.
   - Staged: staging/rebuilt/VR.dll (546.0K, sha256 ab9eccf5...); clean /GL
     rebuild, loadtest LOAD_OK; loaded.log rotated to loaded.log.movefix.
0AQ. **Final cleanup and deliverable build — CLOSED (verified live)**
   - Removed every recovery diagnostic from the deliverable: the vectored
     exception handler, stack walker, ExitProcess/RtlExitUserProcess/
     TerminateProcess MinHook detours, the bootlog module (src/bootlog.cpp
     and include/bootlog.h deleted; CMakeLists updated), the ESP
     g_Dbg/BboxDebug instrumentation and EspGetDebugContext, and the
     temporary ESP/radar/menu/aimbot/slot-20/chams log calls.
   - Confirmed no diagnostic symbols remain:
     `grep -rn "BootLog|bootlog|EspGetDebugContext|g_Dbg|g_BboxDebug|kDbg"`
     over src/include/CMakeLists.txt returns nothing.
   - Clean /GL rebuild: only the pre-existing overlay.cpp C4244 warning.
     Staged staging/rebuilt/VR.dll (532.0K, sha256
     abd9da18f78a7d4a876774cf4036ee9a9418b4f049000e3a3bed7020dadca916).
     Loadtest: LOAD_OK base=72420000, and deleting loaded.log before the
     run leaves no loaded.log after it — clean module.
   - Documents written: README.md (overview/build/layout/status/BLOCKED),
     NOTES/structural-metrics.md (size table), INF-034 (No-Spread BLOCKED
     analysis + cleanup record).
   - Remaining BLOCKED/PENDING, all documented in README.md and
     NOTES/inferences.md: Legit+Rage No-Spread (runtime client.dll pattern
     scan + MD5 seed + weapon+0xD0C), Rage SMAC-mode angle prediction
     (0x100064C9..0x100066B5), post-Auto-Shove anti-aim movement
     (0x100066C9..0x100067AA), sub_10004190 movement normalization, and the
     melee weapon-id-19 path (0x100067B7..0x10006A02). Dead-by-construction
     features match the original and are listed in README.md.
   - Expected live: injecting the staged DLL provides the same menu, ESP,
     radar, chams, aimbots, triggerbot, Auto Shove, movement fix,
     indicators, FOV circle, status overlay and config behavior as the
     previously verified builds, with no popup, no loaded.log, and no
     crash/exit detours.
   - User verified live ("expected live behaviour is verified!") and
     confirmed the menu's input capture is correct: while the ImGui
     interface is open it owns keyboard/mouse input and the game cannot be
     controlled in the background until the interface is closed. This
     matches original VR.dll behavior and is not a defect.
   - Final deliverable: staging/rebuilt/VR.dll, sha256
     abd9da18f78a7d4a876774cf4036ee9a9418b4f049000e3a3bed7020dadca916.
     Project closed; remaining gaps are the documented BLOCKED items.
0AR. **No-Spread implemented (Legit + Rage) — NEW BUILD STAGED**
   - Re-opened after the user confirmed the original's No-Spread works live.
     INF-034's BLOCKED decision was a verification-risk call, not a runtime
     failure; the block was then recovered to the byte and ported.
   - Exact target semantics (Rage 0x10006F81..0x1000712A, Legit
     0x100073BA..0x1000757C):
       * Gate order: No-Spread flag, sub_10004790 on the cached weapon
         entity (v162), cmd->buttons & IN_ATTACK; runs after No-Recoil,
         outside the aim-key gate, but after the canFire-failure return
         (our skipLegit/skipNoRecoil paths).
       * Fresh GetLocalActiveWeapon(local) -> weapon entity for the spread
         read at weapon+0xD0C (raw float, not a netprop).
       * seed = MD5_PseudoRandom(cmd->command_number) & 0x7FFFFFFF, written
         to the engine's random-seed global.
       * Call the resolved client.dll function twice with
         ("CTerrorGun::FireBullet HorizSpread", -spread, spread, 0) and
         ("...VertSpread", -spread, spread, 0); each float result is
         multiplied by -1.0f and added to viewangles[0] / viewangles[1].
   - Runtime resolution (now fully decoded):
       * sub_10037890 scans client.dll for "SharedRandomFloat\0", then
         walks backwards from literal+1 for the dword holding the literal VA.
       * The inline prologue walk (0x10007006) applies masks 0xF0/0x50,
         0xF0/0x80, 0x0F/0x0C, matching 55 8B EC.
       * sub_100378E0 behaves as "first 0xA1 within 0x100 bytes"; the
         32-entry table at 0x100414E9 is consumed only through its first
         byte due to the test eax,eax/jnz structure. The imm32 is the seed
         global address.
       * The reference immediate has a HIGHLOW relocation, so the scan
         works under ASLR.
   - Implementation: src/md5.cpp (MD5_PseudoRandom; verified against
     hashlib for 7 seeds), src/nospread_scan.cpp (exact scan, bounds
     tightened to the module, SEH-guarded), src/nospread.cpp (two
     function-local static resolvers mirroring the target's two static
     pairs; whitelist/IN_ATTACK gates reapplied).
   - Offline validation on the installed client.dll:
     tools/nospread_scan_check.bat maps the real file and runs the real
     scan code: function RVA 0x1ACDB0 (55 8B EC), seed RVA 0x6BF868,
     inside .data - PASS. tools/scan_client_nospread.py independently
     reproduces the same result and disassembles the resolved function
     (loads the seed global at entry, references the literal, cdecl,
     float return).
   - Safety deviations (only on failure paths): a failed scan skips the
     feature instead of calling/dereferencing null; the seed address must
     fall inside client.dll; the spread address is readability-checked.
   - Staged: staging/rebuilt/VR.dll (535.5K, sha256 37b156bf...); clean
     /GL rebuild, loadtest LOAD_OK, no loaded.log.
   - Expected live: with No-Spread enabled in either tab and a whitelisted
     weapon, hold fire - bullet impacts become centered (no cone) compared
     with the same shot with it off, matching the original. First use scans
     client.dll once (~9.6 MB), so a small one-time hitch is expected.
   - Still PENDING: Rage SMAC-mode angle prediction, post-Auto-Shove
     anti-aim movement and the melee weapon-id-19 path.
   - User verified No-Spread live ("expected live behaviour is verified!").
0AS. **Post-recovery documentation — CLOSED**
   - Ledger built from the build map, source definition lines, IDA naming
     log and recorded live tests: 174 rows (166 function rows + 8
     type/global), 126 function rows mapped to 82 distinct original
     addresses/blocks, 40 helper rows without a distinct original address.
     Results: PASS 170, BLOCKED 4, FAIL 0, UNTESTED 0; INFERRED 17.
   - Tests: T001-T022, 22/22 PASS; historical failures and causes in
     tests.md. CHANGELOG.md covers M0-M4 with 10 recorded size
     comparisons. README.md rewritten per the documentation brief.
   - Clean-build confirmation (2026-10-26): 0 errors, only the known
     overlay.cpp C4244 warning; LOAD_OK, no loaded.log; MD5 vectors and
     No-Spread scan check PASS. The clean rebuild hash f70a6147... differs
     from the live-verified staged binary 37b156bf... because the build is
     not reproducible (timestamps/debug metadata); the verified staged
     binary was not replaced.
   - Unavailable records recorded rather than invented: no git repository
     (all commit fields unknown), no REPORT.md, no tools/check.py, tests/
     empty.
0AB. **Thirdperson ported — NEW BUILD STAGED**
   - User verified world weapon-spawn ESP ("expected live behaviour is
     verified!").
   - Target block Cheat_Update 0x10005A36..0x10005BA8 (while byte_1004ABF2):
       * resolves DT_BasePlayer "deadflag" (nested in DT_PlayerState) twice
         and writes zero to localEntity + deadflag + 4 and +8. The source
         values dword_1004B138/B13C live in .data BSS, have no writers, and
         are zero.
       * if dword_1004ABF4 (ThirdpersonKey) != 0: on GetAsyncKeyState(key)&1
         toggle byte_1004B1EC and set z_view_distance to -100/0; nothing is
         written on other frames.
       * if no keybind: set z_view_distance = -100 every update.
     When Thirdperson is off, only z_view_distance = 0 is written (the
     deadflag dwords are not restored - one-way, as in the target).
   - Implementation in src/cheat.cpp; one-shot log `thirdperson deadflag=%d`
     reports the resolved offset. INFERRED safety guard: writes happen only
     for 0x100 < offset < 0x4000 (the target has no guard and even writes at
     entity+4/+8 if the table lookup fails).
   - Menu Visuals: "Thirdperson" checkbox (g_Thirdperson) + keybind button
     (g_ThirdpersonKey) in target order after Weapons ESP##weapons2; config
     key ThirdpersonKey loaded/saved (no config key exists for the checkbox
     itself in the target string table).
   - Expected live: enable Thirdperson without a keybind -> z_view_distance
     -100 should give the third-person camera view (target behaviour);
     with a keybind the view toggles on each key press. Disabling writes
     z_view_distance 0 again.
   - Staged: staging/rebuilt/VR.dll (517.5K, sha256 4614384f...;
     Cheat_Update RVA 0x89d0).
0AA. **Weapon-spawn ESP verified; speedometer dead by equivalence**
   - User verified world weapon-spawn labels ("expected live behaviour is
     verified!"); 0Z closed.
   - Speed text (PaintTraverse 0x1000518E..): reads dword_1004AFF4 as a
     float and draws the rounded speed with Tahoma24 when non-zero.
     `dword_1004AFF4` has exactly ONE reference in the whole binary (that
     read); there is no writer anywhere (byte-scan for the address too), and
     it is not set by sub_1003A300. The original therefore never draws the
     speedometer either, so this block is not ported (equivalence).
0Z. **World weapon-spawn ESP ported — NEW BUILD STAGED**
   - src/esp.cpp DrawWeaponSpawnEsp ports sub_1003DB30:
       * loop index 1..GetHighestEntityIndex(); skip null/local/dormant
         (entity+8 slot 7);
       * sub_1003B030 bbox with the local entity for the angle reference;
         ONLY draw when the visibility flag is true (all 8 corners project
         on-screen; the target's `if (v13)` gate);
       * center = ((minX+maxX)/2, (minY+maxY)/2);
       * client class (entity+8 slot 1) +20 must be 260 (CWeaponSpawn);
       * weapon ID from DT_WeaponSpawn::m_weaponID, mapped through the same
         sub_1003AB50 table; ID 0 and unknown IDs -> "Unknown Weapon" (the
         target guards the switch with `if (weaponID)`, so its case 0 is
         dead);
       * white centered Arial label at the bbox center.
   - Menu Visuals: "Weapons ESP##weapons2" checkbox added in target order
     after Radar/Show names; binds g_EspWeapons2 (byte_1004ABDB). Config key
     ESPWEAPONSPAWN already existed in load/save.
   - Expected live: enable Weapons ESP##weapons2 -> fully-on-screen ground
     weapon spawns get a white name label at their screen position (pistol,
     medkit, pills, throwables, upgrades, etc.). Partially off-screen spawns
     intentionally show nothing (target visibility gate).
   - Build staged: staging/rebuilt/VR.dll (517.0K, sha256 50116a13...;
     map: DrawEsp 0xc120, DrawWeaponSpawnEsp 0xd470, DrawRadar 0xdab0).
0Y. **Radar verified — closed; drag note**
   - User: "expected live behaviour is verified! note: radar display is not
     moveable, meaning it may obfuscate view, in original it may move around."
   - Analysis of sub_1003BFB0: the drag block updates dword_1004AC28/2C when
     the left button is held inside a rectangle derived from those same
     globals, but the frame coordinates (v8/v9) are always recomputed from
     the window center (dword_1004A4B4..C0). The updated position is never
     used for drawing, so as compiled the target radar is also immovable.
     The mouse state (byte_1004AA44 / dword_1004AA3C/AA40) is only refreshed
     by sub_10037170 inside PaintTraverse while the menu is open. We keep the
     static centered radar to match. If the original is confirmed to move,
     re-check this (possible CurrentWindow/rect difference).
0X. **Radar ported — NEW BUILD STAGED**
   - User verified ESP after the DrawPrintText ABI fix: Box/Name/Distance
     and the text paths are live-stable.
   - src/radar.cpp ports:
       * sub_1003BFB0 frame: dark 292x306 panel at
         (int(w*0.5+l), int(h*0.5+t)+10), 292x306 + 280x280 borders
         (255,255,255,alpha 0), white 1px crosshair, dots centered 10px
         higher (C1D0 has no +10).
       * sub_1003B820 entity loop: skip null/dormant (entity+8 slot 7)/
         non-renderable (entity+4 slot 8); keep team != 2; require
         category != 0 || sub_10001EC0 eligibility. GetViewAngles via
         engine slot 19 (QAngle); GetAbsOrigin via entity vtable slot 11.
       * sub_1003C1D0: dx/dy = entity origin - local origin; yaw ->
         radians via float->double (0.017453293005625408)->float; cos/sin
         via the target's float->double->float helpers; v19 = sin*dx -
         cos*dy; v20 = -cos*dx - sin*dy; clamp both to +/-3300 with the
         target's branch scaling; screen = center - (v/-3300)*110; clamp
         x to [cx-100+1 .. cx+100-3] and y to [cy-100+2 .. cy+100-4];
         6x6 FilledRect dot.
       * Colors/labels: "You" white; cat4 Default = orange (255,123,0),
         empty label (target passes the empty global named Default); Tank
         red; Witch red when m_rage == 1.0, else (16,252,8); cat3 classes
         270/265/99/272/263 = Smoker/Jockey/Charger/Spitter/Hunter, red.
       * Show names: target draws the label centered at x = saved EDI + 2,
         where the caller puts viewangles.z (roll) in EDI; reproduced as
         roll-bits+2 (normally x=2, left edge), y = dot y + 4.
   - Position source DEVIATION (INF-017): the target reads an ImGui window
     rect (dword_1004A4B4..C0, set in Hook_EndScene from
     ImGuiContext.CurrentWindow->Pos/Size); we use the game window client
     rect (GetClientRect + ClientToScreen) because our menu window is not
     the target's fullscreen ImGui window. Visual result = centered.
     Drag note: see 0Y above; the target's drag block updates
     dword_1004AC28/2C but the frame is re-centered from the window rect
     every call, so the target radar is effectively immovable too.
   - Menu Visuals: "Radar" and "Show names" checkboxes added in target
     order after Lines ESP; config keys radar/radarnames.
   - TEMP diagnostics: one-shot
     `radar first l=.. t=.. w=.. h=.. cx=.. cy=.. dcx=.. dcy=..` and
     once/sec `radar sec dots=.. names=..`.
   - Expected live: enable Radar -> centered dark radar with crosshair;
     orange dots for common infected, red dots for Tank/special infected;
     Witch green unless full rage; "You" white if the local player is
     infected. Show names adds labels (target quirk: x near the left edge).
   - Build staged: staging/rebuilt/VR.dll (517.0K, sha256 bbc7dc3d...;
     map: DrawRadar RVA 0xda90, DrawRadarDot 0xd820).
0V. **Text crash root cause: DrawPrintText arg-count ABI — FIXED, BUILD STAGED**
   - User: Box ESP worked; enabling Name crashed immediately; distance
     could not be tested.
   - Crash record: phase=filter idx=3 ent=null bbox sane (-59,595,249,1292);
     CRASH n=1 code=C0000005 addr=0000005F (execute fault), EBX=0x5F,
     ECX=0x800, EAX=6B70A574; stack walk:
       VR.dll Hook_PaintTraverse+0x17A (return after DrawEsp call) ->
       engine.dll+0x2AADBE -> vgui2+0x1EC02 -> VR.dll Hook_PaintTraverse+0x9D
       (return after calling the original PaintTraverse) -> ... (nested panel
       paints) -> engine.dll TerminateProcess.
   - ROOT CAUSE: VGUI_Surface031 slot 22 DrawPrintText has THREE stack args
     `(const wchar_t *text, int textLen, FontDrawType_t drawType)`. Target:
     0x1000C65E `push 0` / `push len` / `push text`; call [eax+0x58]; no
     caller stack adjustment (callee-clean). Our helper typedef had FOUR
     (`text, len, 0, textHeight`). After the callee's `ret 12`, ESP was 4
     bytes lower than the compiler expected inside OverlayTextV; its
     epilogue pops esi/ebx BEFORE `mov esp,ebp`, so the saved registers were
     restored from the wrong slots. EBX came back as 0x5F and the next
     indirect call jumped to 0x5F. This also explains the earlier
     distance-phase EIP=0 crash: any text draw corrupted the caller's
     callee-saved registers; Box-only (no text) was stable.
   - Fix: DrawPrintTextFn is now `(void*, const wchar_t*, int, int)` and the
     call is `(surface, wideText, wideCount-1, 0)`. Verified both emitted
     call sites push exactly 3 args (overlay.obj / inlined copies).
   - The 0U bbox/coordinate guards are retained as defense against
     INT_MIN/NaN text positions (the KERNEL32 IsBadReadPtr probe storm was
     real), but they were not the fatal fault.
   - Diagnostics added: `CRASH callret=.. mod=.. off=..` (top-of-stack return
     address at the faulting call site).
   - Expected live: Name/Distance/Health/Weapon text should now render and
     stay stable. Test Box+Name+Distance first, then all features.
   - Build staged: staging/rebuilt/VR.dll (512.0K, sha256 cf86c987...;
     map: Hook_PaintTraverse RVA 0x2740, DrawEsp 0xc200, OverlayTextV
     0x9aa0).
0U. **Crash root cause captured: INT_MIN text positions — GUARDED BUILD STAGED**
   - Fatal record (Box+Name+Distance only):
       CRASH n=1 code=C0000005 addr=00000000
       CRASH av write target=00000000
       CRASH regs eip=00000000 ... ebp=00F7D470 ebx=10000000
       esp ctx phase=7 (distance) idx=4 team=2 cat=5
               bbox=-379186880,-398929184,608125632,-2147483648
       EXIT TerminateProcess code=100 caller=engine.dll off=00201325
     Preceded by a storm of KERNEL32.DLL+0x145EA AVs (EAX=probe ptr,
     ESI=0x1000 page size -> IsBadReadPtr internals) with probe targets
     derived from the same extreme coordinates (0x80000000, 0x800000F4,
     0x95F88840/0x95F88934, ...). fonts were valid (font=235, fonts=1).
   - Interpretation: BuildBoundingBox can produce cvttss2si(inf/NaN) =
     INT_MIN for near-plane/behind projections (ScreenPosition still writes
     coordinates). The painter fed those positions to VGUI text; the engine's
     text/glyph path probed invalid addresses and eventually called a null
     function pointer (EIP=0), then engine.dll terminated the process. The
     target ignores its visibility byte (verified: ebp-0x2FD is written by
     sub_1003B030 but never read), so this guard is a documented INFERRED
     deviation (INF-015), not a target behaviour.
   - Fixes:
       * IsBboxSane(): every coord within +/-32767 and minX<=maxX, minY<=maxY;
         in DrawEsp a bad bbox skips the entity before any draw call.
       * One-shot diagnostics on the first rejected bbox: `esp badbbox ...`
         logs origin/mins/maxs/local eye/pitch/yaw/radius and all 8 projected
         outs plus the corner call order.
       * OverlayTextV: reject font<=0 and |x|/|y|/centered drawX > 32767
         before touching VGUI.
       * DrawLineToCenter: reject non-finite/extreme screen coords and
         implausible screen sizes.
       * `esp sec` now reports `bad=N` (entities skipped by the guard).
       * Non-system CRASH records now include a bounded EBP stack walk
         (`CRASH stack n=.. ret=.. mod=.. off=..`) so the caller chain is
         known even if the faulting module is the engine/OS.
   - Expected live: Box/Name/Distance should render only for finite
     on-screen projections; the floating/off-player labels from garbage
     boxes disappear; no crash. If a crash still occurs, the log will now
     contain either a new CRASH (with module/offset, stack walk and esp ctx)
     or the `esp badbbox` data explaining the projection.
   - Build staged: staging/rebuilt/VR.dll (512.0K, sha256 9f0b3bbe...;
     map: DrawEsp RVA 0xc160, BuildBoundingBox 0xac40).
0T. **Distance misplacement fixed (SSE bbox); crash still silent — NEW BUILD STAGED**
   - User: ESP works, but distance labels render off the player/team, and the
     game crashed after enabling multiple ESP features.
   - New session log: all ESP flags on (team/nm/bx/ds/hp/hb/wp/am/rg/ln=1),
     cand up to 31, phase=0 at every once/sec sample, and NO `CRASH`/`EXIT`
     record even though the VEH build was loaded. `esp offs`: mins=548
     maxs=560 view=244 wpn=4228 clip=2436 rage=4844 mob=4492 maxhp=8156.
   - ROOT CAUSE (distance/placement): the bbox reduction used `<`/`>`.
     sub_1003B030 compiles to SSE minss/maxss chains: a NaN projected corner
     is handled differently, while comparison-based min/max poisoned the
     whole bbox (NaN -> cvttss2si INT_MIN). That moved names/distances/boxes
     and fed INT_MIN coordinates into VGUI. BuildBoundingBox now:
       * calls ScreenPosition in the target's exact point order
         (top3, top0, top2, bottom2, bottom1, bottom0, top1, bottom3);
       * reduces with SseMin8/SseMax8 built from _mm_min_ss/_mm_max_ss in
         the target's operand order (new value as destination, running result
         as source, first step min(x,x));
       * verified the generated BuildBoundingBox contains 32 minss/maxss.
   - Crash capture broadened: VEH now logs every error-severity exception
     (except MSVC C++ 0xE06D7363 / CLR), no longer drops AVs in
     kernel32/kernelbase/ntdll (tagged `CRASH(SYS)` with a separate budget),
     and adds MinHook hooks for ExitProcess, RtlExitUserProcess and
     TerminateProcess that log `EXIT <api> code=.. caller=.. mod=.. off=..`
     plus the ESP context. This covers crashes that never raise an SEH
     exception (fastfail/exit paths).
   - Distance debug: DrawDistance records the last label's idx/x/y/meters;
     once/sec `esp sec ... dist idx=.. x=.. y=.. m=..`, and the crash context
     line includes `dist=idx,x,y,m`.
   - Expected live: distance numbers should now sit under the correct box.
     If the game still dies, send the last ~60 lines of loaded.log; the
     `CRASH`/`EXIT` line identifies module/offset/caller, and `esp ctx` the
     last ESP phase/entity. Also state which ESP checkboxes were on (the
     `esp sec` line lists them).
   - Build staged: staging/rebuilt/VR.dll (509.0K, sha256 d1a755ee...,
     build/Release/VR.map regenerated: DrawEsp RVA 0xbb80, BuildBoundingBox
     0xa800).
0S. **ESP visuals verified; crash under investigation — DIAGNOSTIC BUILD STAGED**
   - User: "esp features work as expected, but game crashed, not sure why".
     Log captured ESP active for a long session (cand 3..31, box/name/
     distance on) and then ends abruptly after a `slot20 calls/s` line; no
     fault information existed.
   - Root-cause fix: `PlayerInfoRaw` was 256 bytes, but L4D2's
     player_info_t is ~332 bytes (xuid + name[128] + userID + guid[33] +
     friendsID + friendsName[128] + flags + customFiles[4]). GetPlayerInfo
     therefore overflowed the caller's stack frame on every ESP entity, every
     frame. The buffer is now 512 bytes, zero-initialized per entity, with
     name[127] forced to NUL.
   - Added a TEMP vectored exception handler (DllMain attach) that logs to
     loaded.log for 0xC0000005/1D/94/FD/409/374: `CRASH code=.. addr=.. mod=..
     off=.. tid=..`, AV read/write + target address, EIP/ESP/EBP/EAX..EDI,
     then `EspGetDebugContext` breadcrumbs (phase, entity index/team/category,
     entity/weapon pointers, bbox). AVs faulting inside kernel32/kernelbase/
     ntdll are skipped so IsBadReadPtr page probes do not consume the
     16-event budget. The handler returns EXCEPTION_CONTINUE_SEARCH, so the
     game's own handling is unchanged.
   - ESP breadcrumb phases: filter, bbox, weapon-handle, weapon-entity,
     player-info, name, distance, health-bar, full-bar, box, lines,
     weapon-text, rage-bar. Once/sec `esp sec` now logs every flag plus
     phase/lastIdx; one-shot `esp offs` logs the resolved netprop offsets
     (mins/maxs/view/wpn/clip/rage/mob/maxhp).
   - Text helper hardening: OverlayTextV now converts into a fixed
     wchar_t[260] stack buffer instead of std::wstring, removing per-label
     heap allocation on the render thread (target-style helper, less churn
     during hordes).
   - Expected live: ESP still behaves as before. If it crashes, send the last
     ~40 lines of loaded.log; the CRASH + `esp ctx` lines identify the faulting
     module/offset and the exact ESP operation/entity.
   - Linker now emits build/Release/VR.map (/MAP, no codegen change) so a
     `mod=...VR.dll off=...` value from a CRASH line can be resolved: the
     logged offset is the RVA, i.e. map VA - 0x10000000 (e.g. DrawEsp is
     1000b470 -> off 0xb470, BuildBoundingBox 0xa340).
   - Build staged: staging/rebuilt/VR.dll (508.5K, sha256 29f8b649...;
     loaded.log diagnostics enabled).
0R. **ESP (player/world) ported — NEW BUILD STAGED**
   - Implementation: `src/esp.cpp` ports sub_1003C410 (entity loop + draw
     branches) and its helpers; `src/overlay.cpp` now exports the VGUI
     primitives (color/filled/outlined/line/text) for reuse.
   - Filter chain per entity index 1..GetHighestEntityIndex():
     entity!=local; IsDormant (entity+8 vtable slot 7); renderable probe
     (entity+4 vtable slot 8, skip if 0); m_usSolidFlags==0x14 skip; team
     filter (Show Team off hides local team); category!=0; sub_10001EC0
     eligibility. Then bbox, active-weapon handle (DT_BaseCombatCharacter
     m_hActiveWeapon -> IClientEntityList slot 4), GetPlayerInfo(index).
     Drawing only proceeds when Enable ESP (byte_1004ABCE) is set.
   - sub_1003B030 reproduced: radius =
     (int)((float)(int)sqrt((mins.y-maxs.y)^2)/1.3); pitch =
     atan((origin.z-eyeLocal.z)/horiz)*57.295776; yaw =
     atan(dy/dx)*57.295776 + 180 if dx>=0; both normalized to [-180,180];
     4 corners at (yaw+45+90k) scaled by cos(pitch) and radius; top z =
     origin.z+maxs.z, bottom z = origin.z+mins.z; min/max of the 8
     IVDebugOverlay::ScreenPosition (slot 12) results -> int bbox.
   - Branches (colors from the target's call-site pushes):
     * cat5 team3: name (amber, y0-15), eye distance "[m]" (white), health
       bar, box (132,66,245), lines (red to screen bottom-center).
     * cat5 team2: same plus weapons/ammo text; box (93,245,66).
     * cat4: "zombie"/"zombie - RUSH", distance, box (255,123,0),
       full bar (sub_1003DA50).
     * cat3: player name, optional [BOT] tag (EspDanger), distance, health
       bar, box (132,66,245), lines, weapon display name (sub_1003AB50).
     * cat1: "Tank"; cat2: "Witch - Rage: %.2f" + DANGER, distance at
       y1+5, full bar, box, rage bar (1px inset, 3px tall), lines.
   - Health bar (sub_1003D870): filled = health*|y1-y0|/maxHealth, bar at
     x0-6, black backdrop over the filled span, green 3px core; numeric
     health at x0-16 when Player Health ESP is on.
   - Menu Visuals: exact target labels/order added (Enable ESP, Show Team,
     Player Name/Box/Distance/Health/HealthBar ESP, Weapons ESP, Ammo ESP,
     Witch Rage Bar ESP, Lines ESP). Config keys from 0x457A0..0x45818:
     EspToggle/EspShowTeam/EspDanger/EspName/EspBox/EspHealth/ESPHEALTH2/
     ESPWEAPON/ESPWEAPONSPAWN/ESPWEAPONSAMMO/ESPWITCHRAGEBAR. The target has
     no key for Player Distance ESP or Lines ESP, so those are not persisted.
   - Target quirk preserved: PaintTraverse only calls the painter when Box OR
     Name OR Distance ESP is enabled; Enable ESP alone does nothing.
   - DEVIATIONS (INFERRED, see NOTES/inferences.md INF-012): player_info name
     forced NUL at byte 127; cat5-team2/cat3 name drawn with "%s" instead of
     the target's name-as-format-string bug; health division guarded when
     maxHealth<=0.
   - Temporary diagnostics in loaded.log: one-shot
     `esp first idx=.. team=.. cat=.. bbox=.. vis=.. surf=.. font=.. dov=..`,
     once/sec `esp sec cand=.. enable=.. box=.. name=.. dist=..`; existing
     `pt hook call`/`pt focus found`.
   - Expected live: turn ON Enable ESP + Player Box ESP (the painter is only
     invoked when Box OR Name OR Distance is on). Other-team players and
     special infected should get outlined boxes (purple survivors/others,
     green infected players, orange zombies), amber names above, "[m]"
     distance below, green health bars left of the box; Show Team also shows
     teammates. Health/HealthBar/Weapons/Ammo/Witch Rage/Lines add their
     elements. `esp sec` should report cand>0.
   - Build staged: staging/rebuilt/VR.dll (507.0K).
   - PENDING: radar (sub_1003BFB0 + sub_1003B820, incl. sub_1003C1D0), world
     weapon-spawn ESP (sub_1003DB30, ESPWEAPONSPAWN), speed text
     (dword_1004AFF4/sub_1003F3C0), Thirdperson (byte_1004ABF2). Their menu
     widgets (Radar, Show names, Weapons ESP##weapons2, Thirdperson) are
     intentionally not shown yet; the old dummy kVisA placeholder group was
     removed so no fake ESP controls remain.
0Q. **Indicators verified by equivalence — closed**
   - User: "expected live behaviour verified! note : no visual changes ,
     same as the original VR.dll nothing happens, noted."
   - The target's only call site is PaintTraverse -> sub_1003AD40, gated by
     dword_1004B048, and no writer for that gate exists in the binary. The
     shipped behaviour is therefore "never drawn"; our direct
     checkbox->panel mapping was the only deviation. DrawIndicatorsPanel is
     no longer called (code kept as reference) and its per-second value
     diagnostics were removed.
   - VGUI font/text/border/bar foundation from 0N/0P is retained and now
     backs the ESP painter.
0P. **Indicators alive-gate fixed — NEW BUILD STAGED**
   - Diagnostic log: fonts ok (Arial=469, Visitor=470, Tahoma=473), PaintTraverse
     hook fires, FocusOverlayPanel found (id=85805560), local player valid,
     m_iHealth=100, but m_lifeState=-1447070976 -> alive gate always failed.
   - Cause: target sub_100045A0 reads m_lifeState as a single BYTE
     (`!v1[offset]`); our getter read a 4-byte int, so adjacent netprops made
     it nonzero. Fixed to an unsigned byte read.
   - Expected live: enabling Indicators now passes the gate and draws the
     "Monitor" panel; `pt indicators draw` should appear in the log.
0O. **Indicators — no visual change; diagnostic build staged**
   - User report: enabling Indicators causes no crash and no visual change.
   - Added one-shot logs: `pt hook call` (PaintTraverse hook reached),
     `pt focus found id=N` (FocusOverlayPanel detected through IPanel slot
     36), `fonts call surface=...` / `fonts ok arial=...` (font creation),
     `pt gate ind=1 local=... life=... hp=...` (first 6 panel draws after the
     checkbox is on), and `pt indicators draw` (draw call entered).
   - CreateOverlayFonts is also retried lazily from PaintTraverse.
   - Next: user enables Indicators and sends these lines; they distinguish
     hook-not-called, focus-not-found, fonts-not-created, alive-gate-failing,
     and draw-called-but-invisible.
   - Suspects if hook not reached: VmtHook Panel slot 41 install; if focus
     not found: GetName slot/name; if gate fails: m_lifeState/m_iHealth
     offsets or local entity; if draw called: VGUI slot signatures/positions.
0N. **Indicators overlay + VGUI foundation — NEW LIVE BUILD STAGED**
   - Chams (hand/player/weapon) verified after the entity-vtable fix (0M).
   - EndScene init now creates the target's fonts in exact order: Arial
     14/700 (0x1004A9C8), Visitor TT2 BRK 13/500 (0x1004A9D0), Tahoma 13
     (handle overwritten), Tahoma 24 x2 (0x1004A9D4 overwritten, 0x1004A9CC)
     via VGUI_Surface031 slots 63/64.
   - src/overlay.cpp ports: sub_1000C390 formatted text (CP_UTF8 ->
     UTF-16, GetTextSize/DrawSetTextColor/DrawSetTextFont/DrawSetTextPos/
     DrawPrintText slots 72/19/17/20/22), sub_1000C2A0 1px border,
     sub_1000C1A0 30x160 bar + Visitor label, sub_1003AD40 "Monitor" panel
     (outer 112x226, inner 100x200, title Arial at top, Recoil/Speed bars
     at inner+9/+59, exact target formulas/constants incl. interval_per_tick).
   - PaintTraverse now forwards to the original, caches the
     "FocusOverlayPanel" id through IPanel slot 36, and draws the panel when
     g_Indicators and the local player is alive (sub_100045A0:
     m_lifeState==0 && m_iHealth>0). New netprop getters: m_vecPunchAngle,
     m_lifeState, m_iHealth.
   - Menu "Indicators" checkbox now binds g_Indicators; config key
     "indicators" added. DEVIATION: the target's checkbox byte_1004B06C
     opens a settings popup and the panel gate is dword_1004B048 (only read
     in PaintTraverse, no direct writer found); this build maps the checkbox
     directly to panel drawing. Panel dragging is PENDING (target uses mouse
     capture globals).
   - Expected live: enable Indicators -> "Monitor" panel at (400,400) with
     Recoil and Speed bars; shooting raises Recoil, moving raises Speed.
     Compare with the original DLL if its Indicators panel is obtainable
     (its enable path is a popup).
0M. **Chams crash root cause found and fixed — NEW BUILD STAGED**
   - User oracle: original VR.dll with Hand+Player chams does NOT crash and
     chams special infected.
   - Crash log: `chams player idx=-1 ...` then `idx=229 high=2048
     model=...` and then nothing (no eval line) -> crash inside
     GetEntityCategory/GetEntityTeam for entity 229.
   - Target assembly (sub_10001DB0 and sub_10001EC0) proves the layout:
     the dword at entity+8 IS the vtable pointer, and the virtual is called
     with this = entity+8 (`mov eax,[ecx+8]; add ecx,8; mov eax,[eax+4];
     call eax`). Our helpers double-dereferenced the vtable pointer
     (`vtable=*(void***)P`) and passed this=P instead of entity+8 -> crash
     for entities whose wrong-this call touched live code/data.
   - Fixed GetEntityCategory (vtable[1], this=entity+8) and
     IsPlayerChamsEligible (vtable[7], this=entity+8). This also corrects the
     shared Name Stealer category filter and player eligibility to the
     target's exact calls.
   - Diagnostic chams logs remain for this test.
   - Expected live: Hand+Player chams on, no crash, special infected get
     chams as in the original.
0L. **Player/Weapon chams crash — likely fixed; guarded build staged**
   - Diagnostic log from the crash run ends at `chams hit hand` with **no**
     `chams player eval` line: the crash happens in the player block before
     its first log, i.e., in GetClientEntity(entityIndex) or the category
     calls.
   - Root cause candidate: unlike the target, our player block called
     entity-list GetClientEntity without the target's nonzero/model gate and
     without an upper bound. A garbage DrawModelState+48 value (pointer-like
     or out-of-range) can crash inside the engine's entity lookup.
   - Fix: player chams now requires g_pVomitMaterial (target gate), a
     non-null model pointer, `entityIndex > 0`, and
     `entityIndex <= GetHighestEntityIndex()` before calling GetClientEntity.
     Valid draws are unchanged.
   - Added logs: `chams player enter`, `chams player idx=%d high=%d model=%p`
     (first 8), then the existing eval/done lines. If it still crashes, the
     last new line pinpoints whether the crash is before GetClientEntity
     (enter/idx), inside the lookup (idx then nothing), or later (eval/done).
   - Also learned: slot-20 is called ~250x/s while updates run ~60x/s;
     Cheat_Update dedup remains once per command number (target may run per
     matching call; revisit if a feature needs it).
0K. **Weapon Chams — LIVE CRASH; diagnostic build staged**
   - User report: enable Hand Chams, close menu, reopen and enable Player
     Chams -> game crashes (previous Player Chams build verified without
     weapon-cham code).
   - Suspects: weapon-cham branches now active (ini may carry
     wpnchams/Handwpnchams/ChamsINFECTED from earlier), multiple XQZ orig
     passes, or the player eligibility path combined with weapon passes.
   - Diagnostic build instruments DrawModelExecute (first-hit only, no
     flood): logs `chams cfg ...` once with all six flags + player RGB;
     `chams hit hand/vmodel/wmodel/infected` on first match; `chams player
     eval/done` for the first 8 player-candidate draws (idx/cat/team/elig/
     match); `chams xqz begin/end` for the first 4 XQZ passes.
   - Reproduce the same sequence; the last line before the crash identifies
     the failing path (eval without done = crash inside IsPlayerChamsEligible;
     begin without end = crash in the extra XQZ draw; done + hit + crash =
     crash in material configuration/draw).
0J. **Weapon Chams added (config-only) — NEW LIVE BUILD STAGED**
   - Player Chams verified live (0I).
   - DrawModelExecute 0x10004C14/0x10004CAC/0x10004D44:
     - Handwpnchams (byte_1004ABED, key "Handwpnchams") + model path
       "models/v_models";
     - wpnchams (byte_1004ABEC, key "wpnchams") + "models/w_models/weapons";
     - ChamsINFECTED (byte_1004ABEF, key "ChamsINFECTED") +
       "models/infected".
     All use material 0x1004B2BC (debugambientcube) and the hand-cham colors
     0x1004AFE4/E8/EC, with the same XQZ/normal pass ordering.
   - ChamsSURVIVOR (byte_1004ABEE, key "ChamsSURVIVOR") is loaded/saved but
     is not referenced by the DrawModelExecute detour (PENDING/used
     elsewhere).
   - Target has no menu checkboxes for these; they stay config-only. Keys
     added to Load/Save.
   - Expected live: set wpnchams/Handwpnchams/ChamsINFECTED=1 in the current
     profile ini (or save then edit), click load (or re-inject). Held weapon,
     world weapon models, and infected models should be tinted with the hand
     cham colors; XQZ adds the through-wall pass. Compare with original
     VR.dll using the same keys/colors.
0I. **Player Chams added — NEW LIVE BUILD STAGED**
   - Hand Chams verified live (0H).
   - DrawModelExecute 0x10004DD3: Player Chams (byte_1004ABE0, config
     "EspChams"). An entity matches if any of:
     1. GetEntityCategory == 4;
     2. model name contains "models/infected";
     3. team == 2 && category != 0 && IsPlayerChamsEligible(entity).
   - IsPlayerChamsEligible replicates sub_10001EC0: object at entity+8
     vtable slot 7 state check, team must be 2/3, then per category uses
     DT_BaseAnimating::m_nSequence and DT_BaseEntity::m_usSolidFlags
     (cat 1/2: !(solid&4) && seq <= 0x46; cat 3: !(solid&4);
     cat 4: !(solid&4) && seq <= 0x131 && solid <= 0x14; default true).
   - Material 0x1004B2E8 (debug/debugambientcube), XQZ through-wall pass
     then normal pass, colors dword_1004AFD8/DC/E0, config keys
     chamscolor/1/2. Color defaults are 0 (INFERRED, same as Hand Chams).
   - UI: Player Chams checkbox now bound; inline "Player Chams Colors"
     R##ColorP1/G##ColorP2/B##ColorP3 sliders added (original uses a popup).
   - Expected live: enable Player Chams (optionally XQZ), set colors ->
     survivors/bots (and infected models) drawn tinted, through walls with
     XQZ. Compare with original VR.dll using the same colors.
0H. **Hand Chams added — NEW LIVE BUILD STAGED**
   - Remove Vomit verified live (0G).
   - DrawModelExecute 0x10004B7C: Hand Chams (byte_1004ABE2). The
     DrawModelState model pointer at +28 is resolved through
     g_pModelInfoClient vtable slot 3; if the name contains
     "models/weapons/arms/v_arms":
     - XQZ (byte_1004ABE1) -> configure material with ignore-z (flag 0x8000)
       and draw the original once (through-wall pass);
     - then configure the normal cham material; the unconditional final draw
       supplies the normal pass.
   - Material setup mirrors sub_10004800/sub_100048C0: material flags via
     IMaterial slot 29 (0x8000=ignoreZ, 1024, 0x2000, 0x4000, 0x8000000 all
     1), ColorModulate via slot 28, ForcedMaterialOverride via model-render
     slot 1 (reset to NULL after the final draw).
   - Material: debug/debugambientcube (target default mode dword_1004ABE8=0).
   - Colors dword_1004AFE4/E8/EC, config keys handchamscolor/1/2; target
     initializer does not set them, so they default to 0 (INFERRED). The
     original exposes them via a color popup opened from the chams buttons;
     this build adds inline sliders "Hand Chams Colors" / R##ColorH1 /
     G##ColorH2 / B##ColorH3 (UI deviation, values/labels match).
   - Config keys ChamsHands/CHAMSXQZ added. Player chams and weapon chams
     remain PENDING (their logic needs entity category/team/animation
     helpers); the Player Chams checkbox is still inert.
   - Expected live: enable Hand Chams, set a visible color (defaults black);
     the first-person arms should be tinted. XQZ adds the through-wall pass.
     Compare with original VR.dll using the same colors.
0G. **Remove Vomit added + true defaults corrected — NEW LIVE BUILD STAGED**
   - Anti Smoker verified live (0F).
   - DrawModelExecute ABI settled: target detour epilogue is `retn 0Ch`
     (this + ctx/state/info; the old 4th "boneToWorld" name was a misnomer,
     and the pass-through hook was already correct).
   - Remove Vomit (byte_1004B008 low, menu checkbox + config key "NoVomit"):
     0x10004980 lazily creates "particle/screenspaceboomervomit" and
     "debug/debugambientcube" through g_pMaterialSystem vtable slot 71
     (name, texture group, 1, 0); when DrawModelState +28 (renderable) and
     +48 (entity index) are non-zero it calls the vomit material's vtable
     slot 29 (flag 4) with the Remove Vomit byte, draws the original once,
     then calls ForcedMaterialOverride(NULL,0) via model-render slot 1.
   - Cham branches remain PENDING; with all cham flags off the target also
     draws once at the end, matching this port.
   - TRUE STARTUP DEFAULTS found in sub_1003A300 (called from the CRT init
     table via sub_10001100): AspectRatioValue=1, LagFast=0, LagKey=0,
     SmokerMode=0 (previous -1 values were IDA's uninitialized .data).
     Corrected.
   - Expected live: enable Remove Vomit, get vomited by a Boomer -> the
     screen vomit overlay disappears; compare with original VR.dll.
0F. **Anti Smoker added — NEW LIVE BUILD STAGED**
   - Lag Exploit verified live (0E).
   - Cheat_Update 0x10006385: gate byte_1004B00E ("Anti Smoker", config
     "antismoker"); mode dword_1004B010 via combo "Smoker Mode"
     {Always(0), Reach Smoker(1)} — target static is -1, so the feature is
     inactive until a mode is chosen (quirk preserved; no config key).
   - If local player's DT_TerrorPlayer::m_isHangingFromTongue != 0:
     mode 0 -> cmd->tick_count (offset 8) = 0x1000000;
     mode 1 -> cmd->forwardmove (offset 24) = 0x7FC00000 (quiet NaN).
   - Added netprop getter GetEntityHangingFromTongue (table-scoped
     DT_TerrorPlayer lookup); Auto Smoker block placed after No Fall Damage
     so the target's early return still skips it.
   - Expected live: enable Anti Smoker, pick Always or Reach Smoker, get
     grabbed by a Smoker -> behavior matches original VR.dll (Always rewrites
     the command tick to 0x1000000; Reach Smoker sets NaN forwardmove).
0E. **Lag Exploit added + keybind widget — NEW LIVE BUILD STAGED**
   - Name Stealer verified live (0D).
   - Cheat_Update 0x1000613E: if byte_1004B03C && dword_1004B038 > 0 &&
     GetAsyncKeyState(dword_1004B034) -> engine slot-74 field +8 += slider
     value (AddEngineSlot74Field8, shared with IWS/Instant Revive).
   - Config keys/statics: LAGEXPLOIT, LagFast (dword_1004B038, slider
     0..1000, target static -1), LagKey (dword_1004B034, VK, static -1).
   - UI recovered from 0x1000B34F/0x1000B537: checkbox "Lag Exploit",
     keybind button sub_10007DD0 (70x18, shows VK name, click toggles
     capture, scans VK 1..123, ESC -> 0, capture-state byte 0x1004B212),
     slider "Lag Exploit##LagExValue" 0..1000. VK-name table extracted
     byte-for-byte from target off_100421D0 (124 entries, same strings).
   - Safety/UI deviations: key index clamped to 0..123 (target indexes the
     table with -1 by default); no ImGui style tweaks from the target's
     button wrapper (visual only).
   - Expected live: enable Lag Exploit, click the keybind button and press a
     key, raise the slider above 0, then hold the key in game -> lag /
     rubberbanding proportional to the slider; release to recover. Compare
     with original VR.dll.
0D. **Name Stealer added — NEW LIVE BUILD STAGED**
   - Chat Spam verified live (0C).
   - Cheat_Update 0x1000564F: while byte_1004B015 is set, a 1 ms timer
     calls sub_100053D0, which:
     1. loops entity indices 1..entitylist vtable+0x20 (slot 8);
     2. keeps entities (!= local) whose category == 5 (0x10001DB0 class-ID
        mapping: 264->4, 277->2, 276->1, {0,99,270,263,265,272}->3,
        {232,275}->5) and whose m_iTeamNum equals the local player's;
     3. picks a random collected index, calls engine GetPlayerInfo (vtable
        slot 8 / 0x20), then formats
        `setinfo name "%s%c"` with name and 0x0D and executes it through
        ExecuteClientCmd (slot 107 / 0x1AC).
   - New netprop helpers: GetClientEntityByIndex (list slot 3),
     GetHighestEntityIndex (list slot 8, sanity-capped 1..4096),
     GetEntityCategory (sub_10001DB0).
   - SAFETY DEVIATION: target formats an uninitialized player-info buffer if
     GetPlayerInfo fails; ours is zeroed and skipped when the name is empty.
   - Name Stealer checkbox already bound to the real flag by the Chat Spam
     work; no config key in the target.
   - Expected live: enable Name Stealer in a game with same-team bots/players
     -> your scoreboard name changes to a random teammate's name and keeps
     updating; compare with original VR.dll.
0C. **Chat Spam added — NEW LIVE BUILD STAGED**
   - Instant Revive verified live (0B).
   - Cheat_Update 0x100056B1: gate byte_1004AC14, mode dword_1004AC18
     (combo "Spam Mode###SpamChatzz" = {Advertise, Chat Jammer}; config keys
     "Chatspam"/"ChatspamMode").
   - Mode 0: seed srand(time), pick one of ten identical tag strings, build
     `"say : " + 21 CRs + " [www.unknowncheats.me] @space785"` (the target's
     sub_1000BD80 is an inlined std::string::insert(0,"say : ")) and call
     engine->ExecuteClientCmd (vtable slot 107 / 0x1AC).
   - Mode 1: same call with the literal `"say : " + 21 CRs + " "`.
   - Throttle quirk: with Name Stealer OFF the target sends on **every**
     update; with Name Stealer ON each mode is throttled to 3000 ms by its
     own static timer. Reproduced with two per-mode QPC timers.
   - "Name Stealer" checkbox now binds the real flag byte_1004B015 (needed
     for the throttle); its own name-changing behavior is still PENDING.
   - Exact literals byte-compared against original/target.dll (21 CRs,
     same tag). Config keys added to Load/Save.
   - Expected live: enable Chat Spam in Misc; Advertise floods chat with the
     UC tag, Chat Jammer floods with the CR-only message. With Name Stealer
     also enabled, messages appear once per ~3 s instead of every update.
0B. **Instant Revive added — NEW LIVE BUILD STAGED**
   - Instant Weapon Switch verified live (0A).
   - Cheat_Update 0x100061BD: gate byte_1004B00C (menu "Instant Revive";
     no config key in the target). While `cmd->buttons & 0x20` (IN_USE) is
     held, call engine vtable slot 74 and add 500 to result+8 every 200 ms
     (per-call-site QPC timer; first press fires immediately).
   - Uses the AddEngineSlot74Field8 helper validated by IWS.
   - Misc checkbox "Instant Revive" bound to the real flag.
   - Expected live: enable it, hold E (USE) at a downed teammate -> revive
     completes immediately/accelerates versus normal; compare with the
     original VR.dll.
0A. **Instant Weapon Switch added — NEW LIVE BUILD STAGED**
   - Bunny Hop auto-jump verified live (0z).
   - Cheat_Update 0x10006184 (gate byte_1004B00B, menu "Instant Weapon
     Switch"; the target has **no** config key for it): when
     `dword_1004A97C != cmd->weaponselect (+44)`, call
     `g_pEngineClient->vtable[74] (0x128)` and add 100 to the dword at
     result+8; then cache weaponselect. The cache updates only while the
     option is enabled (target behavior; initial static value -1).
   - Added shared helper AddEngineSlot74Field8 (also used by the unported
     Lag Exploit +8 += slider and Instant Revive +8 += 500/200 ms blocks).
     Guards only reject null/invalid interface pointers.
   - Misc checkbox "Instant Weapon Switch" bound to the real flag.
   - Expected live: enable it and switch weapons -> switching completes
     immediately versus the normal delay (compare with original VR.dll).
0z. **Bunny Hop corrected + Auto-jump added — NEW LIVE BUILD STAGED**
   - Mapping correction: menu "Bunny Hop" binds byte_1004ABF0 (config
     "AUTOBH") and drives the auto-jump state machine at 0x100060E8. The
     strafe block at 0x10005E20 is gated by byte_1004ABF1 (config
     "AutoStrafer", no menu checkbox) — the previous build incorrectly used
     the Bunny Hop checkbox for strafing.
   - Ported 0x100060E8: if AUTOBH && !SMAC, run the two-byte state machine
     (0x1004B218 request / 0x1004B1E0 state) that clears IN_JUMP while
     airborne and re-presses it on landing; the target skips the request
     clear when the jump branch was taken (reproduced).
   - AutoStrafer now gated by g_AutoStrafer; byte_1004A121 emulated
     (initial value 1, latched to 0 on the first WASD press for the rest of
     the process — target quirk). Strafe logic itself unchanged.
   - Config keys added: AUTOBH, AutoStrafer.
   - Temporary diagnostics removed (nofall lines, NaN counter, netprop dump);
     a `slot20 calls/s=/updates/s=` line remains to confirm call rate.
   - Expected live: check Bunny Hop, hold space while running -> continuous
     auto-jumps (compare with original VR.dll); SMAC Mode disables it.
     AutoStrafer only runs with AutoStrafer=1 in the ini and stops after the
     first WASD press.
0y. **No Fall Damage closed — original is non-functional too**
   - User tested original/target.dll: No Fall Damage does not work there
     either. Our port reproduces the target's exact condition/NaN write, so
     this is verified-by-equivalence and kept as a documented non-functional
     original feature rather than "fixed".
0x. **No Fall Damage — diagnostics round 2 staged**
   - First diagnostic log proved the netprop side is correct:
     `np tables=287 dtplayer=... dtbaseent=...`;
     `flagsName=240 flagsTbl=240 fallTbl=4604 fallName=4604`;
     `teamTbl=228 teamName=228`.
     Live lines show `flags=128/129` (0x80 airborne / 0x81 grounded),
     `team=2`, and `fall` spikes to 784/541/449/373 while airborne -> the
     target's `>360` condition is reached, so the NaN write should fire.
   - Remaining difference found: the target's detour guard compares
     `cmd->command_number` with the caller frame's first argument (likely
     the input sequence number), i.e. it runs `Cheat_Update` on **every**
     matching slot-20 call; our shim deduped to once per new command. The
     dedup was removed in this build (`slot20 calls/s=` now logged).
   - Added `nan=` counter to the once-per-second `nofall` line to confirm
     the 0x7FC00000 write actually happens.
   - Test instructions: enable No Fall Damage, walk off a high ledge without
     pressing jump; report `slot20 calls/s=` and `nofall ... nan=` lines.
0w. **No Fall Damage — LIVE FAILED; diagnostics staged**
   - User report: enabling No Fall Damage and falling from any height has no
     effect.
   - Re-verified target asm: `[edi+18h]=7FC00000h` (cmd->forwardmove), buttons
     at `[edi+24h]`, `m_flFallVelocity` read from local entity, resolver
     sub_100374A0 matches our NetpropOffset byte-for-byte. `this`=cmd and
     ESI=local entity confirmed in the target prologue (`mov edi,ecx`,
     `mov esi,eax` after GetClientEntity).
   - Most likely remaining cause: offset resolution returns a bogus offset
     (target resolver accumulates nested results even when the name is not
     found), so the read values are wrong.
   - Diagnostic build staged: EndScene init logs `np tables/dtplayer/...`
     and `np off flagsName/flagsTbl/fallTbl/fallName/teamTbl/teamName`;
     Cheat_Update logs `nofall flags/team/fall/btn` once per second while
     the option is enabled.
   - Next: user injects, enables No Fall Damage, walks off a ledge, reports
     `loaded.log`. Expected sane values: team=2 (survivor), flags odd on
     ground/even airborne, fall magnitude in the hundreds+ when falling.
0v. **No Fall Damage added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x10006232 (gate byte_1004B00D, menu/config "NoFallDamage"):
     while airborne (`m_fFlags & 1 == 0`), if `m_iTeamNum == 3` or
     `cmd->buttons & 2` (IN_JUMP) -> **return** from Cheat_Update (quirk:
     skips all later feature blocks); else if `m_flFallVelocity > 360.0f`
     and the 300 ms timer fires, write `0x7FC00000` (quiet NaN) to
     `cmd->forwardmove` (CUserCmd+24).
   - Helper semantics resolved: `sub_100015A0` = DT_BasePlayer::m_fFlags,
     `sub_100016A0` = DT_BaseEntity::m_iTeamNum, `sub_100022B0` = QPC
     cooldown (first call always fires because the static starts at zero).
   - netprop.cpp gained table-scoped lookup `NetpropOffsetInTable` (+12 table
     name), `GetEntityTeam`, `GetEntityFallVelocity`.
   - Bunny Hop block restructured from early-return to a conditional so
     No Fall Damage still runs when Bunny Hop is off (no behavior change for
     Bunny Hop itself).
   - Expected live: enable No Fall Damage, fall from height as survivor ->
     no fall damage (movement may briefly NaN-glitch, matching target); as
     infected or while holding jump the block bails; save+load persists
     NoFallDamage.
0u. **Aspect Ratio added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x10005C6B: `if (byte_1004AB80 && !SMAC)`
     SetConVarInt("r_aspectratio", dword_1004AB84). Disabling does **not**
     restore the convar (target quirk preserved).
   - Visuals tab recovered order (0x1000AD86-0x1000AE33): checkboxes
     FOV Changer (0x1004AB88), Aspect Ratio (0x1004AB80), No Visual Recoil
     (0x1004AB97); then SliderInt "Player FOV" (0x1004AB8C, 90-120),
     "Viewmodel FOV" (0x1004AB90, 51-130), "Aspect Ratio##ATRO"
     (0x1004AB84, 0-3). Slider helper 0x100312F0 is inlined SliderInt with
     the label arriving in ECX (recovered from call-site asm).
   - Config keys added in loader order: NoVisRecoil, AspectRatio,
     AspectRatioValue, FovChanger, FovChangerValue, ViewFovChangerValue.
   - Default FovChangerValue stays 51 because the running build was verified
     with 51; the 90 lower bound is a target quirk (static value can sit
     outside the slider range until touched).
   - Expected live: Visuals tab shows the three sliders; enable Aspect Ratio
     and pick 1/2/3 -> horizontal FOV/aspect changes; save+load persists the
     checkbox and slider values.
0t. **ESP/overlay layer mapped (next work item)**
   - Anti-aim verified live; all current gameplay features pass.
   - Overlay painters use **VGUI Surface031**, not ImGui:
     `sub_1003C410` (0x13BD, full ESP), `sub_1003B030` (0x7E7),
     `sub_1003B820` (0x772), `sub_1003BFB0` (0x21C), `sub_1003DB30` (0x355),
     `sub_1003AD40` (0x2E6, Indicators panel).
   - Helpers: `sub_1000C2A0` border rect, `sub_1000C1A0` colored bar+label,
     `sub_1000C390` formatted text (sprintf -> MultiByteToWideChar -> Surface
     slots 68/76/80/88), `sub_100044B0` velocity, `sub_100018B0` angular rate.
   - Missing prerequisite: the four VGUI fonts created in the EndScene init
     block (lines 303-364): Arial 14/700, Visitor TT2 BRK 13/500,
     Tahoma 13/700, Tahoma 24/700 via Surface slots 252 (CreateFont) and
     256 (configure). Globals 0x1004A9C8/9D0/9D4/9CC.
   - Order: (1) port font creation into EndScene init; (2) port C2A0/C1A0 +
     text helper; (3) enable the Panel VMT hook and port `sub_1003AD40`
     (Indicators) -> live verify; (4) then the full ESP painters.
   - Panel/ModelRender hooks remain disabled until the painter is tested.
0s. **Anti-aim added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x10005F8A: when Rage Enabled and not SMAC and
     `(buttons & 0x21)==0`: pitch -> -271/270/-1000 for Pitch=1/2/3;
     yaw -> -90 (RealYaw=1), +90 (2), 180 (3); desync based on
     `g_MenuState` byte 3, which Cheat_Update sets to 1 at entry
     (0x100055E5) making desyncActive always true (target quirk preserved).
   - Rage "Enabled" and Misc "SMAC Mode" checkboxes now bind the real flags;
     Pitch/Real Yaw/Desync combo indices are wired.
   - Expected live: enable Rage+Enabled, choose Pitch/Real Yaw, close menu ->
     view/aim snaps to the fake angles each tick (anti-aim); SMAC Mode
     disables it.
0r. **Full Bright + Remove Fog added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x100058B2: Remove Fog -> `fog_override=1`, `fog_enable=0`;
     off -> `fog_override=0`, `fog_enable=1`.
   - Cheat_Update 0x10005A21: Full Bright -> `mat_fullbright=1/0`.
   - Menu Misc checkboxes now bind g_RemoveFog / g_FullBright (target bytes
     0x1004ABDE / 0x1004ABDF).
   - INFERRED deviation: the target gates this block on `sub_100045A0()`
     && engine IsConnected/IsInGame; our build applies the convars
     unconditionally.
   - Expected live: enable Full Bright -> map fully lit; Remove Fog -> fog
     disappears; disabling restores both.
0q. **No Visual Recoil added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x10005BC1: ON sets `z_gun_vertical_punch=0`,
     `cl_viewbob=0`; OFF sets both to 1 (every tick, as in the target).
   - Visuals "No Visual Recoil" now binds `g_NoVisualRecoil`; Aspect Ratio
     left as a placeholder.
   - Expected live: enable it, fire -> no vertical view punch / bob.
0p. **FOV Changer added — NEW LIVE BUILD STAGED**
   - Cheat_Update 0x10005C17: when byte_1004AB88 is set, sets
     `cl_viewmodelfovsurvivor` = dword_1004AB90 and `fov_desired` =
     dword_1004AB8C through VEngineCvar007 (FindVar slot 13,
     ConVar::SetValue(int) slot 15). Defaults 51/51 (from 0x1003A300).
   - Menu Visuals "FOV Changer" now binds the real feature flag.
   - Expected live: enable FOV Changer -> viewmodel/world FOV jumps to 51.
0o. **Gameplay layer implemented — FIRST IN-GAME FEATURE STAGED**
   - `include/cusercmd.h` (64-byte layout), `include/netprop.h`,
     `src/netprop.cpp` (class-table build via VClient016 slot 7,
     RecvProp stride 60 lookup, local-player/velocity/flags accessors).
   - `src/hooks.cpp`: `Hook_EngineClient_Slot20` naked shim captures the
     caller's ESI as CUserCmd, calls the original slot-20 then `Cheat_Update`
     once per new command_number; installed via `VmtHook` slot 20 inside
     `InstallVmtHooks()`, which `Hook_EndScene` now calls after
     `InitInterfaces()` + `BuildNetpropTables()`.
   - `src/cheat.cpp`: `Cheat_Update` with the recovered Bunny Hop block
     (movement query ±450 / 7200/speed when SPACE held, on ground, no WASD).
   - INFERRED deviations: slot-20 uses a static last-command instead of the
     target's caller-EBP argument compare, plus an `IsBadReadPtr` guard; the
     `byte_1004A121` gate is modelled as "no movement keys held".
   - RISK: Panel/ModelRender pass-through detours are now installed; if the
     game destabilises, those two can be disabled independently.
   - **Crash fixed (first live attempt):** game closed on injection. Root
     cause in the naked slot-20 shim: MSVC `__fastcall` callee-pops its stack
     arguments, so the extra `add esp,8` after the call corrupted the stack.
     Removed. Diagnostics added: `BootLog` stages for EndScene init /
     netprop count / hooks / slot-20 entries / Cheat_Update enter-exit.
     Panel+ModelRender pass-through hooks are temporarily disabled; only
     slot-20 is installed.
   - Expected live: Misc -> Bunny Hop on -> close menu -> stand on ground and
     hold SPACE -> forward speed boost.
0n. **Gameplay layer mapped (CUserCmd, netprops, bunny hop)**
   - `Cheat_Update`'s `this` is the **CUserCmd**, layout proven by the code:
     crc 0, command_number 4, tick_count 8, viewangles 12/16/20,
     forwardmove 24, sidemove 28, upmove 32, buttons 36, impulse 40,
     weaponselect 44, weaponsubtype 48, random_seed 52, mousedx 56 (word),
     mousedy 58, hasbeenpredicted 60. No vtable, no aimdirection.
   - Bunny Hop block (0x10005E20): enabled = `HIBYTE(word_1004ABF0)` (byte
     0x1004ABF1). If VK_SPACE and `(m_fFlags & 1)==0` and byte_1004A121,
     writes forwardmove/sidemove from a velocity query (±450 clamp,
     7200/speed), occasionally setting `*(float*)(cmd+28)`.
   - Helpers are netprop accessors: `sub_100015A0(entity)` = offset of
     `m_fFlags` in `DT_BasePlayer`; `sub_100044B0(entity,&out)` = copies
     `m_vecVelocity[0]` (12 bytes). Both call `sub_100374A0(table, name, 0)`
     for the offset and use the class-table vector `dword_1004AA54/AA58`.
   - Prerequisites before any live gameplay behavior:
     1. build the client class/netprop table vector (EndScene init lines
        253–279: walk `VClient016` class list, push table pointers);
     2. recover `sub_100374A0` (netprop offset lookup);
     3. recover the netprop accessor cluster (0x100012B0–0x10001EC0);
     4. implement `Hook_EngineClient_Slot20` with the caller-ESI capture +
        install it via `VmtHook` slot 20, calling `Cheat_Update`.
   - Next live target: **Bunny Hop** (hold SPACE while strafing → movement
     boost), then the aimbot/ESP blocks.
0m. **Config profiles wired — NEW LIVE BUILD STAGED**
   - Profile combo recovered: label `Config###Configz`,
     items {Legit, Rage, HvH}, index global 0x1004B070.
   - Paths: legit.ini / rage.ini / hvh.ini under `C:\L4D2space`; load/save and
     auto-load now use the selected profile.
   - Expected live: switch profile in Misc, click save -> the matching ini is
     created/updated; load reads the selected profile.
0l. **Config persistence wired — NEW LIVE BUILD STAGED**
   - Keys from the loader (0x10037B00): section `Config`, file
     `C:\L4D2space\legit.ini`.
   - Persisted: ENABLEAIMLEGIT, LegitAimHitboxMode, LEGITAIMFOV, AimFov,
     Pitch, RealYaw, DesyncYaw (LoadSettings/SaveSettings in `src/menu.cpp`).
   - Misc `load`/`save` buttons now call them; settings auto-load on first
     menu render. `CreateDirectoryA("C:\\L4D2space")` before save.
   - Smooth Y and the remaining checkbox/visual settings still need their
     exact loader keys before being persisted.
   - Expected live: toggle/slider changes -> save -> file created; load
     restores; values survive a game restart.
0k. **Combos + sliders added — NEW LIVE BUILD STAGED**
   - `sub_100312F0` = `ImGui::SliderInt(label in ECX, int* v, min, max)`
     (`"%d"` format confirmed in the decompilation).
   - `sub_100080D0`/`sub_100311A0` = `ImGui::Combo(label, int* index,
     const char* const* items, count)`; item width 100.
   - Legit: `Combo("Hitbox", …, {Auto, Head Only})`,
     `SliderInt("FOV", …, 0, 15)`, `SliderInt("Smooth Y", …, 0, 20)`.
   - Rage: `SliderInt("FOV", …, 0, 180)`,
     `Combo("Pitch###Pitchemz", …, {None, Fake Up, Fake Down, Fake Zero})`,
     `Combo("Real Yaw###Yaw", …, {None, Left, Right, Backwards})`,
     `Combo("Desync Yaw###YawFake", …, same yaw items)`.
   - Expected live: Legit/Rage show working combos and draggable sliders.
0j. **Buttons added — NEW LIVE BUILD STAGED**
   - `sub_1002F1F0` identified as `ImGui::Button(label, ImVec2)`.
   - Visuals: `Main##chasdjas` and `Materials##dashkas` (120x20) added.
   - Misc: `load` and `save` (120x25) added.
   - Click actions (chams material switch, config load/save) and the
     combo/slider helpers (`sub_1000BAE0`/`sub_100080D0`, `sub_100312F0`)
     remain PENDING.
   - Expected live: the four tabs show checkbox sets plus these buttons;
     buttons are visual-only for now.
0i. **All four tabs populated — NEW LIVE BUILD STAGED**
   - Rage (15): Enabled, Auto Shoot, Silent Aim, Perfect Silent Aim,
     Shotgun Mode, Auto Shove, Auto Melee, No-Recoil, No-Spread,
     Position Adjustment, Prediction Fix, Priority Special, Hitscan,
     Ignore Team, Ignore Idle Witch.
   - Visuals (21): ESP set (Enable ESP … Thirdperson incl.
     "Weapons ESP##weapons2"), chams (Player/Hand/XQZ), FOV Changer,
     Aspect Ratio, No Visual Recoil.
   - Misc (19): Bunny Hop, SpeedHack, Air Stuck, Teleport##jfsdagkfs,
     Fake Lag, Chat Spam, Remove Vomit/Fog, Full Bright, Invisible Jockey,
     Lag Exploit, Name Stealer, Server Lagger##xczvcxv, No Fall Damage,
     Auto Pistol, Instant Weapon Switch, Instant Revive, Anti Smoker,
     Indicators, SMAC Mode.
   - Menu size raised to 600x650 to fit the largest tab (INFERRED).
   - Expected live: every tab shows its recovered checkbox set and toggles.
0h. **Legit tab checkboxes recovered — NEW LIVE BUILD STAGED**
   - `sub_10030180(label, bool*)` identified as `ImGui::Checkbox`.
   - Legit-tab labels ported in order: Enabled, Shotgun Mode, No-Recoil,
     No-Spread, Position Adjustment, FOV Circle, Priority Special, Hitscan,
     Ignore Team, Ignore Idle Witch, trigger Enabled, Aim At Head, Aim At Body,
     Auto Shove.
   - Settings are local statics for now; binding to the recovered ini loader
     (0x10037B00) and the hitbox/trigger combo helpers
     (`sub_100080D0`/`sub_100312F0`) remain PENDING.
   - **Live issue found (2026-10-25):** the shell-only run had persisted
     `[Window][L4D2 bot by space785] Size=600,69` in the game's `imgui.ini`;
     with NoResize the tab content was clipped and unreachable. Fixed by
     `SetNextWindowSize(600x450, ImGuiCond_Always)` in `Menu_Render`
     (INFERRED size; original size not yet recovered). Re-staged for retest.
   - Expected live: INSERT -> 600x450 window -> Legit tab shows the checkbox
     tree; entries toggle on click.
0g. **Menu shell recovered — LIVE-VERIFIABLE BUILD STAGED**
   - Title decoded from `xmmword_10047630` + immediate: **"L4D2 bot by space785"**
     (20 bytes, XOR key `i-71`).
   - Begin flags `8234` = NoResize(2)|NoScrollbar(8)|NoCollapse(32)|
     NoBringToFrontOnFocus(8192).
   - Tab labels order: Legit(0), Rage(1), Visuals(2), Misc(3); selected index
     at 0x1004A99C; tab hit box 140x30.
   - INSERT (VK 45) toggles `g_MenuState` byte 0 after rendering (recovered
     from the EndScene epilogue); menu draws only when that byte is set and
     WndProc already routes mouse input under the same gate.
   - `src/menu.cpp` now draws the real window + four tab buttons; per-tab
     widget trees and config bindings remain PENDING.
   - Staged: `staging/rebuilt/VR.dll`. Expected live: inject -> watermark
     popup -> dismiss -> press INSERT -> "L4D2 bot by space785" window with
     Legit/Rage/Visuals/Misc tabs; INSERT again closes it.
0f. **Frame skeleton ported** (done): `Hook_EndScene` now runs the standard
   ImGui frame path after init — `ImGui_ImplWin32_NewFrame()`,
   `ImGui::NewFrame()`, `Menu_Render()` (empty stub in `src/menu.cpp`),
   `ImGui::Render()`, `ImGui_ImplDX9_RenderDrawData()`. Build green and
   staged; live behavior is still popup-only because the menu body is empty.
   `sub_1000BC00` identified as the MSVC `std::string(const char*)`
   constructor (SSO: buffer 16, size +16, capacity +20) — library code, not
   a config getter.
   Next: port `Menu_Render` + the config getter/ini layer + the 0x30-byte
   menu-state object.
0e. **Frame/menu path — reconnaissance done, port PENDING**
   - `sub_10001050` (0x10001050) allocates a 0x30-byte zeroed block, calls
     `Menu_Render()` with no arguments, stores the block in `dword_1004B0FC`.
   - **Correction (2026-10-25):** `0x10037B00` is not purely a menu builder.
     Its first section (0x10037B2D onward) is a **config loader**: hundreds of
     `sub_1000BC00(&key, "Config")` pair constructions followed by
     `sub_10039670(&boolGlobal, ...)` / `sub_100398A0(&intGlobal, ...)` reads.
     The UI part starts later (first UI string "Aimbot" at 0x10037FA4).
   - The visible tab strings are split: "Visuals" (0x10041B98) is referenced
     only inside `Hook_EndScene` (0x10009D62), and "Aimbot" (0x10041BA8) is
     referenced both in `Hook_EndScene` (0x10009E06/0x10009E39/0x1000A2FD/
     0x1000A330) and in `Menu_Render` (0x10037FA4). So part of the menu UI is
     inline in the EndScene detour (LTCG), not exclusively in Menu_Render.
   - Snapshots saved: `NOTES/ida/Menu_Render.json` and
     `NOTES/ida/Menu_Render.asm.json`.
   - ImGui address-map attempts are documented in `NOTES/verification.md`:
     ordered-size alignment only matched 31/234, and direct address
     correspondence is invalid (0/10 size matches — link order differs).
     Content matching was attempted (`tools/imgui_match.py`): 34 operand-
     exact and 39 mnemonic-only matches, none of the core APIs — LTCG
     specialized bodies differently, so no address map is recoverable.
     **Pivot:** identify ImGui calls at each target call site by argument
     shape (Begin = string + bool* + flags; Checkbox = label + bool*;
     SliderFloat = label + float* + min/max/fmt; Button = label only;
     combo = string-array + int*), then port the EndScene UI section
     (0x10009D00–0x1000A400) and Menu_Render UI (0x10037F00+) and validate
     visually against the original.
   - The ImGui `NewFrame`/`Render`/`RenderDrawData` calls are inline inside
     `Hook_EndScene` (decomp lines ~400–1330), not in a separate wrapper.
   - Next: port that inline frame block (one-time `ImGui_ImplWin32_NewFrame`,
     `ImGui::NewFrame`, `sub_10001050()`/`Menu_Render`, `ImGui::Render`,
     `ImGui_ImplDX9_RenderDrawData`), then recover `Menu_Render` in chunks
     together with the `sub_1000BC00` config-getter layer and the 0x30-byte
     menu-state struct.
0d. **Startup popup recovered** (done): the XOR-obfuscated MessageBox at
   0x10009754 decodes to caption `L4D2` and text
   `This cheat was released for free on wwww.unknowncheats.com by @space785,
   if you paid for it you got scammed.` (four `w`s preserved). Decoded by
   reading the static blobs 0x10047510/0x100475F0/0x10047600/0x10047640/
   0x10047650 + literal, key `(i - 71)`. Ported as a one-shot `MessageBoxA`
   inside the EndScene lazy-init block; build staged for live testing.
   Expected live behavior now: inject -> modal popup freezes the game ->
   dismiss -> **no GUI yet** (frame/menu path still PENDING).
0c. **EndScene lazy init ported** (done): `Hook_EndScene` now saves/restores
   D3DRS_COLORWRITEENABLE + vertex declaration + vertex shader around the
   frame, and on first call runs `ImGui::CreateContext`, sets
   `ConfigFlags_NoMouseCursorChange`, `ImGui_ImplWin32_Init`,
   `ImGui_ImplDX9_Init`, `ImGui_ImplDX9_CreateDeviceObjects`, then
   `InitInterfaces()`. `InstallVmtHooks()` remains uncalled until its detour
   ABIs are live-tested. No visuals yet (frame/menu path still PENDING).
   Confirmed: `sub_100110A0` is **ImGui::CreateContext** (malloc 0x3B88 =
   15240 = sizeof(ImGuiContext) in 1.74), and `g_pImGuiContext` is ImGui's
   own `GImGui` global, not a cheat wrapper.
0b. **`VmtHook` recovered** (done): `include/vmt.h` + `src/vmt.cpp`
   implement the constructor from 0x10007750 (IDA-verified 12-byte layout;
   malloc(count+1), memcpy to +1, RTTI word from origVtable[-1], redirect
   interface vtable to copy+1). Builds clean; not yet instantiated.
   Next: add the three detour prototypes with their ABI quirks
   (`__fastcall` thisptr for PaintTraverse; explicit `g_pModelRender` this for
   DrawModelExecute; ESI/CUserCmd quirk deferred for slot 20), save originals
   from vtable slots 41/20/19, and install via `VmtHook::entry()` inside the
   recovered `Hook_EndScene` lazy-init block.
0. **Build skeleton** (done): `CMakeLists.txt` + `tools/build.bat` compile
   vendored ImGui 1.74 + MinHook 1.3.3 + Kiero 1.2.9 with MSVC 14.38 x86
   (`/O2 /MD /GS /GL`, link `/LTCG`, no CFG) into `build/Release/VR.dll`.
   `include/recovered.h`, `src/dllmain.cpp` (DllMain/thread/window discovery),
   `src/hooks.cpp` (WndProc + inert EndScene), `src/interfaces.cpp`
   (`InitInterfaces`) build clean.
   Verification tooling lives in `tools/dump_funcs.*` and `tools/dump_one.*`
   (headless IDA + PDB symbol lookup); workflow in `NOTES/verification.md`.
   `FindPattern` (0x100376C0) and `ConsolePrint` (0x100042D0) are recovered
   in `src/utils.cpp`; `InitInterfaces` now measures 0x1D2 vs target 0x207.
   **Always clean-build before measuring** (stale `/GL` IL bit us once).
   Next build task: recover `Hook_EndScene`'s lazy-init/VMT-hook block
   (0x10009160 lines 173–302: ImGui backend init, `InitInterfaces`, three
   `VmtHook` installs, state save/restore, frame + menu, END-key toggle).
1. ~~Recover the public/hook surface prototypes (Phase 1).~~ **In progress:**
   set exact prototypes for `CheatThread_Main`, `EnumWindows_FindGameWindow`,
   `WndProc_Hook`, `Hook_EndScene`, `Hook_PaintTraverse`,
   `Hook_EngineClient_Slot20` (needs disassembly to resolve the
   `__userpurge`/ESI convention), `Hook_DrawModelExecute`, `VmtHook_Init`,
   `InitInterfaces`, `Cheat_Update`, `Menu_Render`.
   Then declare the `VmtHook` and `CGlobalVars` types in the IDB and verify
   with `sizeof`/`offsetof` expectations.
2. Rebuild Source interface structs/vtables in IDA (Phase 2) for
   `IVEngineClient`, `IPanel`, `IVModelRender`, `IClientEntityList`,
   `IVEngineCvar`, `IPlayerInfoManager`, and the ImGui IO layout.
3. Recover the netprop accessor cluster (leaf functions) and config parser
   first; these are pure and unit-testable.
4. Set up the build (MSVC 14.38 x86, /LD /MD /O2 /GS /guard:cf) and a staging
   harness using `SKernelInjector.exe`; keep `original/target.dll` untouched.
5. Recover `sub_100055B0` (main update), then the menu `sub_10037B00`, then
   the ESP/chams/aimbot helpers; verify each batch structurally.
6. FLIRT/library replacement: compile/obtain ImGui (version TBD), MinHook
   1.3.3, and Kiero; match against the binary before excluding ranges.
