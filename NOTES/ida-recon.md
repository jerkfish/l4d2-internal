# IDA Phase 0/1 recon — target.dll

IDA 9.0, 32-bit database, base `0x10000000`, SHA-256 verified against
`original/target.dll`. All addresses are image VAs.

## 1. Function inventory

- `list_functions`: **509 functions**.
- Named by IDA: 89 (`_DllMain@12`, `StartAddress`, `EnumFunc`, `CompareFunction`,
  the CRT family, imports/thunks).
- 420 remain `sub_*`.
- Size distribution: 86 functions < 0x20; 95 in 0x20–0x7F; 212 in 0x80–0x1FF;
  95 in 0x200–0x7FF; 21 ≥ 0x800.
- Largest: `sub_100166A0` 0x2A5F, `sub_10009160` 0x26BE, `sub_100055B0` 0x20EA,
  `sub_10037B00` 0x1B68.

## 2. Link-order / library boundary map

| Range | Funcs | Bytes | Classification | Evidence |
|---|---:|---:|---|---|
| `0x10001010–0x1000E360` | 110 | 52,777 | **User code** (features, netprops, hook bodies) + some inlined ImGui/STL | 19 netprop accessors, `sub_100055B0` main update, `sub_10009160` EndScene detour, `sub_10004980` chams |
| `0x1000E360–0x10032AD0` | 234 | 146,964 | **ImGui core** (statically compiled) | huge widget functions; 9 direct ImGui-string functions; only 18 string-bearing functions overall; all large call clusters internal |
| `0x10032AD0–0x10033EE0` | 9 | 5,059 | **Kiero + engine bridge** | `sub_10033B10` references `Kiero` / `Kiero DirectX Window`; `sub_10033900` is user interface setup (7 `CreateInterface` calls) |
| `0x10033EE0–0x10035BF0` | 12 | 6,910 | **MinHook 1.3.3 + HDE64** | 8-function BFS cluster from `MH_CreateHook`/`MH_EnableHook`; no strings |
| `0x10035BF0–0x10037B00` | 16 | 7,776 | **User utility / engine glue** | `sub_100376C0` wildcard pattern scanner; `sub_10035BF0` console-command code |
| `0x10037B00–0x1003DF80` | 23 | 25,140 | **User menu/ESP/config** | `sub_10037B00` has 86 cheat + 20 config strings; config load/save at `sub_10039AD0`/`sub_10039F20`; ESP/world renderers |
| `0x1003DF80–0x1003F4A0` | 101 | 5,296 | **CRT / SEH / security** | IDA-named `__scrt_*`, `__security_*`, `_except_handler4`, math helpers |
| `0x1003F4A0+` | 4 | 265 | CRT/security tail | tiny helpers |

The ImGui range boundary is fuzzy: user code calls ImGui API functions, and a
few helpers at `0x1000E360+` (e.g. `sub_1000E360`, `sub_1000E400`) may be STL or
user utilities. Treat the range as library until individual evidence says
otherwise; log exceptions.

## 3. Entry chain / initialization

```
DllEntryPoint (0x1003EAB3, CRT)
  -> __security_init_cookie (0x1003EEE1)
  -> dllmain_dispatch (0x1003E97D)
       -> DllMain (0x1000BA00)
            DLL_PROCESS_ATTACH:
              DisableThreadLibraryCalls(hinst)
              CreateThread(0, 0, StartAddress, hinst, 0, 0)
            DLL_PROCESS_DETACH:
              if (dword_1004B180) sub_10034E20(0,0); free(Block); Block=0;
```

`StartAddress` (0x1000B960, 0x98 bytes):

1. `sub_10033B10(1)` — **Kiero init** for D3D9:
   - registers class `"Kiero"`, creates `"Kiero DirectX Window"`,
   - `Direct3DCreate9(32)`, `CreateDevice(HAL, ...)`,
   - copies the D3D9 device vtable (0x1DC bytes) into `Block = calloc(0x77,4)`,
   - sets `dword_1004B180 = 1`.
2. If initialized: `sub_10034AE0(Block[42], ...)` — **MinHook create-hook** on
   `IDirect3DDevice9::EndScene` (vtable index 42); detour is `sub_10009160`.
3. `EnumWindows(EnumFunc, 0)` until the first top-level window of the current
   process is found (`hWnd`).
4. `SetWindowLongA(hWnd, GWL_WNDPROC, sub_1000B830)` — WndProc hook.
5. Outer loop ends; then `while (!GetAsyncKeyState(VK_END))` — kill switch.

`sub_10034E20` is MinHook enable/disable; DllMain detach calls it with `(0,0)`.

