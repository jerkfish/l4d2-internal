# Phase 0 — Recon evidence log

Target: `original/target.dll` (copy of `VR.dll`)
SHA-256: `fc5625237f287f59a2d9d5cd4b9202e861f1cb46f9a76bf2d14272baafa0397d`
Date: 2026-10-25
Tooling: Python 3.15 + `pefile`, `capstone` (via `uv run --with ...`), `file`,
plus direct reads of the IDA installation and plugin directory.
IDA was **not** used yet (MCP not connected — see end of this file).

Every claim below is reproducible with `tools/pe_recon.py`.

---

## 1. PE headers

| Field | Value | Evidence |
|---|---|---|
| Size | 313,344 bytes | filesystem |
| Machine | `0x014C` IMAGE_FILE_MACHINE_I386 | FILE_HEADER.Machine |
| Magic | `0x010B` PE32 | OPTIONAL_HEADER.Magic |
| Characteristics | `0x2102` (DLL \| 32BIT_MACHINE \| EXECUTABLE_IMAGE) | FILE_HEADER |
| TimeDateStamp | `0x65C63F11` = 2024-02-09 15:04:49 UTC | FILE_HEADER |
| Linker version | 14.38 | OPTIONAL_HEADER |
| OS / subsystem version | 6.0 / 6.0 | OPTIONAL_HEADER |
| Subsystem | 2 (GUI) | OPTIONAL_HEADER |
| DllCharacteristics | `0x0140` DYNAMIC_BASE \| NX_COMPAT | OPTIONAL_HEADER |
| ImageBase | `0x10000000` | OPTIONAL_HEADER |
| SizeOfImage | `0x50000` | OPTIONAL_HEADER |
| AddressOfEntryPoint | RVA `0x3EAB3` (VA `0x1003EAB3`) | OPTIONAL_HEADER |
| NumberOfRvaAndSizes | 16 | OPTIONAL_HEADER |

Sections:

```
.text   VA 0x00001000  vsize 0x0003F6C7  raw 0x00000400  rsize 0x0003F800  flags 0x60000020
.rdata  VA 0x00041000  vsize 0x00008F68  raw 0x0003FC00  rsize 0x00009000  flags 0x40000040
.data   VA 0x0004A000  vsize 0x00001650  raw 0x00048C00  rsize 0x00000600  flags 0xC0000040
.rsrc   VA 0x0004C000  vsize 0x00000580  raw 0x00049200  rsize 0x00000600  flags 0x40000040
.reloc  VA 0x0004D000  vsize 0x00002F04  raw 0x00049800  rsize 0x00003000  flags 0x42000040
```

Data directories:

```
IMPORT      RVA 0x00048FDC  size 0x140
RESOURCE    RVA 0x0004C000  size 0x580
BASERELOC   RVA 0x0004D000  size 0x2F04
DEBUG       RVA 0x000477B0  size 0x70
TLS         RVA 0x00047840  size 0x18
LOAD_CONFIG RVA 0x000476F0  size 0x40
IAT         RVA 0x00041000  size 0x284
```

**No EXPORT directory** (RVA 0 / size 0). This is a hard structural fact: the
DLL exposes no API to `GetProcAddress`. It must be loaded/injected and driven
through `DllMain` and in-process hooks.

Entry-point stub (`0x1003EAB3`, 12 instructions):

```
push ebp
mov  ebp, esp
cmp  dword ptr [ebp+0Ch], 1        ; fdwReason == DLL_PROCESS_ATTACH
jne  short loc_1003EAC1
call 0x1003EEE1                    ; __security_init_cookie (GS)
loc_1003EAC1:
push [ebp+10h] / push [ebp+0Ch] / push [ebp+8]
call 0x1003E97D                    ; __DllMainCRTStartup(hinst, reason, reserved)
add  esp, 0Ch
pop  ebp
retn 0Ch
```

