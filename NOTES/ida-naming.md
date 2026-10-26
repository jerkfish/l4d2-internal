# IDA naming log — target.dll

Date: 2026-10-25. Database: IDA 9.0, 32-bit, image base `0x10000000`,
SHA-256 verified against `original/target.dll`.

Names below were applied to the IDB. Every non-obvious name is **INFERRED**
from the cited decompilation/disassembly; nothing here is a guess without
evidence. Source files must carry the same `// INFERRED: ...` marker.

## 1. Functions

| Address | Name | Evidence |
|---|---|---|
| `0x1000BA00` | `DllMain` | Already named; thread spawn / detach cleanup (`free(g_pD3D9VTableCopy)`) |
| `0x1000B960` | `CheatThread_Main` | `CreateThread` start routine; Kiero → MinHook → find window → WndProc → VK_END loop |
| `0x1000B920` | `EnumWindows_FindGameWindow` | `EnumWindows` callback; `GetWindowThreadProcessId` == current PID |
| `0x1000B830` | `WndProc_Hook` | `SetWindowLongA(GWL_WNDPROC)` target; calls ImGui Win32 handler and `CallWindowProcA(g_pfnPrevWndProc, ...)` |
| `0x10009160` | `Hook_EndScene` | MinHook target for D3D9 slot 42; ImGui `imgui_impl_dx9` / `imgui_impl_win32` strings |
| `0x10033900` | `InitInterfaces` | 12 `CreateInterface` resolutions; `g_pGlobalVars = PlayerInfoManager002->slot1()` |
| `0x100076A0` | `Hook_EngineClient_Slot20` | VMT detour installed at `VEngineClient013` vtable `+0x50`; calls original then `Cheat_Update` |
| `0x10004FC0` | `Hook_PaintTraverse` | VMT detour at `VGUI_Panel009` vtable `+0xA4` (slot 41); ESP/world draw + `FocusOverlayPanel` / `MainMenu` |
| `0x10004980` | `Hook_DrawModelExecute` | VMT detour at `VEngineModel016` vtable `+0x4C` (slot 19); chams material strings |
| `0x100055B0` | `Cheat_Update` | Called once per new command number from the slot-20 detour; 39 callees |
| `0x10037B00` | `Menu_Render` | 86 cheat strings + 20 config strings; called from the EndScene frame path |
| `0x10007750` | `VmtHook_Init` | Allocates 12-byte object, counts vtable entries, `memcpy`, redirects `*iface`; **constructor** (`new VmtHook(iface)` pattern at call sites) |
| `0x10033B10` | `Kiero_Init` | Strings `Kiero`, `Kiero DirectX Window`; `Direct3DCreate9(32)`, `CreateDevice`, vtable copy `0x1DC` |
| `0x10034AE0` | `MH_CreateHook` | MinHook 1.3.3 trampoline allocator; version resource + import fingerprint |
| `0x10034E20` | `MH_EnableHook` | MinHook enumerate/enable/disable; called as `MH_EnableHook(0,0)` on detach |
| `0x100376C0` | `FindPattern` | Wildcard byte-pattern scanner; two call sites in `InitInterfaces` (results discarded) |

Already named by IDA (not renamed): `_DllMain@12`, `DllEntryPoint`,
`__security_init_cookie`, `dllmain_dispatch`, `StartAddress` (kept as IDA name
because the recovered source will use `CheatThread_Main`), CRT family, imports.

## 2. Globals

### 2.1 Source engine interfaces (from `InitInterfaces`)

| Address | Name | Interface string | Module |
|---|---|---|---|
| `0x1004A9E4` | `g_pClientEntityList` | `VClientEntityList003` | client.dll |
| `0x1004A9E8` | `g_pEngineTrace` | `EngineTraceClient003` | engine.dll |
| `0x1004A9EC` | `g_pEngineCvar` | `VEngineCvar007` | vstdlib.dll |
| `0x1004A9F0` | `g_pEngineClient` | `VEngineClient013` | engine.dll |
| `0x1004A9F4` | `g_pModelInfoClient` | `VModelInfoClient004` | engine.dll |
| `0x1004A9F8` | `g_pVGuiPanel` | `VGUI_Panel009` | vgui2.dll |
| `0x1004A9FC` | `g_pMaterialSystem` | `VMaterialSystem080` | materialsystem.dll |
| `0x1004AA00` | `g_pModelRender` | `VEngineModel016` | engine.dll |
| `0x1004AA04` | `g_pDebugOverlay` | `VDebugOverlay003` | engine.dll |
| `0x1004AA08` | `g_pVGuiSurface` | `VGUI_Surface031` | vguimatsurface.dll |
| `0x1004AA0C` | `g_pPlayerInfoManager` | `PlayerInfoManager002` | server.dll |
| `0x1004AA10` | `g_pClient` | `VClient016` | client.dll |
| `0x1004A9E0` | `g_pGlobalVars` | `PlayerInfoManager002` slot 1 result | — |

`g_pGlobalVars` is confirmed by its only uses: `*(float*)(g_pGlobalVars + 28)`
multiplied with tick counts, i.e. `CGlobalVarsBase::interval_per_tick` at
offset 28 (decompiled `sub_100055B0` lines 976 and 1178).

### 2.2 VMT hook state