## 4. EndScene detour and VMT hooks

`sub_10009160` (0x26BE) is the EndScene detour. It contains compiled-in ImGui
DX9/Win32 backend strings (`imgui_impl_dx9` at 0x1000933E, `imgui_impl_win32` at
0x10009280) and does lazy initialization:

- calls `sub_10033900()` at 0x100093F5 (interface + VMT hooks),
- renders ImGui; calls `sub_10037B00` (menu builder) through the menu/UI path,
- reads `imgui.ini` / `imgui_log.txt` via `sub_1000E010`,
- loads `C:\Windows\Fonts\Courbd.ttf` via `sub_100299C0`,
- has 60 cheat + 20 ImGui + 2 SMAC strings (menu/hook logic).

VMT hook class `sub_10007750(interface)`:

- `[obj+0]` = interface pointer, `[obj+4]` = original vtable,
  `[obj+8]` = allocated vtable copy; the interface's vtable pointer is
  redirected to the copy.
- The copy is patched at one slot; the original slot is saved for calling.

Hooks installed in `sub_10009160`:

| # | Interface global | Interface | Vtable slot | Detour | Saved original |
|---|---|---|---:|---|---|
| 1 | `dword_1004A9F8` | `VGUI_Panel009` (vgui2) | 41 (`+0xA4`) | `sub_10004FC0` | `dword_1004A9AC` (slot 41) |
| 2 | `dword_1004A9F0` | `VEngineClient013` (engine) | 20 (`+0x50`) | `sub_100076A0` | `dword_1004A998` (slot 20) |
| 3 | `dword_1004AA00` | `VEngineModel016` (engine) | 19 (`+0x4C`) | `sub_10004980` | `dword_1004A9B4` (slot 19) |

Notes:
- Slot 41 of `IPanel` is `PaintTraverse` → `sub_10004FC0` (ESP/overlay).
- Slot 19 of `IVModelRender` is `DrawModelExecute` → `sub_10004980` (chams;
  references `debug/debugwireframe`, `particle/screenspaceboomervomit`,
  `debug/debugambientcube`, model textures).
- Slot 20 of `IVEngineClient` → `sub_100076A0`. It calls the saved original,
  caches `*(a1+4)` (looks like `CUserCmd::command_number`, but the interface
  slot should be confirmed against the correct SDK header), and calls the main
  update `sub_100055B0` once per new command value.
- The two client.dll pattern scans in `sub_10033900` are performed by
  `sub_100376C0` (`FindPattern`-style wildcard scanner). Their return values are
  discarded at the call sites; they are likely version/validity checks, not the
  VMT hook targets. Confirm with xrefs/globals later.
- EndScene hook itself is installed through MinHook, not the VMT class.

## 5. Interface globals (from `sub_10033900`)

`sub_10033900` calls `GetProcAddress(module, "CreateInterface")` for engine.dll,
client.dll, vstdlib.dll, vguimatsurface.dll, vgui2.dll, materialsystem.dll,
server.dll, then:

| Global | Module | Interface string |
|---|---|---|
| `dword_1004AA0C` | server.dll | `PlayerInfoManager002` |
| `dword_1004AA10` | client.dll | `VClient016` |
| `dword_1004A9EC` | vstdlib.dll | `VEngineCvar007` |
| `dword_1004A9F0` | engine.dll | `VEngineClient013` |
| `dword_1004A9E4` | client.dll | `VClientEntityList003` |
| `dword_1004A9F4` | engine.dll | `VModelInfoClient004` |
| `dword_1004AA00` | engine.dll | `VEngineModel016` |
| `dword_1004A9FC` | materialsystem.dll | `VMaterialSystem080` |
| `dword_1004A9F8` | vgui2.dll | `VGUI_Panel009` |
| `dword_1004AA08` | vguimatsurface.dll | `VGUI_Surface031` |
| `dword_1004A9E8` | engine.dll | `EngineTraceClient003` |
| `dword_1004AA04` | engine.dll | `VDebugOverlay003` |

After resolution: calls `PlayerInfoManager002` vtable slot 1 (`GetLocalPlayer`-
like) and stores the result in `dword_1004A9E0`. If `VEngineCvar007` is null,
prints `pConsole failed (NULL)` via `sub_100042D0`.

## 6. Key user functions