This is the standard MSVC `_DllMainCRTStartup`; the real `DllMain` is supplied
by the CRT startup path and must be resolved in IDA. The call at `0x1003EEE1`
is `__security_init_cookie`: it compares `__security_cookie` (`0x1004A040`)
with `0xBB40E64E`, calls the perf-counter/`GetSystemTimeAsFileTime` helper at
`0x1003EE94` when needed, and writes its complement to `0x1004A080`.

### TLS

TLS directory exists (`0x47840`, `AddressOfCallBacks = 0x100412B4`), but the
callback array contains eight zero DWORDs. **No TLS callbacks execute.** The
directory is likely a default emitted by the CRT; do not model it in the
recovered source.

### Load config / hardening

`SecurityCookie = 0x1004A040`, `GuardFlags = 0x100`
(`IMAGE_GUARD_CF_INSTRUMENTED`). `__security_init_cookie` at entry confirms
`/GS`. CFG presence means `/guard:cf` was used. Exact `GuardCFFunctionTable`
parsing is deferred to IDA; `pefile`'s 32-bit load-config struct size (192)
does not match the directory size (0x40), so treat the secondary fields as
tentative.

### Debug directory

CodeView `RSDS` record present; PDB path:

```
C:\Users\void\Documents\xyz\cheats\l4d2 - UC\compiled\l4d2-x86.pdb
```

Additional debug entries: type 12 (MISC) and type 13 (ILTCG / PDB checksum-ish).
The path is direct evidence of the original project name (`l4d2 - UC`), the
author's user profile (`void`), and the intended architecture (`x86`).

### Resources

Only a version resource:

```
CompanyName      Tsuda Kageyu
FileDescription  MinHook - The Minimalistic API Hook Library for x64/x86
FileVersion      1.3.3.0
InternalName     MinHookD
LegalCopyright   Copyright (C) 2009-2017 Tsuda Kageyu. All rights reserved.
ProductName      MinHook DLL
ProductVersion   1.3.3.0
```

**INFERRED:** the project includes MinHook's `.rc` (or a copy of it); this is
why the main module advertises MinHook. It does **not** imply the whole DLL is
MinHook's own DLL (which would export `MH_*`; this one has no exports), nor
that the main module was built `/MDd` (no `*D.dll` imports).

---

## 2. Rich header / toolchain fingerprint

Manual decode (XOR key `0x019DDB5`; `DanS` marker at file offset `0xF8`):

```
prod id  build   count
0x0109   33130   29     \
0x00FF   33130    1      > VS 2022 17.8 toolset (MSVC 14.38.33130)
0x0102   33130    1     /
0x0105   33030   23     \
0x0104   33030   10      > 14.38 preview objects
0x0103   33030    9      |
0x0101   33030    4     /
0x0101   30795    9      > 14.20-era (VS 2019 16.x) objects
0x009C   30319    2     \
0x009C   21202    2      > VS 2017-era objects
0x0093   30729   16     /  VS 2019 16.0-era objects
0x0097   0        1
0x0001   0      171      (import libraries / non-toolset objects)
```

The optional-header linker version **14.38** matches build 33130 exactly:
**MSVC 14.38.33130, VS 2022 17.8, x86 target**. Mixed older product IDs are
consistent with prebuilt static libraries: MinHook 1.3.3 (released 2017) was
built with an older toolset, and ImGui/Kiero are often built separately.

Runtime evidence from imports:

- Dynamic release CRT: `VCRUNTIME140.dll`, `MSVCP140.dll`, UCRT API sets.
  No `VCRUNTIME140D.dll`, `MSVCP140D.dll`, or `ucrtbased.dll` → not `/MDd`.
  Static CRT (`/MT`) would not import these → **`/MD`**.
