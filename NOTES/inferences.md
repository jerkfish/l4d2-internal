# Inference log

Format: `INF-NNN` — claim, confidence, evidence, how to confirm. Every inferred
name/type in recovered source must carry `// INFERRED: <evidence>` and be
listed here. Nothing in this file is confirmed fact unless marked otherwise.

## INF-001 — Module is an injected Left 4 Dead 2 cheat DLL

- **Confidence:** high (behavioral/identity), low for exact feature semantics.
- **Evidence:** L4D2 netprop names (`DT_TerrorPlayer`, `m_iHealth`,
  `m_nTickBase`, `DT_Witch`, ...); Source engine interface strings
  (`VClient016`, `VEngineClient013`, ...); cheat menu strings (Aimbot, ESP,
  Chams, SpeedHack, ...); config under `C:\L4D2space\`; branding
  `[www.unknowncheats.me] @space785`; PDB path
  `...\cheats\l4d2 - UC\compiled\l4d2-x86.pdb`; zero exports.
- **Impact:** the DLL is injection-driven; there is no exported API to
  reconstruct. Verification needs a host.
- **Confirm:** IDA xrefs from `DllMain`/CreateInterface calls; optional
  comparison with the public L4D2 SDK headers (do not copy leaked SDK code).

## INF-002 — `tracefitlers` is a user-defined type name (typo preserved)

- **Confidence:** high for the name; low for semantics.
- **Evidence:** RTTI type descriptor `.?AUtracefitlers@@` at file offset
  `0x49164`; only non-std RTTI name in the binary.
- **Impact:** likely a trace/ray-filter struct used by aimbot/hitscan code.
  Preserve the misspelling verbatim in the recovered source.
- **Confirm:** find the vtable/type_info xrefs in IDA; inspect the class layout.

## INF-003 — MinHook 1.3.3 is statically linked; version resource came from
MinHook's `.rc`

- **Confidence:** high for MinHook; medium for the `.rc` mechanism.
- **Evidence:** MinHook 1.3.3.0 VS_VERSIONINFO; MinHook's characteristic
  imports (thread snapshot/suspend/context, `K32GetModuleInformation`,
  `VirtualProtect`, heap allocator); no `MH_*` exports, so this is not
  MinHook's own DLL. Static libraries do not carry `.rsrc`, so the project
  likely compiles MinHook sources and includes its `.rc` (or a copy).
- **Impact:** exclude MinHook code from user-function recovery; use the
  upstream MinHook 1.3.3 source as the replacement. Do not invent a
  `MH_Initialize` API because the binary doesn't export one.
- **Confirm:** FLIRT/pattern match MinHook functions; check whether the
  MinHook source files' code and `.rdata` strings match.

## INF-004 — Release `/MD`, `/O2`, `/GS`, `/guard:cf`, `/DEBUG`

- **Confidence:** high for `/MD`, `/GS`, CFG, `/DEBUG`; medium for `/O2`.
- **Evidence:** imports `VCRUNTIME140`/`MSVCP140`/UCRT with no `*D` variants;
  SecurityCookie + `__security_init_cookie`; GuardFlags `0x100`; RSDS debug
  directory; dense inlined code and small import surface suggest optimized
  release. `/GL`/LTCG not yet proven.
- **Impact:** build with the matched release toolchain; exact per-function
  matching may be affected by LTCG.
- **Confirm:** IDA function-group inspection, `/GL`-style cross-function
  inlining, COMDAT folding patterns, and the final map file comparison.

## INF-005 — The UTF-16 module name is `engine.dll`

- **Confidence:** high.
- **Evidence:** bytes at `0x45276`–`0x4528D`: the ASCII string
  `CreateInterface\0` overlaps the first UTF-16 code unit, so the byte-level
  extractor initially printed `eengine.dll`. Re-aligning to `0x45278` yields
  `engine.dll`.
- **Impact:** use `engine.dll` in the recovered module-name logic.
- **Confirm:** xref to the UTF-16 literal in IDA.

## INF-006 — `DllMain` is CRT-registered, not the entry point

- **Confidence:** high.
- **Evidence:** entry `0x1003EAB3` is the standard `_DllMainCRTStartup`; it
  calls `__security_init_cookie` only on `DLL_PROCESS_ATTACH`, then
  `__DllMainCRTStartup(0x1003E97D)`.
- **Impact:** locate the real `DllMain` through the CRT startup's function
  argument, not by treating the entry point as `DllMain`.
- **Confirm:** inspect `0x1003E97D` and the CRT startup data in IDA.

## INF-007 — `Kiero` is the D3D hook/window bootstrap

- **Confidence:** medium.
- **Evidence:** strings `Kiero`, `Kiero DirectX Window`, `d3d9.dll`,
  `Direct3DCreate9`, `d3d10/11/12.dll`, `opengl32.dll`, `vulcan-1.dll`,
  `imgui_impl_win32`, `imgui_impl_dx9`.
- **Impact:** treat as third-party library code; recover the call sites and
  configuration, not Kiero internals. Note `vulcan-1.dll` is misspelled in the
  binary (the real library is `vulkan-1.dll`); preserve the string.
- **Confirm:** FLIRT/pattern match Kiero; inspect the overlay init path.

## INF-009 — Built with `/GL` whole-program optimization + `/LTCG`

- **Confidence:** high.
- **Evidence:** (a) `ImGui_ImplWin32_Init` / `ImGui_ImplDX9_Init` bodies
  (separate TUs) are inlined into `Hook_EndScene`; (b) `MH_CreateHook` exists
  as a specialized clone with the detour constant-propagated to `Hook_EndScene`
  and the trampoline stored directly into `g_pfnEndScene_Orig`
  (`0x1004A98C`); (c) MinHook's `MH_EnableHook`/`MH_DisableHook` are merged
  into one internal function taking an `enable` flag; (d) Kiero's
  `kiero::init` receives its argument in ECX — an LTCG-chosen convention for
  a non-exported function.
- **Impact:** the rebuild must use `/GL` on every TU and `/LTCG` at link or
  codegen will diverge substantially. Function cloning/merging is expected;
  compare with that in mind.
- **Confirm:** rebuild with `/GL` and check the same functions reproduce.

## INF-010 — ImGui v1.74 (master) with stock DX9/Win32 backends

- **Confidence:** very high.
- **Evidence:** compile-time layout probe against 38 upstream tags:
  `offsetof(ImGuiContext, IO) == 8`, `ImGuiIO.KeyMap == 44`,
  `BackendPlatformName == 180`, `ImeWindowHandle == 216`, `MousePos == 224`;
  `ImGuiContext.Style == 5392`, `.CurrentWindow == 6576`,
  `.DrawData == 7160`; `ImGuiWindow.WriteAccessed == 124`. Only 1.74 matches
  all at once. Backend init source matches line-for-line (QPC, KeyMap VK
  table, backend names, flags). Full table: `NOTES/ida-imgui-version.md`.
- **Impact:** vendor ImGui 1.74; exclude its ~234 functions from recovery.

## INF-011 — Kiero 1.2.9 compiled with `KIERO_INCLUDE_D3D9=1` and
`KIERO_USE_MINHOOK=1`

- **Confidence:** high.
- **Evidence:** binary strings `Kiero`, `Kiero DirectX Window` and the
  misspelled `vulcan-1.dll` (Kiero 1.2.9 and older; fixed in 1.2.10); the
  decompiled `kiero::init` matches 1.2.9's D3D9 branch exactly
  (`Direct3DCreate9(32)`, `GetAdapterDisplayMode`, D3DPRESENT_PARAMETERS,
  `CreateDevice(..., 288, ...)`, `calloc(119, 4)`, `memcpy(0x1DC)`),
  followed by an inlined `MH_Initialize` (lock + `HeapCreate`) which only
  compiles when `KIERO_USE_MINHOOK=1`.
- **Impact:** vendor kiero.cpp/h at commit `9f41abe` (v1.2.9) with those two
  macros set to 1; D3D9 is the only renderer actually compiled.

## INF-008 — Config files are read/written via the Win32 profile API

- **Confidence:** high.
- **Evidence:** `GetPrivateProfileStringA`, `WritePrivateProfileStringA`,
  `CreateDirectoryA`, paths `C:\L4D2space\legit.ini`, `rage.ini`, `hvh.ini`,
  and a large list of key names.
- **Impact:** config parsing is pure enough to unit-test without the game; a
  high-value early recovery target.
- **Confirm:** xrefs to the path literals and to the INI APIs.

## INF-012 — ESP painter (sub_1003C410) helper semantics and ABI

- **Confidence:** high for the drawing math/flags (call sites disassembled);
  medium for the virtual-interface slot meanings.
- **Evidence:** sub_1003C410 call sites, sub_1003B030/1003D7D0/1003D870/
  1003DA50/1003AA60/1003AB50/100014B0 disassembly; menu strings at
  0x10041A7C ("%s") and 0x10047254/0x10047278 ("health : %d", "zombie",
  "Witch - Rage: %.2f", "[BOT]", "DANGER!"); config key block 0x457A0..
  0x45818; float constants 0x1004733C (1.3), 0x100473CC (57.295776),
  0x100473C4 (45.0), 0x100473F4 (180.0), 0x10047450 (-180.0),
  0x10047408 (360.0), 0x100472CC (0.017453292), 0x100472D0 (0.0254).
- **Details:** entity+4 vtable slot 8 is probed and 0 skips the entity
  (renderable/model-ish; exact virtual unnamed). entity+8 vtable slot 7 is
  IsDormant (already proven by sub_10001EC0). GetAbsOrigin is entity+0
  vtable slot 11. IVDebugOverlay::ScreenPosition is slot 12 (offset 48);
  IVEngineClient::GetScreenSize is slot 5 (offset 20); GetPlayerInfo is
  slot 8 (offset 32); player_info_t::name is at struct offset 8 (the target
  passes &info+8 to the text helper). IClientEntityList slot 4
  (offset 16) resolves m_hActiveWeapon handles (GetClientEntityFromHandle),
  while slot 3 (offset 12) resolves entity indexes.
- **Deviations:** the target passes the player name buffer as the
  printf-style format string in the cat5-team2 and cat3 name draws (a
  format-string bug); we use "%s" + the name argument. We also force a NUL
  at name[127] and guard the health-bar divide when maxHealth <= 0. These
  avoid undefined reads without changing normal-name rendering.
- **Impact:** ESP output depends on ScreenPosition being called with this =
  g_pDebugOverlay (VDebugOverlay003) and on the VGUI surface slot numbers
  11/12/14/15 for color/filled/outlined/line; all were verified in the
  previous overlay work.

## INF-013 — player_info_t write size and ESP crash diagnostics

- **Confidence:** high for the struct size; medium for the crash cause.
- **Evidence:** Source 2013 player_info_t = xuid (8) + name[128] + userID (4)
  + guid[33] + padding + friendsID (4) + friendsName[128] + fakeplayer/ishltv
  + customFiles[16] + filesDownloaded, ~332 bytes. The target's ESP reserves
  the player_info block at ebp-0x2D8 and the next distinct local sits at
  ebp-0x164 (0x174 gap). ESP build 0R used a 256-byte buffer and
  GetPlayerInfo therefore overwrote ~76 bytes of DrawEsp's frame per entity;
  the live log shows ESP running normally for a long session and then the
  process dying without a fault record.
- **Fix:** buffer enlarged to 512 bytes and zeroed per entity; name[127]
  forced to NUL.
- **Diagnostics:** TEMP vectored exception handler (0xC0000005/1D/94/FD/
  409/374) logs CRASH code/address/module/offset/tid, AV direction/target,
  registers, and EspGetDebugContext breadcrumbs (phase/index/team/category/
  entity/weapon/bbox). IsBadReadPtr probe AVs have a faulting instruction in
  kernel32/kernelbase/ntdll and are skipped. Remove this handler before the
  final deliverable.
- **Impact:** if the crash persists, the log's CRASH/`esp ctx` lines identify
  whether the fault is in VR.dll (which phase) or a game/OS module.

## INF-014 — sub_1003B030 reduces with minss/maxss, not comparisons

- **Confidence:** high (instruction-level evidence).
- **Evidence:** 0x1003B5xx..0x1003B7xx contains alternating load/minss and
  load/maxss sequences. Order: first out0 component with itself
  (`minss xmm,xmm`), then new values with the running result as the source
  operand: out4, out1, out5, out2, out6, out3, out7 (v78, v75, v72, v69,
  v66, v63, v60 for X; v79, v76, v73, v70, v67, v64, v61 for Y). The eight
  ScreenPosition calls use points top3, top0, top2, bottom2, bottom1,
  bottom0, top1, bottom3 with output vectors zero-initialized first.
- **Consequences:** comparison-based min/max (`if (v < min) min = v`) does
  not match when a projected corner is NaN: the target's minss/maxss chain
  can still produce a finite bbox, while comparisons poison the result and
  `(int)NaN` becomes INT_MIN. This moved ESP labels/boxes and could feed
  invalid coordinates to the VGUI renderer.
- **Implementation:** src/esp.cpp `SseMin8`/`SseMax8` use `_mm_min_ss` /
  `_mm_max_ss` with the target's operand order; BuildBoundingBox calls
  ScreenPosition in the target order and reduces with those helpers
  (generated code verified to contain 32 minss/maxss).
- **Impact:** bboxes now match the target for NaN/behind-camera projections;
  distance labels use the same bbox as box/name drawing.

## INF-015 — ESP extreme-coordinate guard (deviation from target)

- **Confidence:** high for the crash mechanism; the guard itself is a deliberate
  deviation from the target's code.
- **Evidence:** live crash 2026-10-26: phase=distance, bbox contains
  INT_MIN/huge values, KERNEL32+0x145EA AV storm (EAX=probe target,
  ESI=0x1000 => IsBadReadPtr page probes), then CRASH n=1
  code=C0000005 addr=0/eip=0 and engine.dll TerminateProcess. ScreenPosition
  writes NaN/inf-derived coordinates for near-plane/behind corners, and
  cvttss2si converts them to INT_MIN.
- **Target behaviour:** sub_1003C410 stores the visibility byte from
  sub_1003B030 at ebp-0x2FD but never reads it; the target therefore also
  forwards these extreme positions to VGUI. We do not reproduce that unsafe
  path.
- **Guard:** IsBboxSane requires all four coords in [-32767,32767] and
  minX<=maxX/minY<=maxY; rejected entities are skipped (counted in `bad=`).
  OverlayTextV rejects font<=0 and |x|,|y|,centered drawX > 32767;
  DrawLineToCenter rejects extreme screen coords/implausible screen sizes.
- **Impact:** only invisible/off-screen garbage projections are skipped;
  finite on-screen ESP output is unchanged. Remove/tune only if a
  verification explicitly requires the target's unsafe behaviour.

## INF-016 — VGUI_Surface031::DrawPrintText takes 3 stack args

- **Confidence:** very high (target assembly + ABI).
- **Evidence:** target sub_1000C390 at 0x1000C65E:
  `push 0` (drawType), `push [ebp-0x128]` (text length), `push edx`
  (wide text), `mov eax,[eax+0x58]`, `call eax`; no `add esp` afterwards,
  so the callee is __thiscall callee-clean with three stack arguments.
  GetTextSize (slot 0x120) in the same helper uses four args
  (font, text, &width, &height).
- **Bug:** our helper declared four args (text, len, 0, textHeight). The
  callee popped only 12 of the 16 pushed bytes, leaving ESP 4 bytes low.
  OverlayTextV's epilogue pops esi/ebx before `mov esp,ebp`, so the saved
  callee-saved registers were restored from the wrong slots (EBX=0x5F in the
  live crash), and the next indirect call jumped to 0x5F.
- **Fix:** `DrawPrintTextFn` = `void(__thiscall*)(void*, const wchar_t*,
  int, int)`; call `(surface, wideText, wideCount-1, 0)`.
- **Impact:** any text drawing (name, distance, health, weapon, tags) was
  corrupting the caller; boxes/lines were unaffected. This is the root cause
  of the name/distance crashes.

## INF-017 — Radar math/ABI and screen-rect source

- **Confidence:** high for the math/colors/labels (instruction-level); medium
  for the screen-rect equivalence.
- **Evidence:** sub_1003BFB0 / sub_1003B820 / sub_1003C1D0 disassembly.
  Constants: 0.5 (0x100472FC), deg2rad double 0.017453293005625408
  (0x10047330), 110.0 (0x100473E8), +/-3300.0 (0x10047428/0x10047458),
  sign/abs masks 0x100475E0/0x100475C0.
  C1D0 ABI: the label x is the function's saved EDI; every B820 call site
  loads EDI with viewangles.z (roll) before the call, so labels draw at
  rollBits+2 (x=2 for the usual roll=0), y=dotY+4. Dot colors come from
  ECX (red), EDX (green), and the pushed third stack value (blue):
  You (255,255,255), Default (255,123,0, empty string global "Default"),
  Tank (255,0,0), Witch rage==1.0 -> (255,0,0) else (16,252,8),
  Smoker/Jockey/Charger/Spitter/Hunter (255,0,0).
- **Screen rect:** the target's Hook_EndScene stores
  ImGuiContext.CurrentWindow->Pos/Size (ImGuiWindow offsets 0xC..0x18)
  into dword_1004A4B4..C0 and copies left/top into dword_1004AC28/2C.
  sub_1003BFB0/C1D0 center the radar on that rect. Our menu is a 600x650
  window rather than the target's always-present ImGui window, so
  src/radar.cpp uses GetClientRect(g_hGameWindow)+ClientToScreen instead
  (INFERRED equivalent for a centered overlay). Dragging is PENDING.
- **Impact:** radar is expected centered on the game window; if the original
  appears offset, compare against the target's ImGui-window rect.

## INF-018 — World weapon-spawn ESP and radar drag analysis

- **Confidence:** high (sub_1003DB30 disassembly/decompilation).
- **Weapon-spawn ESP:** sub_1003DB30 takes the local entity in ECX, loops
  entities, skips local and dormant (entity+8 slot 7), calls sub_1003B030
  with the local entity, and draws ONLY when the returned visibility byte is
  true (all eight projected corners on-screen). Centre = ((minX+maxX)/2,
  (minY+maxY)/2). It requires ClientClass+0x14 == 260 (CWeaponSpawn) and
  reads DT_WeaponSpawn::m_weaponID; the same ID->name table as sub_1003AB50
  is used. Because the switch is guarded by `if (weaponID)`, ID 0 and
  unknown IDs produce "Unknown Weapon" (the case 0 "CS Base" arm is dead).
  Label is white, centred, Arial, at the box centre.
- **Radar drag:** sub_1003BFB0 updates dword_1004AC28/2C from the mouse
  delta while the left button is held inside a 292x292 rect derived from
  those globals, but the frame is drawn at v8/v9 computed from the window
  rect (dword_1004A4B4..C0) every call. The drag therefore has no visible
  effect in the target. The mouse state at byte_1004AA44 /
  dword_1004AA3C/AA40 is refreshed by sub_10037170 only while the menu is
  open. Our static centred radar matches the compiled target.

## INF-019 — Thirdperson mechanism and dead speedometer

- **Confidence:** high (Cheat_Update disassembly); medium for the visual
  result of z_view_distance=-100 (needs live comparison).
- **Thirdperson:** while byte_1004ABF2 is set, Cheat_Update resolves
  DT_BasePlayer::deadflag (a nested DT_PlayerState prop) and writes zero to
  localEntity+deadflag+4 and +8. The source globals dword_1004B138/B13C are
  in .data BSS (no writers), so the written value is always 0. It then sets
  z_view_distance=-100 (or, with ThirdpersonKey dword_1004ABF4, toggles it
  -100/0 on key press through byte_1004B1EC). With the option off it sets
  z_view_distance=0 only; the deadflag dwords are never restored. The menu
  binds "Thirdperson" to byte_1004ABF2 and a keybind widget to
  dword_1004ABF4 (key config name ThirdpersonKey).
- **Safety:** our port requires 0x100 < deadflagOffset < 0x4000 before the
  two writes (the target has no guard). The resolved offset is logged once as
  `thirdperson deadflag=%d`.
- **Speedometer:** PaintTraverse reads dword_1004AFF4 as a float and draws
  the rounded speed with Tahoma24 if non-zero. A binary-wide byte scan shows
  only that one reference to the address; no writer exists, and
  sub_1003A300 does not initialize it. The original never draws it; we do not
  port the block (equivalence).

## INF-020 — Teleport and Air Stuck (NaN movement exploit)

- **Confidence:** high (Cheat_Update machine code 0x10005C8E..0x10005CFE).
- **Teleport:** gated by the low byte byte_1004AC1C. If dword_1004ABAC
  (TeleportKey) is zero or currently held, the target stores the raw dword
  0x7FC00000 (quiet NaN) at cmd+0x18 and cmd+0x1C (forwardmove and sidemove).
  The engine's movement code consumes the NaN, producing the prediction-error
  "teleport". AutoStrafer runs later (0x10005E20) and can overwrite the NaN
  when its own conditions hold, so our block is kept immediately before it.
- **Air Stuck:** gated by byte_1004AC1D (HIBYTE of word_1004AC1C). The check
  is `(sub_100015A0(local) & 1) == 0`, i.e. !FL_ONGROUND; then, if
  dword_1004ABA8 (AirStuckKey) is zero or held, only cmd+0x18 (forwardmove)
  is set to 0x7FC00000. sidemove is untouched (target quirk).
- **ABI correction:** the pseudocode prints `sub_100015A0(v137)` with a
  stack-style argument, but the machine code at 0x10005CCE does
  `mov ecx, esi; call 100015A0`, where ESI is the local entity pointer.
  sub_100015A0 is therefore a __thiscall flags getter; our
  GetEntityFlags(local) call is the correct port. The decompiler's
  `v137 = 0/1` lines belong to an unrelated No-Visual-Recoil temp and must
  not be used for this condition.
- **Menu/config:** Misc tab order is Bunny Hop, SpeedHack, Air Stuck +
  keybind, Teleport + keybind, Fake Lag. Key config names are TeleportKey
  and AirstuckKey (verified in the target string table). SpeedHack and
  Fake Lag are bound to their real flags (byte_1004B024 / byte_1004AC0E) but
  their slot-20 logic remains PENDING, so toggling them currently does
  nothing.
- **Config:** Menu_Render 0x10037B00 is not the per-frame menu painter; it is
  the settings-descriptor builder, called once from sub_10001050 with a fresh
  0x30-byte descriptor (three std::vector lists: bool, int, float). The same
  descriptor is consumed by SaveSettings 0x10039AD0 and LoadSettings
  0x10039F20, so every inserted key is both saved and loaded. The Misc keys
  are "TeleportKey", "AirstuckKey", "Teleport", "Airstuck" (note: no space),
  "SpeedHack", "SpeedhackVal", "SpeedhackKey", "FakeLag", "FakeLagValue".
  The per-frame widget code lives in Hook_EndScene.
- **Expected live behaviour:** Teleport with no keybind continuously applies
  NaN movement (prediction-error jumps); with a keybind it applies only while
  held. Air Stuck with no keybind freezes horizontal movement whenever the
  player is airborne; with a keybind only while held. Jumping first is
  required for Air Stuck because of the FL_ONGROUND gate.
- **Slot-20 call site:** the hook at 0x100076A0 is on
  IVEngineClient::SetViewAngles (slot 19 is GetViewAngles). CInput::CreateMove
  keeps the CUserCmd in ESI and passes &cmd->viewangles as the stack
  argument. The target calls Cheat_Update only when
  `cmd->command_number == [caller EBP + 8]` (the caller's sequence_number
  argument), then stores the number back to that slot. It is NOT a
  "command number changed" test: the same slot is also called from other
  engine paths with different ESI values, which the target skips. Replicating
  this condition is required for the command written to the engine to be the
  one carrying the NaN.

## INF-021 — SpeedHack (slot-20 stack-chain subtraction) and dead Teleport

- **Confidence:** high (target machine code 0x100076DD..0x10007745).
- **Teleport/Air Stuck closed by equivalence:** the user tested our staged
  build and the original VR.dll side by side on the current L4D2 build; both
  features are inert in the original as well. The NaN-movement exploit no
  longer produces a remote teleport or an air freeze in this game version, so
  our port (which matches the target code) is behaviorally equivalent.
- **SpeedHack gate:** byte_1004B024 set, dword_1004B02C (slider value) > 0.
  The key test is `GetAsyncKeyState(dword_1004B028) != 0` — the target tests
  the whole AX result (held or pressed since the previous call), unlike the
  other key checks which test 0x8000. Default key is 115 ('S').
- **Counter:** dword_1004A96C starts at 0. Each qualifying detour call:
  `old = counter--; if (old > 0) apply; else counter = value;`. On key-up the
  counter also reloads. Therefore the hack applies `value` times in a row,
  then skips one call, then repeats.
- **Stack chain:** starting from the caller's EBP (CInput::CreateMove's
  frame): p1 = [EBP], p2 = [p1], p3 = [p2]; the target executes
  `add dword ptr [p3+4], -5`. Our port performs the same three reads and
  subtraction with IsBadReadPtr/null guards (valid pointers behave
  identically). The target does not know the object type; the chain is
  reproduced structurally.
- **Choke mask:** after Cheat_Update and the command-number write-back, the
  target does `byte ptr [p2-0x1D] &= HIBYTE(g_MenuState)` (two chain reads
  from the caller EBP). Cheat_Update sets byte 3 of g_MenuState to 1 at its
  start, and the Fake Lag block clears it while choking, so this is the
  packet-choke mask. With Fake Lag still PENDING it ANDs with 1 (no-op); we
  now execute it anyway to match the target.
- **UI:** target Hook_EndScene 0x1000AEBC..0x1000B0AD order is Bunny Hop,
  SpeedHack + keybind, Air Stuck + keybind, Teleport + keybind, Fake Lag,
  Fake Lag slider (0..16, dword_1004AC10), SpeedHack slider (0..14,
  dword_1004B02C). The sliders appear after the Fake Lag checkbox, not next
  to their own checkboxes (target quirk preserved).
- **Config:** see INF-020; the descriptor includes SpeedhackVal,
  SpeedhackKey, FakeLagValue and the four checkbox flags, so save/load now
  write and read them.

## INF-022 — Fake Lag (choke counter + slot-20 send-byte mask)

- **Confidence:** high (Cheat_Update 0x10005F4A..0x10005F76 and target
  0x10007738 mask).
- **Counter:** while byte_1004AC0E is set, dword_1004A970 gates the choke:
  if <= 0 it reloads from dword_1004AC10 (slider 0..16, default 14) and does
  not choke; otherwise it decrements and clears HIBYTE(g_MenuState). Because
  Cheat_Update sets HIBYTE to 1 at its start (0x100055B0), the result is
  `value` choke updates followed by one release update.
- **Mask:** the slot-20 detour applies `byte [(second chain pointer)-0x1D] &=
  HIBYTE(g_MenuState)` after Cheat_Update (target 0x10007738). With the bit
  cleared the engine's outgoing-packet flag is masked to 0, holding the
  command; the release update leaves it at 1. The chain is walked from the
  caller EBP exactly as in the target.
- **Order:** the Fake Lag block sits after AutoStrafer and before the
  anti-aim block in Cheat_Update. This matters because anti-aim's desync
  test reads HIBYTE(g_MenuState) and must see the choke-cleared value.
- **Indicator label:** Hook_EndScene 0x10009AE6 draws "Fake Lag (Choking)"
  when byte_1004AC0E && dword_1004AC10 > 0 && IVEngineClient slot 26
  returns true. That draw call is part of the EndScene status overlay gated
  by byte_1004B06C (the "Indicators" checkbox, 0x1000B744), not part of the
  Fake Lag logic. It is not ported yet; it should be added with the EndScene
  overlay (watermark + "SMAC MODE IS DISABLED!" + color swatch helper
  sub_10008180).
- **Expected live behaviour:** with Fake Lag enabled and value > 0, outgoing
  commands are held for `value` updates then released once, producing
  visible movement stutter/rubber-banding. Compare against the original with
  the same slider value.

## INF-023 — Legit tab mapping and FOV Circle overlay

- **Confidence:** high (Hook_EndScene 0x10009DBA..0x1000A2AC and
  0x10009845..0x1000990E machine code).
- **Legit tab order/groups** (Hook_EndScene): Aimbot {Enabled,
  keybind, Hitbox (Auto/Head Only), Shotgun Mode, FOV 0..15, Smooth Y
  0..20, Smooth X 0..20}; Accuracy {No-Recoil, No-Spread, Position
  Adjustment, FOV Circle}; Target {Priority Special, Hitscan, Ignore Team,
  Ignore Idle Witch}; Triggerbot {Enabled, keybind, Delay 0..15, Aim At
  Head, Aim At Body, Auto Shove, Shove FOV 0..15}. Exact globals are listed
  in recovered.h (byte_1004AB3C..byte_1004AB7A, byte_1004AFFC..dword_
  1004B000, dword_1004ABA4).
- **Config descriptor quirks** (Menu_Render 0x10037B00): Smooth X
  (dword_1004AB54) is saved as "LEGITSMOOTHYAW"; Position Adjustment
  (byte_1004AB5C) as "LegitLagComp"; the FOV Circle bool (byte_1004AB7A)
  as "FovVisualize"; the Triggerbot key (dword_1004ABA4) as "TriggKey";
  Aim At Body as "triggerbody"; Rage enabled (byte_1004AB7B) uses the
  "Aimbot" string as its key. Smooth Y (dword_1004AB58), Triggerbot
  Enabled/Aim At Head (word at 0x1004AFFC) and Delay (dword_1004B000) have
  no descriptor entry = never saved by the target; we preserve that.
- **FOV Circle:** drawn when byte_1004AB7A is set and IVEngineClient slot
  26 (offset 0x68, likely IsInGame; also used by the radar and Fake Lag
  label) returns true. GetScreenSize is slot 5 (offset 0x14). Center is
  (width/2, height/2) in integer halves. Radius =
  (float)FOV / (float)fov_desired * (float)(width/2) - 0.5f, where FOV is
  dword_1004AB50 (Legit slider 0..15) and fov_desired is dword_1004AB8C
  (Visuals FOV Changer "Player FOV", default 51).
- The target draws the circle with an inlined ImDrawList::AddCircle on the
  background draw list (GImGui + 0x1C40 in ImGui 1.74): constant
  0x40C25C20 = 2*pi*29/30 proves num_segments = 30, thickness 1.0, color
  white 0xFFFFFFFF. Our compiled build emits the identical `push 1Dh` /
  0x40C25C20 sequence.
- **Safety:** we require fov_desired > 0 and a valid engine screen size;
  the target divides by fov_desired unguarded.

## INF-024 — Legit block gates and No-Recoil

- **Confidence:** high (Cheat_Update 0x10007222..0x100073B3).
- **Gate chain:** the target's Legit block runs only when byte_1004AB3C
  (Legit "Enabled") is set. Inside it, Auto Shove is checked first, then
  sub_100046B0 (active weapon m_iClip1 > 0) must pass; otherwise the target
  jumps to the end of Cheat_Update, skipping triggerbot, aimbot, No-Recoil
  and No-Spread. After the clip gate the target also requires team == 2
  (survivor) via sub_100016A0.
- **Active weapon:** sub_10001AA0 resolves DT_BaseCombatCharacter::
  m_hActiveWeapon on the local entity; the handle goes through
  IClientEntityList::GetClientEntityFromHandle (slot 4), and sub_100046B0
  reads DT_BaseCombatWeapon::m_iClip1 from that weapon entity.
- **No-Recoil (0x10007350):** if byte_1004AB4D is set, sub_100018B0 copies
  DT_BasePlayer::m_vecPunchAngle (resolved once, cached offset), and the
  target subtracts each component from cmd->viewangles (+0xC/+0x10/+0x14)
  with scale 1.0 (dword 0x10047328 = 1.0f). Our port calls the existing
  GetEntityPunchAngle helper, which is the exact sub_100018B0 port.
- **Port scope:** this stage implements only the gate chain + No-Recoil;
  Triggerbot (sub_100353F0), Auto Shove (sub_100359D0), the Legit aimbot
  (sub_10035BF0) and No-Spread (0x100073B3) are still PENDING. With
  dword_1004AB40/dword_1004ABA4 unbound (default 0) the target's
  aim/trigger actions do not run, so the live test isolates No-Recoil.
- **Safety fallback (INFERRED):** if the active-weapon/clip netprop offsets
  cannot be resolved, our clip gate passes instead of disabling the whole
  Legit block. The target has no such fallback. A one-shot
  `legit weapon off=%d clip=%d` log records the resolved offsets.

## INF-025 — EndScene status overlay and dead Legit features

- **Confidence:** high for the overlay strings/positions (machine code
  Hook_EndScene 0x1000980F..0x10009840); high for the dead-feature analysis.
- **Status overlay:** Hook_EndScene draws two outlined lines on the ImGui
  background draw list (GImGui + 0x1C40 in ImGui 1.74) every frame:
  * (10,10), white 0xFFFFFFFF: the XOR-decoded string
    "IF YOU PAID FOR THIS CHEAT YOU GOT SCAMMED, PLEASE REFUND." (blob at
    0x10047690/0x10047610/0x10047620 + qword 0xBEBAA8A8BECBAFBA + dword
    16833717, XOR key byte = (i - 71), length 0x3A);
  * (10,25), yellow 0xFF00FFFF, only when `(_BYTE)word_1004AC0C == 0`:
    "SMAC MODE IS DISABLED!".
  sub_10028CC0 renders outlined text (four black passes at +/-1 then the
  main colour). Our port uses ImDrawList::AddText on the background list and
  keeps the verified FOV circle in the same overlay function.
- **No-Recoil is dead by equivalence:** the user tested the ported feature
  against the original DLL and the original is inert on the current game
  build. Our gate/implementation matches the target machine code but the
  feature has no observable effect.
- **Auto Shove is dead by construction:** sub_100359D0 computes
  `normalize(enemyEye - viewangles) . localEye` and compares acos() to the
  Shove FOV. The distance check uses localEye/enemyEye correctly, but the
  angle operands are not view-direction vs target-direction; the compiled
  math cannot produce a sensible aim cone, so the feature does not work in
  the original either. Not ported.
- **Remaining Legit items:** Triggerbot (sub_100353F0) and the Legit aimbot
  (sub_10035BF0) are still PENDING; both need the EngineTrace Ray_t/trace_t
  ABI and the ITraceFilter vtable (target vftable 0x10041538) before they
  can be ported safely.

## INF-026 — Rage tab full mapping, config keys, and the shadowed anti-aim bug

- **Confidence:** high (Hook_EndScene 0x1000A2B1..0x1000A749; descriptor
  Menu_Render 0x10037B00; defaults sub_1003A300).
- **Groups/order:** Aimbot {Enabled, keybind, Hitbox (Auto/Head Only),
  Auto Shoot, Silent Aim, Perfect Silent Aim, Shotgun Mode, Auto Shove,
  Auto Melee, FOV 0..180}; Accuracy {No-Recoil, No-Spread, Position
  Adjustment, Prediction Fix}; Target {Priority Special, Hitscan,
  Ignore Team, Ignore Idle Witch}; Hack vs Hack {Enabled, Pitch, Real Yaw,
  Desync Yaw}. Exact globals are listed in recovered.h.
- **Config keys:** "Aimbot" is used as the key for the Rage Enabled flag
  (AB7B) because the descriptor reuses the group-label string. Others:
  AimKey, AimFov, AutoFire, Silent, PSilent, ShotgunMode, AutoShove,
  AutoMelee, NoRecoil, NoSpread, LagComp, PredictionFix, priorityspecial,
  bHitscanMode, IgnoreTeam, IgnoreWitch, AntiAim, Pitch, Yaw, FakeYaw.
  The UI Hitbox combo (AB6C) is not persisted; the descriptor saves
  dword_1004AB68 under "AimHitbox" but the UI never touches AB68.
- **Shadowing bug (fixed):** menu.cpp declared file-local statics
  g_RagePitch/g_RageRealYaw/g_RageDesyncYaw (and g_RageFov). They shadowed
  the extern globals that Cheat_Update reads, so the anti-aim combo indices
  never reached the anti-aim block. The statics and the dummy kRageA/B/C
  checkbox groups are removed; the rebuilt Rage tab binds the real globals.
  This also corrects the config key set for the anti-aim fields.
- **Scope:** only Hack vs Hack has live logic (implemented and previously
  verified). The Aimbot/Accuracy/Target controls now persist and drive
  their real flags, but sub_10003380 (Rage aimbot) and sub_10035BF0 (Legit
  aimbot) remain unported, so aiming/firing is not implemented yet.

## INF-027 — Legit triggerbot (sub_100353F0) ABI, filter and quirks

- **Confidence:** high (self-contained 0x341-byte function plus one caller
  at Cheat_Update 0x100072D3; all offsets read from the raw disassembly).
- **Signature:** __stdcall Triggerbot(CUserCmd *cmd, CBaseEntity *local);
  the caller pushes local then cmd, and the target's first two instructions
  load esi=cmd, edi=local.
- **Ray_t (0x48 bytes):** +0x00 start, +0x10 delta, +0x20 startOffset,
  +0x30 end, +0x40 m_pWorldAxisTransform, +0x44 m_IsRay, +0x45 m_IsSwept.
  startOffset/end/transform are zeroed, m_IsRay = 1, m_IsSwept = any delta
  component != 0. start = eye position; delta = (start + forward*8192) -
  start, with forward = (cosYaw*cosPitch, sinYaw*cosPitch, -sinPitch).
- **trace_t (0x58 bytes):** hitgroup at +0x44, entity pointer at +0x4C;
  the target zeroes only the first 0x24 bytes before TraceRay. The field at
  +0x48 is unnamed (physicsbone/worldSurfaceIndex inferred from Source).
- **ITraceFilter:** vftable 0x10041538; slot 0 (0x10002130) returns
  entity != filter->local; slot 1 (0x10002140) returns 0. Object is 8 bytes.
- **Engine call:** IEngineTrace vtable +0x14 (slot 5) with thiscall and the
  four arguments (ray, 0x46004003, filter, trace). Mask is CONTENTS_SOLID |
  CONTENTS_WINDOW | CONTENTS_MOVEABLE | CONTENTS_DEBRIS | CONTENTS_HITBOX.
- **Hit acceptance:** m_pEnt non-null, sub_10001EC0 (already recovered as
  IsPlayerChamsEligible) true, GetEntityTeam != 2. Then hitgroup 1 = head,
  2/3 = body, 4..7 gated on float dword_1004B004 != 0. That global has no
  writer anywhere in the module (only one read), so limb hits are dead.
- **Delay quirk:** if dword_1004B000 < 1 nothing happens at all, even on a
  matching hit. For 1..15 the threshold is B000/30 == 0, so the counter is
  never incremented and the shot is immediate; dword_1004B184 then resets.
- **Enabled quirk:** byte_1004AFFC ("Enabled##trigger") is bound in the menu
  (0x1000A1A3) and zeroed by the initializer, but no code reads it. The
  trigger is armed by the TriggerKey alone (GetAsyncKeyState high bit) when
  the outer Legit block is active.
- **Config:** Menu_Render descriptors persist dword_1004ABA4 as "TriggKey",
  byte_1004AFFD as "trighead" and byte_1004AFFE as "triggerbody". Delay
  (B000) and enabled (AFFC) are not persisted.
- **Safety deviations:** we reject a zero eye position (origin lookup
  failure) and validate g_pEngineTrace/vtable; healthy behavior matches.
- **Size:** ours 0x3A0 vs target 0x341 (guards + diagnostics).

## INF-028 — Legit aimbot (sub_10035BF0) selection math, hitboxes, gates

- **Confidence:** high (0xC71-byte function plus helpers, all offsets and
  constants read from the raw disassembly).
- **State globals (target .data):** eye 0x1004B1A8, view angles 0x1004B3D0,
  forward 0x1004B3E0, target index 0x1004B188 (-1), best distance
  0x1004B18C (99999.9f), aim point 0x1004B190, stored aim point 0x1004B19C,
  stored cmd angles 0x1004B1B4, health 0x1004B060, max health 0x1004B064,
  class-name pointer 0x1004B05C. Rage sub_10003380 shares the health/name
  fields.
- **Flow:** GetViewAngles (IVEngineClient slot 19) at entry; forward =
  (cosYaw*cosPitch, sinYaw*cosPitch, -sinPitch); iterate entities; per
  candidate SetupBones (entity+4 slot 13, 256 bone mask, 128 max bones),
  hitbox index by group, bbox corners by bone matrix, midpoint and LOS
  trace (sub_100026E0: mask 0x46004003, filter skips local, visible when
  the trace entity is the candidate, fraction == 1.0, or the candidate's
  class name is "CBreakableSurface" — the target checks the candidate, not
  the trace hit entity).
- **FOV check:** acos(clamp(dot(normalize(aim-eye), forward), -1, 1)) *
  57.295776 vs g_LegitAimFov. The 1/(dist + 1.1920929e-7) normalization and
  the min/max clamp reproduce the target's NaN propagation.
- **Priority:** category 3 -> 2.0, category 5 -> 3.0, else 1.0. With
  g_LegitPrioritySpecial the best distance is NOT updated, so its initial
  99999.9 remains the distance filter; priority must strictly improve.
- **Hitbox groups:** sub_100352B0 selects by group (0 auto, 1 head, 2
  chest, 3 stomach, 4/5 legs). Shotgun Mode (AB4C) forces group 3 for
  weapon ids 11/8/3/4. Hitscan (AB5F) with group 0 returns hitbox 0
  immediately. The aimbot's auto mode rewrites AB44 per candidate class
  (raw class ids: 264/0/263 -> 2, 99/265 -> 1, 270 -> 3, 276/277 -> 0;
  other ids leave it unchanged), and the new value is only read on later
  candidates because the rewrite happens after the hitbox lookup.
- **Hitbox bbox transform:** mstudiohitbox_t stride 68, group at +4, bone
  at +0, bbmin at +8, bbmax at +20; bone matrix stride 48. Both corners
  must be non-zero (float epsilon 1.1920929e-7); midpoint is the aim point,
  LOS is from the local eye.
- **Position Adjustment (AB5C):** writes cmd->tick_count =
  (m_flSimulationTime(local) + max(cl_interp, cl_interp_ratio /
  cl_updaterate)) / interval_per_tick + 0.5. ConVar GetFloat is convar
  vtable slot 11 after VEngineCvar007 FindVar slot 13.
- **Aim application:** pitch = atan((eye.z-aim.z)/hypot(eye-aim)),
  yaw = atan((eye.y-aim.y)/(eye.x-aim.x)) + (dx >= 0 ? 180 : 0), both
  normalized to [-180,180] with the target's loops. Smoothing (only when
  both sliders > 0) divides the pitch delta by Smooth Y (AB58) and the yaw
  delta by Smooth X (AB54), relative to the engine view angles captured at
  entry. The result is pushed through IVEngineClient slot 20
  (SetViewAngles), which the target does through its own detour.
- **Call-site gates:** Legit enabled, team 2, m_iClip1 > 0, weapon id in
  the sub_10004790 whitelist, aim key held. Shotgun ids {11,8,3,4} bypass
  the can-fire check; otherwise m_flNextPrimaryAttack must be <=
  (m_nTickBase + 1) * interval_per_tick and byte_1004B2E4 must be zero
  (the byte has no writer anywhere, so it is always zero). A held aim key
  that fails the can-fire check on a non-shotgun also skips the No-Recoil
  block, exactly as the target's jump to 0x1000767F does.
- **Safety deviations:** IsBadReadPtr guards around engine/interface/
  model/bone reads, and tick_count is skipped when interval_per_tick is
  non-positive. Healthy behavior matches.
- **Size:** ours 0xD00 vs target 0xC71.

## INF-029 — Rage aimbot (sub_10003380), PSilent flow, hidden Smooth

- **Confidence:** high (0xE08-byte function, helper pair, and the enclosing
  Cheat_Update section 0x1000647C..0x10006F60 read from raw disassembly).
- **Runtime state:** distinct from the Legit aimbot except the shared
  health/name fields: eye 0x1004B120, view angles 0x1004B25C, forward
  0x1004B248, target index 0x1004B100, best distance 0x1004B104 (99999.9f),
  aim point 0x1004B108, stored aim point 0x1004B114, stored cmd angles
  0x1004B12C, has-target 0x1004B058, attack-suppressed 0x1004B057.
- **Settings:** Rage Aimbot Enabled byte_1004AB7B gates the whole section;
  AimKey dword_1004AB9C; AimFov dword_1004ABB0; Hitbox mode dword_1004AB6C
  (Auto/Head Only); resolved hitbox dword_1004AB68 (persisted "AimHitbox");
  ShotgunMode byte_1004AB79; Hitscan byte_1004AB78; IgnoreTeam
  byte_1004ABBD; IgnoreWitch byte_1004ABC6; PrioritySpecial byte_1004ABC1;
  Auto Shoot byte_1004ABC0; Silent byte_1004ABC2; Perfect Silent
  byte_1004ABC3; Prediction Fix byte_1004AB99; Position Adjustment
  byte_1004AB7E (only when SMAC mode off); Smooth dword_1004AB70
  (persisted "Smooth", no UI).
- **Call flow:** gated by byte_1004AB7B; clip check (sub_100046B0) and team
  == 2 fall through to the Legit section; weapon whitelist (sub_10004790)
  and Rage AimKey gate the aimbot; shotguns {11,8,3,4} or canFire
  (m_flNextPrimaryAttack <= (m_nTickBase+1)*interval_per_tick, with the
  never-written byte_1004B278) allow it. Perfect Silent calls the aimbot
  only when Fake Lag (byte_1004AC0E) and SMAC mode are off, then restores
  the command view angles/forwardmove/sidemove unless a shot went out
  (buttons & 1 && canFire); otherwise the second call site is used. A
  failed canFire gate jumps past the Rage No-Recoil and the entire Legit
  section (target 0x1000767F).
- **Aim math:** identical to the Legit aimbot (acos FOV, best distance,
  priority, bbox transform, LOS sub_100026E0). Smoothing divides BOTH the
  pitch and yaw deltas by dword_1004AB70 and only runs when Silent, Perfect
  Silent and Auto Shoot are all zero. SetViewAngles (slot 20) is skipped
  when Silent is on and SMAC mode is off.
- **Auto Shoot:** with Prediction Fix on and the Speedhack key
  (dword_1004B028) held, IN_ATTACK is pressed at most every 120 ms using
  the target's QPC timer (initialized on first use). Otherwise IN_ATTACK
  is pressed every call; in SMAC mode the target then applies a two-byte
  state machine (byte_1004A92C / byte_1004A8E8) that suppresses attack
  unless m_fFlags has bit 0 (on ground), with a one-frame re-press path.
- **Menu quirks:** target 0x10009DBA (Legit tab) clears Rage Aimbot when
  Legit is enabled; target 0x1000A2B1 (Rage tab) clears Legit when Rage
  Aimbot is enabled. Implemented in menu.cpp.
- **PENDING:** Rage Auto Shove block (0x10006489..0x100067B7), melee
  weapon-id-19 path (0x100067B7..0x10006A02), and the SMAC-mode angle
  prediction block (0x10006C2D..0x10006D97). With default settings
  (Auto Shove off, SMAC off, firearm) the ported path matches the target.
- **Safety deviations:** interface/model/bone guards and a non-positive
  interval_per_tick guard; healthy behavior matches.
- **Size:** ours 0xEB0 vs target 0xE08.

## INF-030 — EndScene Indicators window and Fake Lag bar

- **Confidence:** high (Hook_EndScene 0x1000990E..0x10009B20 plus the helper
  sub_10008180, mapped against our ImGui 1.74 build offsets).
- **Window:** the target sets ImGuiNextWindowData directly:
  Flags (context+6700) |= 2 (HasSize), SizeCond (+6708) = 1 (Always),
  SizeVal (+6732/+6736) = (200,50), then Begin("Indicators", NULL, 0x22).
  0x22 = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse. Public
  equivalent: SetNextWindowSize(ImVec2(200,50), ImGuiCond_Always) +
  Begin("Indicators", nullptr, those flags).
- **Gate:** byte_1004B06C (g_Indicators) wraps the window. The bar/label
  additionally require byte_1004AC0E (Fake Lag), dword_1004AC10
  (FakeLagValue) > 0, and IVEngineClient slot 26 (in-game gate).
- **Bar helper (sub_10008180):** an inlined Dummy-like item. Width =
  400 - 2*style.FramePadding.x (style.FramePadding is at context+5448);
  size height 6. It calls ItemSize/ItemAdd, then two ImDrawList
  AddRectFilled calls: base rect [cursor.x .. cursor.x + 0.7*width] in the
  first color, overlay [cursor.x .. cursor.x + 0.7*width*pulse] in the
  second color, both 6 px tall with rounding 0 and ImDrawCornerFlags_All.
  Afterwards dword_1004A9BC (pulse) = sin(g.Time * 13.0) * 0.5 + 0.5,
  using the previous frame's value for the overlay. Initial pulse is 0.
- **Colors:** ImGuiContext.Style is at +5392; ImGuiStyle.Colors is at +176
  with 16-byte entries, so context+5904 = Colors[21] = ImGuiCol_Button and
  context+5920 = Colors[22] = ImGuiCol_ButtonHovered. The target packs them
  manually (saturate each channel, *255 + 0.5, alpha multiplied by
  style.Alpha) which is exactly ImGui::GetColorU32.
- **Draw order:** status overlay, FOV circle, Indicators window, radar,
  then the cheat menu. Our DrawIndicators is called in that position.
- **Clipping quirk:** the 200-px window clips the 392-px bar; preserved by
  using the same window size and the window draw list.

## INF-031 — Movable radar window and radar frame anchoring

- **Confidence:** high (Hook_EndScene 0x10009B20..0x10009C7C and the radar
  painter sub_1003BFB0 read from raw disassembly).
- **Window:** when byte_1004AC30 (Radar) and IVEngineClient slots 27 and 26
  are true, the target pushes ImGuiCol_WindowBg with the colour loaded from
  0x100474C0 = (0.2,0.2,0.2, alpha 0), sets NextWindowSize to
  (292,306) with Cond Always (context+6700 |= HasSize, +6708 SizeCond,
  +6732/+6736 SizeVal), and calls Begin("Radar", NULL,
  ImGuiWindowFlags_NoResize|NoCollapse). After Begin it copies
  window->Pos (window+12/+16) to dword_1004A4B4/A4B8 and window->Size
  (window+20/+24) to dword_1004A4BC/A4C0, then End() and PopStyleColor().
- **Anchoring:** sub_1003BFB0 computes frameCenterX = (int)(A4BC*0.5 +
  A4B4), frameCenterY = (int)(A4C0*0.5 + A4B8 + 10). The dot projection
  sub_1003C1D0 uses the same X and Y without the +10. The 292x306 frame
  background, 280x280 border box and crosshair are drawn from that center
  through the VGUI surface, so the radar follows the ImGui window.
- **Dead code preserved as documentation:** the dword_1004AC28/AC2C
  "drag" block inside sub_1003BFB0 computes mouse deltas and writes those
  globals, but EndScene overwrites AC28/AC2C from the window position every
  frame, and nothing consumes the delta. dword_1004A9C4/A994 are write-only
  integer copies. Neither affects the visible radar, so the port creates
  the window and reads its geometry but skips the orphan block.
- **Why our radar was fixed before:** radar.cpp used the game window rect
  and screen centre; the original has no such fallback and centres on the
  ImGui window, making the radar draggable.

## INF-032 — Legit movement fix and dead-feature audit

- **Movement fix (0x1000757C..0x1000767A).** The Legit section saves the
  command yaw at 0x1000725B ([ebp-0x40]) before triggerbot/aimbot changes.
  The tail recomputes the movement vector: magnitude = sqrt(forward^2 +
  side^2); sub_10036890 returns moveYaw = atan2(side, forward) * 57.295776
  normalized to [0,360) (with a 90/270 pitch and zero yaw when forward and
  side are both zero; only the yaw is consumed); moveAngle =
  (cmd->viewangles[1] - savedYaw + moveYaw) * 0.017453292; forwardmove =
  cos(moveAngle) * magnitude; sidemove = sin(moveAngle) * magnitude. This
  keeps the world-space movement direction when the aimbot rotates the
  view. The canFire-failure jump at 0x1000767F skips this tail.
- **Invisible Jockey (byte_1004B014).** The block at 0x10005D00 scans
  entities (skip local/dormant/non-eligible; process category 5, or any
  entity when the local player is category 3), computes the distance
  between local and candidate eye positions, then compares with
  `comiss 0.0, dist; jbe skip`. The write to cmd->forwardmove
  (bit pattern 0x7CF0BDC2) therefore only happens when 0 > distance,
  which cannot occur. Dead by construction; ported as-is. No config
  descriptor entry exists for B014.
- **Auto Pistol (byte_1004AB7C) / Server Lagger (byte_1004B03D).** Menu
  bindings at 0x1000B562 / 0x1000B496, config descriptor entries at
  0x10038C31 (key "AutoPistol") and 0x100394DD (key "serverlag"), default
  zero. No code reads either flag, so both are UI/config-only in the
  target. Our menu now binds the real globals and persists those keys.
- **PaintTraverse dead items.** dword_1004AFF4 is read once as a float
  after sub_100045A0; the branch is only entered for NaN (jnp after
  ucomiss), and the global has no writer. dword_1004B048 gates the
  sub_1003AD40 panel the same way and also has no writer. Both blocks are
  unreachable in the original; our PaintTraverse hook remains correct
  without them.

## INF-033 — Rage Auto Shove (sub_10002D90)

- **Confidence:** high (0x5D0-byte function read from raw disassembly; the
  enclosing gate at Cheat_Update 0x10006489).
- **Gate:** g_RageAutoShove (byte_1004ABC4) and the sub_10004790 weapon
  whitelist; runs before the clip check. Inside, the local player must have
  DT_TerrorPlayer::m_isIncapacitated == 0, cmd->buttons must not contain
  IN_JUMP (0x20), and the Rage Aim key (dword_1004AB9C) must NOT be held
  (GetAsyncKeyState high bit clear).
- **Candidate filters (target order):** null/local, sub_10001EC0, model
  (entity+4 slot 8), team == 4, team == 2, dormant (entity+8 slot 7),
  SetupBones (entity+4 slot 13, 128 bones, mask 256), ghost set for
  category 5 + team 3, m_usSolidFlags == 20, category not in {3,4,5}.
- **Geometry:** dx/dy/dz = localEye - entityEye (GetEntityEyePosition,
  sub_100014B0); 3D distance must be strictly < 92.0 (0x100473DC). Pitch =
  atan(dz / sqrt(dx^2+dy^2)); yaw = atan(dy/dx), plus 180 when dx >= 0.
  Both are normalized to [-180,180]. The target's fdclass/NaN checks map to
  isfinite(pitch) && isfinite(yaw) && isfinite(cmd->forwardmove).
- **Effect:** cmd->buttons |= 0x800 (IN_ATTACK2), viewangles[0] = pitch,
  viewangles[1] = yaw, viewangles[2] = 0. The target keeps scanning after
  a hit (no break), so the highest matching entity index wins.
- **Dead side effect:** the function resolves GetLocalActiveWeapon (through
  sub_10001AA0 + GetClientEntityFromHandle) and discards the result. Omitted.
- **PENDING:** SMAC-mode prediction block before the shove
  (0x100064C9..0x100066B5, including an IN_ATTACK2 clear), the
  g_RageEnabled anti-aim movement adjustment after the shove
  (0x100066C9..0x100067AA), and sub_10004190's movement normalization when
  anti-aim is off. With default settings (SMAC off, Hack vs Hack off) the
  core aim+shove behavior is complete; movement may differ slightly from
  the target's extra normalization.

## INF-034 — No-Spread is BLOCKED; diagnostic cleanup

- **Status:** analyzed in full, deliberately not implemented (too
  build-specific to verify); both target blocks remain unported.
- **Target blocks:** Rage 0x10006F60..0x1000712D, Legit
  0x100073B3..0x1000757C. Both are gated by the respective No-Spread flag,
  the weapon whitelist, and cmd->buttons & IN_ATTACK.
- **Target technique (high confidence on control flow, low confidence on
  runtime values):**
  1. `GetModuleHandleA("client")`; locate the `SharedRandomFloat` string in
     client.dll, then resolve its references backwards to the exported
     engine function that uses it.
  2. Scan the resolved function for the random-seed global (the target
     looks for the first `mov eax, [imm32]` within ~0x100 bytes of the
     entry) and write `sub_10037080(command_number) & 0x7FFFFFFF` into it.
     `sub_10037080` is an MD5-based block hash over the command number;
     the target uses the dword at hash-state + 2.
  3. Read the weapon's raw float spread at `weapon + 0xD0C` (not a
     netprop), call the resolved engine function with
     `("CTerrorGun::FireBullet HorizSpread", -spread, spread, 0)`, and
     subtract the result from `viewangles[0]`; repeat with
     `VertSpread` -> `viewangles[1]`.
- **Why BLOCKED:** the port depends on a runtime pattern scan inside the
  user's current `client.dll`, an unverified MD5-seed derivation, and a raw
  weapon field offset. A wrong scan silently corrupts the seed or calls an
  arbitrary address; there is no way to validate equivalence without
  reverse-engineering the exact client.dll build, and a crash at injection
  would compromise the oracle. Not implemented at the time; the two
  No-Spread checkboxes stayed config-only.
- **Superseded by INF-035:** the scan was subsequently decoded to the byte
  and validated offline against the installed client.dll, so the blocks are
  now implemented (build staged for live verification).
- **Cleanup record:** the final deliverable removes all recovery
  instrumentation: the VEH exception logger, the
  ExitProcess/RtlExitUserProcess/TerminateProcess detours, the boot log
  file/module, and every temporary ESP/radar/aimbot/slot-20/chams
  diagnostic. The clean DLL was load-tested (LOAD_OK, no loaded.log) and is
  byte-identical to the build exercised by the last live-verification
  pass. Remaining unported behavior (Rage SMAC prediction, post-shove
  anti-aim movement, melee id-19 path) is listed in README.md.

## INF-035 — No-Spread implemented (Rage + Legit)

- **Confidence:** high (byte-level disassembly of both blocks and all three
  scan helpers; validated offline against the installed client.dll).
- **Placement:** Rage 0x10006F81..0x1000712A sits in Cheat_Update's Rage
  section after No-Recoil, inside the team==2 and clip checks but outside
  the aim-key gate. Legit 0x100073BA..0x1000757C sits after Legit
  No-Recoil and before the movement fix, likewise outside the aim-key
  gate. Both are skipped by the canFire-failure early return
  (0x10006E16 / 0x10007343, our skipLegit/skipNoRecoil).
- **Gates:** the No-Spread flag (ABBF Rage / AB4E Legit), sub_10004790 on
  the cached weapon entity (v162), and `cmd->buttons & IN_ATTACK`.
- **Spread source:** a fresh `GetLocalActiveWeapon(local)` ->
  `IClientEntityList::GetClientEntity(handle)` gives the weapon entity;
  the float at `weapon + 0xD0C` is used as both the min/max range.
- **Seed:** `MD5_PseudoRandom(cmd->command_number) & 0x7FFFFFFF`, written
  to the engine seed global. sub_10037080 is standard MD5 over the
  4-byte little-endian command number, returning the dword at digest
  offset 6 (sub_10036A00 is the unrolled MD5 transform).
- **Effect:** call the resolved client.dll function twice:
  `f("CTerrorGun::FireBullet HorizSpread", -spread, spread, 0)` and
  `f("...VertSpread", -spread, spread, 0)`; multiply each returned float
  by -1.0f (0x10047448) and add to viewangles[0] / viewangles[1]. The
  function is cdecl (caller cleans 0x10), returns float, and its prologue
  starts by loading the seed global.
- **Runtime resolution, exact semantics:**
  1. `GetModuleHandleA("client")`.
  2. sub_10037890 scans linearly from the module base for the 18-byte
     literal `"SharedRandomFloat\0"`, then walks backwards from literal+1
     comparing dwords to the literal's VA; the match is the immediate
     operand of an instruction that references the string.
  3. Inline walk from 0x10007006 checks `(b0 & 0xF0)==0x50`,
     `(b1 & 0xF0)==0x80`, `(b2 & 0x0F)==0x0C` (55 8B EC) going backwards;
     the hit is the function entry.
  4. sub_100378E0's loop looks like a 32-byte signature compare but the
     `test eax,eax / jnz` structure means only its first byte (0xA1) is
     compared; it scans the first 0x100 bytes for the first `mov eax,
     [imm32]`. The imm32 is the seed global's address; the target then
     writes the MD5 seed there. The nearby bytes are the HorizSpread
     literal and not an actual signature table.
  5. The literal reference carries a HIGHLOW base relocation, so the
     backwards dword search still works after ASLR relocation.
- **Implementation:** src/md5.cpp (MD5_PseudoRandom), src/nospread_scan.cpp
  (scan with module bounds and SEH), src/nospread.cpp (per-tab static
  resolvers mirroring the target's two static pairs; whitelist and
  IN_ATTACK re-checked). MD5 verified against hashlib for 7 seeds;
  tools/nospread_scan_check.cpp maps the installed client.dll and runs the
  real scan code, resolving function RVA 0x1ACDB0 (55 8B EC) and seed RVA
  0x6BF868 (.data) - PASS. tools/scan_client_nospread.py independently
  reproduces the result and disassembles the function.
- **Safety deviations (failure paths only):** failed scans skip the feature
  instead of calling/dereferencing null; the seed address must point back
  into client.dll; the spread address is readability-checked. Successful
  paths match the target instruction-for-instruction.
