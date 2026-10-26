# Changelog

Chronological by milestone, not by chat message. Dates are the recovered
session dates; commit hashes are unavailable because the workspace has no
git repository.

## M0 - Recon and toolchain (2026-10-25)

Target proved to be PE32 i386 (not x64), image base 0x10000000, no export
table, MSVC 14.38, /O2 /GS /GL /LTCG; ImGui 1.74, MinHook 1.3.3 and Kiero
1.2.9 are statically linked. IDA MCP bridge fixed on port 13337; 509
functions, 688 strings, 61 structures catalogued. Dead end: the x64 plan
and export-based harness were invalid; replaced with inject-and-compare
against the original DLL.

## M1 - Bootstrap, hooks, interfaces, menu

Recovered DllMain/thread/window bootstrap, EndScene MinHook, VmtHook and the
slot-20/41/19 detours, InitInterfaces, netprop walker and the menu/config
descriptor. Key discoveries: slot-20 receives CUserCmd in ESI and validates
against the caller's EBP+8; VmtHook copies the vtable shifted by one; ImGui
1.74 context offsets (Style +5392, colors +5904/+5920); config keys live in
the 0x37B00 descriptor. Dead end: the slot-20 `__userpurge` prototype only
resolved from raw assembly. Metrics: clean build, T001/T002/T005/T012 PASS,
3 size comparisons.

## M2 - ESP, radar, overlays, chams

Recovered player and world ESP, movable radar, status/FOV overlays,
Indicators, and the chams material pipeline. Key discoveries: DrawPrintText
takes three stack arguments; projected INT_MIN coordinates crash the text
renderer; bounding boxes reduce eight SSE min/max chains; radar centers on
the ImGui window, not the game window. Dead ends: crash hunt from bad text
arguments and misplaced distance text, both fixed by ABI/format changes.
Metrics: T006/T007/T008/T013/T017/T018/T022 PASS.

## M3 - Movement, triggerbot, aimbots, Auto Shove

Recovered Teleport/Air Stuck/No-Recoil equivalence, SpeedHack, Fake Lag,
Legit Triggerbot, Legit/Rage aimbots, movement fix and Rage Auto Shove.
Key discoveries: trace mask 0x46004003, hitgroup +0x44, entity +0x4C;
aimbot shared state and vestigial fields; ten dead-by-construction features.
Dead ends: triggerbot trace offsets corrected by raw disassembly, aimbot
corner2 condition fixed, menu statics shadowing globals removed. Metrics:
T009/T010/T011/T014/T015/T016/T019 PASS, 4 size comparisons.

## M4 - No-Spread, cleanup, final build

Recovered Legit/Rage No-Spread: MD5_PseudoRandom, client.dll
SharedRandomFloat scan, mask-based prologue walk, seed global, +0xD0C
spread, Horiz/Vert calls. Offline validation resolved the installed
client.dll at function 0x1ACDB0 / seed 0x6BF868. Removed VEH, exit-process
hooks, boot logging and all diagnostics. Dead end: the earlier BLOCKED
decision was reversed once the scan was decoded and validated. Metrics:
T003/T004/T020/T021 PASS; 4 blocks remain documented.

| Milestone | Tests passed | Structural comparisons | Blocked items | Artifact |
|---|---:|---:|---:|---|
| M0 | 0 | 0 | 0 | recon only |
| M1 | 4 | 3 | 0 | first clean build |
| M2 | 7 | 0 | 0 | visuals build |
| M3 | 7 | 4 | 4 (PENDING) | combat build |
| M4 | 4 | 3 | 4 (documented BLOCKED) | staging/rebuilt/VR.dll, sha256 37b156bf... |
| Total | 22 | 10 | 4 | 174 ledger rows |