- `__CxxFrameHandler3` + `_CxxThrowException` → MSVC C++ EH (`/EHsc` likely).
- `_except_handler4_common` → 32-bit SEH variant used by the CRT.
- `/GS` cookie, `/guard:cf`, `/DYNAMICBASE`, `/NXCOMPAT` confirmed.
- `/DEBUG` confirmed by RSDS record. No embedded PDB.
- Optimization level and `/GL` still need IDA (function shape / cross-function
  inlining). Preliminary expectation: `/O2 /GL` is plausible given the heavy
  inlining and small import surface, but **do not record as fact yet**.

Installed toolchain on this machine: VS 2022 Community with **14.44.35207**
only. A byte-close rebuild wants 14.38.33130 (VS 17.8 toolset).

---

## 3. Imports (full)

```
KERNEL32.dll (49)
  GetCurrentProcessId, MultiByteToWideChar, GlobalAlloc, GlobalFree,
  GlobalLock, GlobalUnlock, QueryPerformanceFrequency, QueryPerformanceCounter,
  GetProcAddress, GetModuleHandleW, VirtualFree, VirtualAlloc, VirtualQuery,
  HeapCreate, VirtualProtect, HeapFree, GetCurrentProcess, Thread32Next,
  Thread32First, GetCurrentThreadId, SuspendThread, ResumeThread,
  CreateToolhelp32Snapshot, DisableThreadLibraryCalls, HeapReAlloc,
  CloseHandle, HeapAlloc, GetThreadContext, FlushInstructionCache,
  SetThreadContext, OpenThread, K32GetModuleInformation,
  WritePrivateProfileStringA, CreateDirectoryA, GetPrivateProfileStringA,
  InitializeSListHead, GetSystemTimeAsFileTime, IsDebuggerPresent,
  IsProcessorFeaturePresent, TerminateProcess, SetUnhandledExceptionFilter,
  UnhandledExceptionFilter, SleepConditionVariableSRW, WakeAllConditionVariable,
  AcquireSRWLockExclusive, ReleaseSRWLockExclusive, CreateThread,
  GetModuleHandleA, Sleep
USER32.dll (30)
  GetAsyncKeyState, FindWindowA, DestroyWindow, DefWindowProcA,
  CreateWindowExA, UnregisterClassA, RegisterClassExA, LoadCursorA,
  ScreenToClient, GetCapture, ClientToScreen, IsChild, GetForegroundWindow,
  SetCapture, SetCursor, EmptyClipboard, GetClipboardData, ReleaseCapture,
  SetCursorPos, OpenClipboard, CloseClipboard, SetClipboardData,
  GetWindowThreadProcessId, GetKeyState, GetCursorPos, EnumWindows,
  MessageBoxA, CallWindowProcA, SetWindowLongA, GetClientRect
SHELL32.dll (1)
  SHGetFolderPathA
MSVCP140.dll (3)
  _Query_perf_counter, ?_Xlength_error@std@@YAXPBD@Z, _Query_perf_frequency
IMM32.dll (3)
  ImmGetContext, ImmReleaseContext, ImmSetCompositionWindow
XINPUT1_3.dll (2)
  ordinal #2, ordinal #4
VCRUNTIME140.dll (14)
  __current_exception_context, _CxxThrowException, memset,
  _except_handler4_common, __current_exception, strstr,
  __std_exception_copy, __std_exception_destroy, __std_terminate,
  __CxxFrameHandler3, __std_type_info_destroy_list, memchr, memcpy, memmove
api-ms-win-crt-stdio   (12): __acrt_iob_func, __stdio_common_vfprintf,
  __stdio_common_vsprintf, __stdio_common_vsprintf_s, ftell,
  __stdio_common_vsscanf, fread, _wfopen, fwrite, fclose, fseek, fflush
api-ms-win-crt-heap    (4): free, malloc, calloc, _callnewh
api-ms-win-crt-utility (3): srand, rand, qsort
api-ms-win-crt-time    (1): _time64
api-ms-win-crt-runtime (12): _register_onexit_function, _execute_onexit_table,
  _crt_atexit, _cexit, terminate, _initterm, _initterm_e, _seh_filter_dll,
  _initialize_onexit_table, _configure_narrow_argv,
  _initialize_narrow_environment, _invalid_parameter_noinfo_noreturn
api-ms-win-crt-string  (1): _stricmp
api-ms-win-crt-convert (1): atof
api-ms-win-crt-math    (10): _libm_sse2_sqrt_precise, floor,
  _libm_sse2_sin_precise, _libm_sse2_cos_precise, _libm_sse2_atan_precise,
  _libm_sse2_acos_precise, _CIatan2, ceil, roundf, _fdclass
```