| Address | Size | Name/role | Evidence |
|---|---:|---|---|
| `0x1000BA00` | 0x76 | `DllMain` | thread spawn / detach cleanup |
| `0x1000B960` | 0x98 | `StartAddress` | Kiero init → MinHook → window hook → End key |
| `0x1000B920` | 0x39 | `EnumFunc` | finds current-process top-level window |
| `0x1000B830` | 0xE7 | WndProc hook | ImGui handler `sub_1002E1C0`, mouse state, calls original |
| `0x10009160` | 0x26BE | EndScene detour + ImGui backends | `imgui_impl_*`, menu, SMAC, interface init |
| `0x10033900` | 0x207 | interface + VMT hook setup | 12 `CreateInterface` calls |
| `0x10007xxx` | 0x78 | VMT hook class (`sub_10007750`) | allocates/copies vtable |
| `0x100076A0` | 0xAE | IVEngineClient slot-20 detour | calls main update |
| `0x10004FC0` | 0x408 | IPanel slot-41 detour | ESP/world/overlay |
| `0x10004980` | 0x639 | IVModelRender slot-19 detour | chams/materials |
| `0x100055B0` | 0x20EA | main update | 39 callees, netprops, SMAC detection |
| `0x10037B00` | 0x1B68 | menu builder | 86 cheat + 20 config strings |
| `0x10039AD0` / `0x10039F20` | 0x447/0x3D0 | config load/save | `C:\L4D2space\legit.ini` |
| `0x100012B0`…`0x10001EC0` | ~0x100 each | netprop offset accessors | `DT_*` + `m_*` string pairs |

User-ish string evidence: 9 functions with cheat strings, 23 with netprop
strings, 12 with ImGui internal strings. Many user helpers (math, vectors,
config) have no strings and must be classified by call graph.

## 7. Key globals

| Global | Meaning (INFERRED / evidence) |
|---|---|
| `Block` (0x1004B0F0-ish; confirm exact) | D3D9 device vtable copy (0x77 dwords) |
| `dword_1004B180` | Kiero initialized flag |
| `dword_1004A9D8` | ImGui IO/context pointer (WndProc handler + menu) |
| `dword_1004B054` | menu-open / capture gate (WndProc checks byte) |
| `dword_1004AA14` | MinHook global lock (interlocked) |
| `hHeap` | MinHook heap |
| `lpAddress` / `lpMem` / `dword_1004B648` | MinHook trampoline/hook lists |
| `hWnd`, `lpPrevWndFunc` | game window and original WndProc |
| `dword_1004A9B8`/`A9C0`/`A988` | VMT hook objects |
| `dword_1004A998`/`A9AC`/`A9B4` | saved original function pointers |

Exact `Block`/`hHeap` addresses need `list_globals_filter`; pending.

## 8. RTTI / strings confirmed under IDA

- `.?AUtracefitlers@@` at 0x10049164 (only non-std RTTI name; typo preserved).
- `CreateInterface` at 0x10046668; interface names at 0x10046754+.
- `Kiero` at 0x100468A8, `Kiero DirectX Window` at 0x100468B0.
- `imgui_impl_dx9` 0x100465F8, `imgui_impl_win32` 0x10046608.
- Config paths 0x10045B0C–0x10045B4C.
- SMAC strings 0x100417FC, 0x10041B38, 0x100421B0.
- PDB path 0x1004673C.

## 9. Implications for recovery

- **No exports**: Phase 1 surface is `DllMain`, the thread, the WndProc, and the
  three VMT hook callbacks. Reconstruct those prototypes first.
- **ImGui**: the 234-function core is a library. Replace with the matching
  ImGui version (likely 1.6x–1.7x; exact version TBD) rather than recovering it.
  The user-authored UI is `sub_10037B00`, `sub_1000CCA0`, `sub_10008ED0`, and
  the menu/config helpers.
- **MinHook**: 8 core functions + HDE64; replace with upstream MinHook 1.3.3.
- **Kiero**: `sub_10033B10` is a single inlined init; replace with Kiero source.
- **CRT**: replace with real CRT; exclude.
- **Verification**: the rebuilt DLL must be injected into L4D2 and compared
  against the original. Pure functions (netprop offsets, config parser, pattern
  scanner, math) can be unit-tested in a synthetic host.

## 10. Open questions / next steps

1. Exact ImGui version and compile flags (needed for library replacement and
   codegen matching).
2. Exact MinHook/Kiero upstream revisions (MinHook 1.3.3 confirmed by resource;
   Kiero version TBD).
3. Confirm the three vtable slot numbers against the L4D2 SDK headers.
4. Map `dword_1004B054`, `dword_1004A9D8`, `dword_1004A9E0`, and the `Block`
   global exact addresses.
5. Start Phase 1 prototypes for `DllMain`, `StartAddress`, the WndProc, and the
   three detours; then Phase 2 structs for the Source interfaces and ImGui IO.
