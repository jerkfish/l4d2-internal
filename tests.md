# Test index

No automated test harness exists in this workspace: `tests/` is empty,
`tools/check.py` and `REPORT.md` are absent, and there is no git history.
T001-T004 are runnable tools; T005-T022 are live inject-and-compare tests
recorded in `STATE.md` and confirmed by the user against
`original/target.dll`. "PASS" means the covering subsystem was verified;
individual helpers were not isolated unless a test says so.

The oracle `VR.dll` is not included in this publishable copy; supply it
locally to reproduce T005-T022. T001-T004 run from the copied tree.

The live-verified binary was `staging/rebuilt/VR.dll` in the local working
tree (not included; sha256
`37b156bf...`). A clean rebuild after the verification produced a
different PE hash (`f70a6147...`) because the build is not reproducible
(timestamps/debug metadata); source and code paths are unchanged.

## M1 - bootstrap, menu, config

T001 | orig n/a (build harness) | input: clean source tree, VS 2022 14.38 x86 | expected: VR.dll links with 0 errors | rebuilt: 0 errors, only pre-existing overlay.cpp C4244 warning | PASS | tools/build.bat
Visual Studio build: `tools\build_vs.bat` (or `vs\VR.sln`, Release|Win32) was built and load-tested in the copied tree: 0 errors, only the known C4244 warning, `LOAD_OK`.

T002 | orig n/a (load harness) | input: LoadLibrary(build\Release\VR.dll) | expected: module loads and produces no diagnostic file | rebuilt: LOAD_OK base=0x72420000, no loaded.log | PASS | tools/loadtest.cpp
T005 | orig 0x9160 sub_10009160 + 0x76A0 sub_100076A0 -> Hook_EndScene + Hook_EngineClient_Slot20 | input: inject, press END/INSERT | expected: one-time scam MessageBox, ImGui menu toggles, menu owns input while open | rebuilt: identical; input capture confirmed as original behavior | PASS | src/hooks.cpp:692, src/hooks.cpp:144
T012 | orig 0x37B00 sub_10037B00 + 0x10009DBA UI blocks -> Menu_Render | input: open tabs, change values, save/load | expected: same labels/order/defaults, tab mutual exclusion, persisted key quirks | rebuilt: identical; config survives restart | PASS | src/menu.cpp:550

## M2 - ESP, radar, overlays, chams

T006 | orig 0x3C410 sub_1003C410 -> DrawEsp | input: in-game survivor/infected, all player ESP options | expected: box/name/distance/health bar/weapon/ammo/witch rage/lines/team/bot tag | rebuilt: identical; no crash from projected text | PASS | src/esp.cpp:418
T007 | orig 0x3DB30 sub_1003DB30 -> DrawWeaponSpawnEsp | input: map with weapon spawns | expected: on-screen weapon-spawn labels, same names/filter | rebuilt: identical | PASS | src/esp.cpp:607
T008 | orig 0x3BFB0 sub_1003BFB0 + 0x3B820 sub_1003B820 -> DrawRadar | input: enable radar | expected: frame, rotated dots, labels, same colors/range | rebuilt: identical | PASS | src/radar.cpp:112
T013 | orig 0x9160 sub_10009160 overlay blocks -> DrawEndSceneOverlay | input: enable Legit/FOV circle | expected: scam/SMAC status text and FOV circle | rebuilt: identical | PASS | src/hooks.cpp:570
T017 | orig 0x990E block + 0x8180 sub_10008180 -> DrawIndicators | input: Fake Lag on, value > 0 | expected: 200x50 Indicators window, 392 px clipped bar, sin(Time*13) pulse, Button colors | rebuilt: identical | PASS | src/hooks.cpp:623
T018 | orig 0x9B20 block -> DrawRadarWindow | input: drag radar window | expected: 292x306 movable window, WindowBg alpha 0, frame at pos+size/2+10 | rebuilt: identical | PASS | src/hooks.cpp:666
T022 | orig 0x4980 sub_10004980 + 0x4800 sub_10004800 -> Hook_DrawModelExecute | input: enable player/hand/weapon/infected chams | expected: XQZ materials, same model filters/passes | rebuilt: identical in final regression | PASS | src/hooks.cpp:322