Interpretation:

- Thread enumeration + context APIs → MinHook's `EnableHook`/`DisableHook`
  thread-freeze logic, not user cheat logic.
- Heap*/Virtual* and `K32GetModuleInformation` → MinHook trampoline allocator
  and module bounds.
- INI APIs + `CreateDirectoryA` → cheat config (`C:\L4D2space\*.ini`).
- `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos` → hotkeys and aimbot input.
- `IsDebuggerPresent` → likely MinHook or a trivial check; verify xrefs.
- `SHGetFolderPathA` → likely path for `imgui.ini`/`imgui_log.txt`.
- XInput ordinals → controller/trigger support in the cheat UI or aim code.
- ImGui backends are loaded dynamically by name (`d3d9.dll`, `d3d10.dll`,
  `d3d11.dll`, `d3d12.dll`, `opengl32.dll`, `vulcan-1.dll` strings), so they do
  not appear in the import table.

---

## 4. Strings / module identity

### 4.1 Engine and game evidence

- Netprops: `DT_BasePlayer`, `DT_BaseEntity`, `DT_TerrorPlayer`,
  `DT_BaseCombatCharacter`, `DT_BaseAnimating`, `DT_Witch`,
  `DT_BaseCombatWeapon`, `DT_WeaponSpawn`, `DT_Infected`,
  `m_usSolidFlags`, `m_iHealth`, `m_iMaxHealth`, `m_vecViewOffset[0]`,
  `m_fFlags`, `m_iTeamNum`, `m_flSimulationTime`, `m_vecPunchAngle`,
  `m_nTickBase`, `m_hActiveWeapon`, `m_isGhost`, `m_isIncapacitated`,
  `m_nSequence`, `m_rage`, `m_flNextPrimaryAttack`, `m_lifeState`,
  `m_vecVelocity[0]`, `m_flFallVelocity`, `m_isHangingFromTongue`, `m_iClip1`,
  `m_weaponID`, `m_vecMins`, `m_vecMaxs`, `m_vecOrigin`, `m_mobRush`.
- Engine interfaces (string table around `.rdata` 0x45268):
  `CreateInterface`, `PlayerInfoManager002`, `VClient016`,
  `VEngineCvar007`, `VEngineClient013`, `VClientEntityList003`,
  `VModelInfoClient004`, `VEngineModel016`, `VMaterialSystem080`,
  `VGUI_Panel009`, `VGUI_Surface031`, `EngineTraceClient003`,
  `VDebugOverlay003`.
- Module names: `client.dll`, `server.dll`, `engine.dll`, `vstdlib.dll`,
  `vguimatsurface.dll`, `vgui2.dll`, `materialsystem.dll`.
- Console commands/vars: `cl_updaterate`, `cl_interp_ratio`, `cl_interp`,
  `SharedRandomFloat`, `fog_override`, `fog_enable`, `mat_fullbright`,
  `deadflag`, `z_view_distance`, `z_gun_vertical_punch`, `cl_viewbob`,
  `cl_viewmodelfovsurvivor`, `fov_desired`, `r_aspectratio`,
  `sv_showdebugtracers`, `say : `, `setinfo name "%s%c"`.
- Model/texture paths: `models/weapons/arms/v_arms`, `models/v_models`,
  `models/w_models/weapons`, `models/infected`,
  `particle/screenspaceboomervomit`, `debug/debugwireframe`,
  `debug/debugambientcube`.