| Address | Name | Evidence |
|---|---|---|
| `0x1004A9B8` | `g_pVmtHook_Panel` | `VmtHook_Init(g_pVGuiPanel)` result in `Hook_EndScene` at 0x1000949A |
| `0x1004A9C0` | `g_pVmtHook_EngineClient` | `VmtHook_Init(g_pEngineClient)` at 0x100094E2 |
| `0x1004A988` | `g_pVmtHook_ModelRender` | `VmtHook_Init(g_pModelRender)` at 0x10009524 |
| `0x1004A9AC` | `g_pfnPaintTraverse_Orig` | read from original vtable `+164` (slot 41) at 0x100094C3 |
| `0x1004A998` | `g_pfnEngineClientSlot20_Orig` | read from original vtable `+80` (slot 20) at 0x10009505 |
| `0x1004A9B4` | `g_pfnDrawModelExecute_Orig` | read from original vtable `+76` (slot 19) at 0x1000954B |
| `0x1004A98C` | `g_pfnEndScene_Orig` | MinHook trampoline for EndScene; `return g_pfnEndScene_Orig(a1)` at 0x1000B801 |

`Hook_EndScene` also saves/restores three D3D9 states around the frame:

| Address | Name | Evidence |
|---|---|---|
| `0x1004A9A8` | `g_SavedColorWriteEnable` | `GetRenderState(168)` at 0x1000919D / `SetRenderState(168, ...)` at 0x1000B7C5 (168 = `D3DRS_COLORWRITEENABLE`) |
| `0x1004A9A0` | `g_pSavedVertexDeclaration` | `GetVertexDeclaration` at 0x100091AB / `SetVertexDeclaration` at 0x1000B7E4 |
| `0x1004A9A4` | `g_pSavedVertexShader` | `GetVertexShader` at 0x100091B9 / `SetVertexShader` at 0x1000B7F3 |

### 2.3 Bootstrap / runtime state

| Address | Old name | New name | Evidence |
|---|---|---|---|
| `0x1004B16C` | `Block` | `g_pD3D9VTableCopy` | `calloc(0x77, 4)` in `Kiero_Init`; freed in `DllMain` detach |
| `0x1004B144` | `hWnd` | `g_hGameWindow` | result of `EnumWindows` in `CheatThread_Main`; target of `SetWindowLongA` |
| `0x1004A9B0` | `lpPrevWndFunc` | `g_pfnPrevWndProc` | saved by `SetWindowLongA`, used by `CallWindowProcA` |
| `0x1004B180` | `dword_1004B180` | `g_bKieroInitialized` | set by `Kiero_Init`, checked by `DllMain` detach |
| `0x1004B160` | `dword_1004B160` | `g_pD3D9Device` | `= a1` (device argument) at 0x1000934B in `Hook_EndScene` |
| `0x1004B168` | `dword_1004B168` | `g_hWnd_ImGuiBackend` | `= hWnd` at 0x10009276 in the inlined `imgui_impl_win32` init |
| `0x1004A9D8` | `dword_1004A9D8` | `g_pImGuiContext` | written with ImGui backend init fields: `ConfigFlags`/`BackendFlags` at +8/+12, `KeyMap[]` defaults at +0x34, `BackendPlatformName` at +0xBC, `ImeWindowHandle` at +0xE0, `MousePos` +0xE8, `MouseDown` +0xF0; large offsets (+5392…+7232) match `ImGuiContext`, not `ImGuiIO` |
| `0x1004B054` | `dword_1004B054` | `g_MenuState` | byte 0 = menu open (WndProc mouse capture gate, `PaintTraverse` argument, toggled by END key in `Hook_EndScene`); byte 3 used as a mask in the slot-20 detour; **layout TBD** |
| `0x1004AA18` | `hHeap` | `g_hMinHookHeap` | MinHook heap allocator global |

## 3. ImGui layout fingerprint (for version matching, Phase 2)

`g_pImGuiContext` is the ImGui context pointer. Observed context-relative
offsets in `Hook_EndScene`:

- `+0x08` ← `0x20` (written as a dword; `ImGuiConfigFlags_NoMouseCursorChange` = 32)
- `+0x0C` ← `|= 6` then `|= 8` (backend flags: mouse cursors + set mouse pos, then renderer flag)
- `+0x20` ← `0`
- `+0x34` ← 22 consecutive `KeyMap[]` defaults (`Tab=9, Left=37, ... Z=90`)
- `+0xBC` ← pointer to `"imgui_impl_win32"`
- `+0xC0` ← pointer to `"imgui_impl_dx9"`
- `+0xE0` ← `hWnd`
- `+0xE8` / `+0xEC` ← `MousePos` (float x/y) in `WndProc_Hook`
- `+0xF0` ← `MouseDown[0]` (`GetAsyncKeyState(1) != 0`)
- `+5392`, `+5904`, `+5920`, `+6576`, `+6700`, `+6804`, `+6812`, `+7160`, `+7232` — context fields used during frame rendering

These offsets are the acceptance test when compiling candidate ImGui versions.
Do not vendor an ImGui version until `offsetof` matches at least `KeyMap`,
`BackendPlatformName`, `ImeWindowHandle`, `MousePos`, `MouseDown`.

## 4. Open naming questions

1. `g_MenuState` byte 3: used as `byte & mask` on an engine object field in
   `Hook_EngineClient_Slot20` (0x10007740). Find the writer of
   `0x1004B057` and the target field before naming it.
2. ImGui version (layout above does not match stock 1.60–1.90 assumptions
   checked by hand; needs compile-time verification).
3. `Hook_EngineClient_Slot20` prototype: decompiler reports `__userpurge` with
   `this` in ESI, which conflicts with a normal VMT `__thiscall`; resolve with
   disassembly before typing the function.