## M3 - movement and combat

T009 | orig 0x5A36 block -> Cheat_Update thirdperson | input: toggle thirdperson | expected: deadflag writes and camera change | rebuilt: identical | PASS | src/cheat.cpp:1352
T010 | orig 0x76DD block -> EngineSlot20_Process | input: hold SpeedHack key | expected: movement rate changes by the same counter logic | rebuilt: identical | PASS | src/hooks.cpp:73
T011 | orig 0x76A0 slot-20 mask -> EngineSlot20_Process | input: enable Fake Lag | expected: packet-choke counter/mask and Indicator bar | rebuilt: identical | PASS | src/hooks.cpp:73
T014 | orig 0x353F0 sub_100353F0 -> Legit_Triggerbot | input: hold trigger key on target | expected: mask 0x46004003 eye trace, hitgroup match, Delay 0 never fires, Delay 1-15 immediate, limb flag dead | rebuilt: identical quirks | PASS | src/cheat.cpp:395
T015 | orig 0x35BF0 sub_10035BF0 -> Legit_Aimbot | input: hold Legit aim key | expected: FOV/smoothing/hitbox/whitelist/can-fire gates, silent handling, movement fix preserves world direction | rebuilt: identical | PASS | src/cheat.cpp:727
T016 | orig 0x3380 sub_10003380 + 0x2D90 sub_10002D90 -> Rage_Aimbot + Rage_AutoShove | input: hold Rage aim key; then Auto Shove without aim key | expected: priority/hitbox/silent/perfect-silent/auto-fire/prediction; shove < 92 units, IN_JUMP/incap blocked, aim key disables | rebuilt: identical | PASS | src/cheat.cpp:1049, src/cheat.cpp:1277
T019 | orig dead blocks -> Cheat_Update | input: Teleport/Air Stuck/No-Recoil/Invisible Jockey/Auto Pistol/Server Lagger | expected: inert in original (inverted guard/unwritten flags) | rebuilt: inert identically | PASS | src/cheat.cpp:1352

## M4 - No-Spread, offline checks, final regression

T003 | orig 0x37080 sub_10037080 -> MD5_PseudoRandom | input: seeds 0,1,2,42,12345,0xFFFFFFFF,0x12345678 | expected: MD5(seed LE)[6:10] per hashlib | rebuilt: exact match, all 7 | PASS | src/md5.cpp:52
T004 | orig 0x37890 sub_10037890 + 0x378E0 sub_100378E0 -> NoSpread_ScanModule | input: mapped installed client.dll (7.9 MB, base 0x10000000) | expected: function RVA 0x1ACDB0 (55 8B EC), seed RVA 0x6BF868 | rebuilt: exact match, C++ and Python replays | PASS | src/nospread_scan.cpp:79
T020 | orig n/a (regression) | input: inject final clean build, exercise all features | expected: all prior behavior, no loaded.log, no crash/exit detours | rebuilt: identical; user verified | PASS | local staging copy (not included; sha256 abd9da18...)
T021 | orig 0x6F81 + 0x73BA Cheat_Update blocks -> NoSpread_Rage/Legit | input: enable No-Spread in either tab, whitelisted firearm, hold fire | expected: impacts centered vs spread cone; one-time client.dll scan; no crash | rebuilt: identical; user verified; staged sha256 37b156bf... | PASS | src/nospread.cpp:134

## Failing / flaky tests

None current. Historical failures, all resolved and retained here for traceability:

- Load test reported "LOG CREATED" once because it ran against a stale staged DLL copied before the clean build; rerunning with the clean DLL produced LOAD_OK and no log. Cause: staging mistake, not code.
- Triggerbot initially used wrong trace offsets; raw disassembly corrected hitgroup +0x44 and entity +0x4C. Cause: decompiler type coercion.
- Legit aimbot corner2 zero-ish test was written as an impossible condition; corrected to `> -eps && < eps`. Cause: transcription error.
- Radar diagnostic build failed to compile after re-anchoring the frame (unused left/top/width/height); diagnostic code was rewritten. Cause: stale locals after a port change.
- No-Spread was marked BLOCKED in INF-034 before the scan was fully decoded; INF-035 and T004/T021 supersede it. Cause: verification-risk call, not a runtime failure.