- Weapon names: CS Base, Assault Rifle, Auto Shotgun, Boomer Claw, Chainsaw,
  Charger Claw, Cola Bottles, Firework Crate, First Aid Kit, Gas Can, Gnome,
  Grenade Launcher, Hunter Claw, Adrenaline, Ammo Pack, Defibrillator,
  Explosive/Incendiary Upgrade Pack, Vomit Jar, Jockey Claw, Molotov,
  Oxygen Tank, Pain Pills, Pipe Bomb, Pistol, Magnum Pistol, Propane Tank,
  Pump Shotgun, AK-47, Desert, SG552, Chrome Shotgun, SPAS,
  Submachine Silenced, Smoker Claw, Sniper Rifle, Military Sniper, Scout,
  Spitter Claw, Submachine Gun, Tank Claw.
- Entity display names: Tank, Witch, Smoker, Jockey, Charger, Spitter, Hunter,
  `zombie - RUSH`, `zombie`, `[BOT]`, `Witch - Rage: %.2f`, `DANGER!`.

### 4.2 Cheat UI / features (evidence of scope, not to be reimplemented)

Menu tabs/features: MainMenu, Aimbot, Triggerbot, Silent Aim, Perfect Silent
Aim, Player ESP (name/box/distance/health/healthbar/weapons/ammo/witch rage
bar/lines/world), Chams (player/hand/XQZ, wireframe), FOV changer, aspect
ratio, no visual recoil, bunny hop, speedhack, air stuck, teleport, fake lag,
chat spam/advertise/jammer, remove vomit/fog/fullbright, invisible jockey,
lag exploit, name stealer, server lagger, no fall damage, auto pistol,
instant weapon switch/revive, anti smoker, anti-aim (pitch/fake yaw),
thirdperson, indicators, radar.

Config paths and keys:

```
C:\L4D2space\
C:\L4D2space\legit.ini
C:\L4D2space\rage.ini
C:\L4D2space\hvh.ini
imgui.ini
imgui_log.txt
ENABLEAIMLEGIT, LegitAimKey, LegitAimHitbox, LegitAimHitboxMode,
LegitShotgunMode, LEGITNORECOIL, LEGITNOSPREAD, LEGITAIMFOV,
LEGITSMOOTHYAW, LegitLagComp, LegitIGNOREWITCH, LegitIgnoreTeam,
LegitbHitscanMode, Legitpriorityspecial, LegitAutoShove,
LegitAutoShoveFov, TriggKey, trighead, triggerbody, AimFov, AimKey,
Smooth, Silent, PSilent, AutoFire, MagicBullets, NoRecoil, NoSpread,
AdvancedHitscan, PredictionFix, IgnoreTeam, IgnoreWitch,
IgnoreLagExploit, IgnoreUninterpolated, bHitscanMode, AutoShove,
AutoMelee, priorityspecial, LagComp, ShotgunMode, AimHitbox,
FovVisualize, EspToggle, EspShowTeam, EspDanger, EspName, EspBox,
EspHealth, ESPHEALTH2, ESPWEAPON, ESPWEAPONSPAWN, ESPWEAPONSAMMO,
ESPWITCHRAGEBAR, NOFOG, radarnames, EspChams, CHAMSMATERIAL,
CHAMSMATERIALHAND, NoVisRecoil, AspectRatio, AspectRatioValue,
FovChanger, FovChangerValue, ViewFovChangerValue, HealthBarESP,
NoVomit, ColorPickerESP, ColorPickerChams, ThirdpersonKey, AutoPistol,
AutoStrafer, AccuracySpeed, ChatspamKey, TeleportKey, AirstuckKey,
chamscolor, chamscolor1, chamscolor2, handchamscolor, handchamscolor1,
handchamscolor2, ChamsINFECTED, ChamsSURVIVOR, Handwpnchams,
wpnchams, ChamsHands, CHAMSXQZ, radar, AUTOBH, Chatspam,
ChatspamMode, SpeedhackVal, SpeedhackKey, LAGEXPLOIT, LagFast,
LagKey, Teleport, Airstuck, EnginePred, antismoker, NoFallDamage,
FakeLag, FakeLagValue, serverlag, serverlagkey, AntiAim, Pitch,
FakeYaw, indicators
```

Anti-cheat and branding:

```
SMAC anti-cheat is installed!
No anti-cheat detected!
SMAC MODE IS DISABLED!
SMAC Mode
[www.unknowncheats.me] @space785
```

Overlay/library strings:

```
Kiero
Kiero DirectX Window
imgui_impl_dx9
imgui_impl_win32
imgui.ini / imgui_log.txt
Arial, Tahoma, Visitor TT2 BRK, ProggyClean.ttf
C:\Windows\Fonts\Courbd.ttf
```

### 4.3 RTTI

Only four standard library type descriptors plus one user RTTI name:

```
.?AVbad_alloc@std@@
.?AVexception@std@@
.?AVbad_array_new_length@std@@
.?AVtype_info@@
.?AUtracefitlers@@          <-- user-defined struct/class
```

**INFERRED:** `tracefitlers` is a user-defined type name (note the typo:
"tracefitlers", not "tracefilters"). It is the only RTTI evidence of a named
user class; capture it verbatim and list it in the inference log. There is
very little RTTI overall (`/GR` may still be on for exceptions only, or most
classes are non-polymorphic).

---

## 5. What this means for the plan

1. **Phase 1 (exports) is empty.** There is nothing to reconstruct at the ABI
   boundary. The public surface is `DllMain` plus created thread(s) and engine
   vtables obtained via `CreateInterface`.
2. **The bulk of the binary is MinHook + ImGui (+ font data) + engine SDK.**
   FLIRT/pattern matching must remove those before any "user function" count.
   The `.text` is ~256 KiB, but the user-authored fraction is likely far
   smaller than the raw function count suggests.
3. **Verification needs a host.** No exports → the harness must inject into
   L4D2 or emulate the interfaces. Behavioral comparison of aimbot/ESP logic
   against a live game is impractical in CI; the synthetic host should focus on
   pure functions (config parser, math, menu state, netprop offsets, hook
   install/uninstall) and structural diffing for the rest.
4. **x86 ABI, not x64.** Function prototypes need 32-bit conventions; IDA's
   default x86 types apply. The "hidden first parameter for >8-byte returns"
   x64 rule is irrelevant; 32-bit MSVC instead returns small structs in
   EAX:EDX and returns larger ones via hidden pointer with `ret 4`
   (callee-pops for `thiscall`/`stdcall` variants). This must be handled when
   reconstructing prototypes.
5. **The binary likely uses /O2 and maybe /GL**, so function-boundary 1:1
   matching may be poor; judge matches at function-group level as the brief
   allows.

---

## 6. IDA / MCP status and stop condition

Installed:

- `C:\Program Files\IDA Professional 9.0\ida.exe`, `idat.exe`, `idalib/`
- `C:\Users\Ahmad\AppData\Roaming\Hex-Rays\IDA Pro\plugins\mcp-plugin.py`
  (75 KiB; starts `http.server` on `localhost:13337`; requires Python ≥ 3.11
  in IDA's Python)
- Decompiler plugin `hexx64.dll` contains the strings `HEXCX86` and `HEXCX64`
  (IDA 9 appears to package x86 and x64 decompilation in that module), and the
  machine has licenses; live confirmation still needed.
- No IDA process is running (`tasklist` showed none).
- No `ida_*` tools are exposed to this agent session, so the MCP server is not
  connected/registered on the Cherry Studio side.

The brief says: "Confirm both are available over MCP before starting; if not,
stop and tell me." Because there is no MCP connection (and because the target
is 32-bit, not the stated x64), **work is stopped here** pending user action.
