# VR.dll recovered source

Recovered C++17 source of the injected Left 4 Dead 2 cheat module `VR.dll`
(build dated 2024-02-09), reconstructed from the original `target.dll` with
IDA Pro. This is the publishable source-only copy: the original binary, IDA
databases, raw analysis dumps, built/staged DLLs, injector and logs are not
included (see `PUBLISHING.md`). The rebuilt module is behaviorally equivalent
for the features listed below; exact files and evidence are in `ledger.csv` /
`ledger.json`.

## Discrepancies

- No git repository exists in this workspace. Git history, diffs and commit
  hashes are unavailable; every ledger `iteration_commit` is
  `unknown (no git repository)`.
- `REPORT.md`, `tools/check.py` and its output history do not exist;
  `tests/` is empty. The requested cross-check against check.py/REPORT.md
  cannot be performed. Counts below come from `ledger.json`, the build map
  and `STATE.md`.
- The target has 509 IDA functions (including CRT/library code). The ledger
  covers 166 project function rows, not all 509: 126 rows map to 82 distinct
  original addresses/blocks, 40 helper rows have no distinct original
  address.
- Ledger totals: 174 rows (166 functions + 8 type/global rows), PASS 170,
  BLOCKED 4, FAIL 0, UNTESTED 0, INFERRED 17.
- The live-verified staged binary (not included) had sha256
  `37b156bf...`. The clean rebuild below
  produced sha256 `f70a6147...`; the build is not reproducible because of
  timestamps/debug metadata. The verified artifact was not replaced.
- Source density: 18 `INFERRED` comments; 17 ledger rows marked INFERRED
  (the comments also cover globals/types and safety guards inside otherwise
  confirmed functions).
- Ledger schema: the requested fields are present, plus a separate
  `orig_name` column. The 8 type/global rows use kind `type`/`global`
  (outside the function-kind enum) as the brief allows type/global rows.
  Third-party code (ImGui, MinHook, Kiero, CRT) is excluded.
- Ledger provenance: `build/Release/VR.map` (function inventory),
  `NOTES/ida/functions.tsv` / `functions_lines.tsv` (source definitions),
  `NOTES/ida-naming.md` and INF-001..INF-035 (address/name evidence),
  `STATE.md` and `tests.md` (verification results). Regenerate with
  `python tools/gen_ledger.py`.

## What the DLL does

Injected 32-bit L4D2 client module. It installs D3D9 EndScene and engine
VMT hooks, draws an ImGui menu and overlays, and modifies `CUserCmd` in the
slot-20 ProcessInput detour. Recovered features: menu/config, player and
world ESP, movable radar, chams, thirdperson, Bunny Hop/Auto Strafer,
SpeedHack, Fake Lag, triggerbot, Legit and Rage aimbots, Rage Auto Shove,
movement fix, No-Spread, and a group of dead-by-construction or inert
miscellaneous features. The original shows a one-time scam-warning
MessageBox on first EndScene; the rebuild preserves it.

## Target fingerprint (oracle, not included)

- Original `target.dll` (not included), 313,344 bytes, sha256
  `fc5625237f287f59a2d9d5cd4b9202e861f1cb46f9a76bf2d14272baafa0397d`
- PE32 i386, image base `0x10000000`, entry `0x3EAB3`, no exports
- MSVC 14.38, `/O2 /GS /GL`, `/LTCG`, `/DYNAMICBASE /NXCOMPAT`
- ImGui 1.74, MinHook 1.3.3, Kiero 1.2.9 (statically linked)

## Build

Requirements: Windows, Visual Studio 2022 Community with the v143 14.38
toolset, CMake 3.20+.

```
tools\build.bat        # configure + build x86 Release -> build\Release\VR.dll
tools\loadtest.bat     # LoadLibrary smoke test -> LOAD_OK
tools\md5_check.bat    # MD5_PseudoRandom vs hashlib vectors
tools\nospread_scan_check.bat  # real scan vs installed client.dll
```

Compile flags: `/O2 /GS /GL /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS`, `/MD`;
link `/LTCG /DYNAMICBASE /NXCOMPAT /DEBUG /MAP`. Output is
`build\Release\VR.dll`. For live testing, copy it to a local staging
location; staged binaries are not part of this repository.

Clean-build confirmation (2026-10-26, after the No-Spread port): rebuild
finished with 0 errors, only the pre-existing `src/overlay.cpp(245)` C4244
warning; `LOAD_OK base=0x72420000`; no `loaded.log`. Both offline checks
pass (MD5 vectors; scan resolves function RVA `0x1ACDB0`, seed RVA
`0x6BF868`).

## Public API

The original and the rebuild have **zero exports**: this is an injected
module, not a library. The interface surface is:

| Entry | Calling convention | Location |
|---|---|---|
| `DllMain` | `__stdcall` | `src/dllmain.cpp:36` |
| `CheatThread_Main` | `__stdcall` thread | `src/dllmain.cpp:71` |
| `Hook_EndScene` | `__stdcall` MinHook target | `src/hooks.cpp:692` |
| `Hook_EngineClient_Slot20` | `__stdcall`, CUserCmd in ESI, caller EBP quirk | `src/hooks.cpp:144` |
| `Hook_PaintTraverse` | `__fastcall` VMT detour | `src/hooks.cpp:168` |
| `Hook_DrawModelExecute` | `__stdcall` VMT detour | `src/hooks.cpp:322` |
| `WndProc_Hook` | `__stdcall` | `src/hooks.cpp:500` |

Recovered layouts/types are in `include/`: `cusercmd.h` (64-byte CUserCmd),
`interfaces.h` (engine interfaces, CGlobalVars), `netprop.h`, `recovered.h`
(settings/runtime globals), `vmt.h` (12-byte VmtHook), `nospread.h`,
`nospread_scan.h`, `md5.h`.

## Layout

- `src/` - recovered implementation: dllmain, hooks, interfaces, vmt,
  netprop, menu, cheat, esp, radar, overlay, utils, md5, nospread,
  nospread_scan
- `include/` - declarations, layouts, address comments
- `third_party/` - ImGui 1.74, MinHook 1.3.3, Kiero 1.2.9
- `tools/` - build/load/offline checks, IDA dump helpers,
  `gen_ledger.py`, `scan_client_nospread.py`
- `tests/` - empty (no automated suite)
- `NOTES/` - `inferences.md` (INF-001..INF-035), `ida-naming.md`,
  `structural-metrics.md`, `verification.md`, `ida/` dumps and the
  function inventory
- `ledger.csv`, `ledger.json`, `tests.md`, `CHANGELOG.md`, `STATE.md`

## Verification harness

1. Build with `tools\build.bat`; copy `build\Release\VR.dll` to a local
   injection folder (for example `staging\rebuilt\VR.dll`, outside this
   repository).
2. Inject the built DLL into L4D2 with a local injector and compare against
   the original DLL. Both the injector and the oracle DLL are supplied
   locally and are not part of this repository.
3. Automated/local checks: `tools\loadtest.bat` (load only),
   `tools\md5_check.bat` (seed vectors), `tools\nospread_scan_check.bat`
   (maps the installed `client.dll` and runs the real scan),
   `tools\scan_client_nospread.py` (independent scan replay).
4. Structural comparison: build with `/MAP`; parse `build\Release\VR.map`
   and compare function sizes with the targets in
   `NOTES/structural-metrics.md`. There is no `check.py`.
5. Test chains and historical failures: `tests.md`.

## Test status

22/22 tests PASS (4 runnable tools, 18 live inject-and-compare groups), 0
current failures or flaky tests. Historical failures and their causes are
in `tests.md`.

## Known limitations

### BLOCKED (4)

1. Rage SMAC-mode angle prediction, `0x100064C9..0x100066B5`.
2. Post-Auto-Shove anti-aim movement, `0x100066C9..0x100067AA`.
3. Melee weapon-id-19 Rage path, `0x100067B7..0x10006A02`.
4. `sub_10004190` movement normalization when anti-aim is off.

These are off at default settings except the melee path, which affects melee
weapons only. Ledger rows `F025`, `F047`-`F049`.

### INFERRED names/types

17 ledger rows are INFERRED, including: `InitInterfaces` slot 1
(GetGlobalVars), `GetEntityRenderableModelId`, config loader/saver wiring and
keys, menu fixed size, `CGlobalVars` partial layout, `g_MenuState` partial
layout, `LocalWeaponHasClip` fallback, and the safety guards
`ReadPointer`/`IsBboxSane`/varargs text handling. All are flagged in source
with `// INFERRED`.

### Preserved quirks, bugs and undefined-behavior dependencies

- Triggerbot: Delay 0 never fires; Delay 1-15 yields threshold 0 and fires
  immediately; limb flag has no writer, so hitgroups 4-7 never match.
- Invisible Jockey: inverted `0.0 > distance` guard makes the write
  unreachable; ported as-is.
- Auto Pistol and Server Lagger flags are never read; UI/config-only.
- PaintTraverse speed text and the Monitor panel gates have no writers.
- Legit Auto Shove is geometrically broken; Teleport, Air Stuck and Legit
  No-Recoil are inert in the original.
- Legit LOS accepts `CBreakableSurface` by checking the candidate, not the
  hit entity.
- Radar's orphan mouse-drag block is intentionally not ported; EndScene
  overwrites those globals every frame.
- Menu owns keyboard/mouse while open; confirmed original behavior.

### Intentional deviations (failure paths only)

- `IsBboxSane` rejects non-finite/absurd box coordinates; the original can
  pass INT_MIN into text and crash.
- `ReadPointer` and interface/model/bone guards skip instead of walking
  null.
- `LocalWeaponHasClip` falls back to true if netprops cannot resolve.
- No-Spread skips on scan failure instead of calling/dereferencing null.
- No-Spread validates the seed address lies inside `client.dll`.
Successful paths match the target.

### Not byte/structure-identical

Size deltas are recorded in `NOTES/structural-metrics.md`:
`InitInterfaces` 0x1D2 vs 0x207; `FindPattern` 0x18F vs 0x1C3;
`ConsolePrint` 0x30 vs 0x2A; `Legit_Triggerbot` 0x3A0 vs 0x341;
`Legit_Aimbot` 0xD00 vs 0xC71; `Rage_Aimbot` 0xEB0 vs 0xE08;
`Rage_AutoShove` 0x5B0 vs 0x5CD; `Cheat_Update` 0x1380 vs 0x20EA
(BLOCKED blocks missing). `MD5_PseudoRandom` is a loop (0x1B0) instead of
the target's unrolled transform (0xE2 + 0x67A), verified by vectors.

### Leftover scan over src/ and include/

- `goto LABEL_`: 0 hits.
- `LODWORD`: 0 hits.
- `v1`/`v2`-style identifiers: 0 hits.
- `__int64`: 2 hits, `src/cheat.cpp:237,239` (QPC millisecond arithmetic;
  intentional Windows type).
- `sub_XXXX`: 142 hits, all in comments documenting provenance; none are
  code identifiers.

## Maintenance guide

### Add a recovered function

1. Recover/decompile it in IDA; record the original address and evidence in
   `NOTES/inferences.md`.
2. Add the port with an address comment and `// INFERRED` if the name,
   signature or layout is inferred.
3. Rebuild clean (`tools\build.bat`) and measure against
   `NOTES/structural-metrics.md`.
4. Add a test chain to `tests.md` and a ledger override to
   `tools/gen_ledger.py`; run `python tools/gen_ledger.py`.
5. Update `STATE.md`.

### Rename safely

Check all xrefs in IDA and the ledger before renaming. Update the source
comment, the ledger override, evidence, and any `// INFERRED` note in the
same change. Rebuild and rerun the offline checks. Names in this project are
chosen from strings, vtable slots, caller usage, unwind data, or live
behavior; do not rename without that evidence.

### Re-run verification

`tools\build.bat` -> `tools\loadtest.bat` -> `tools\md5_check.bat` ->
`tools\nospread_scan_check.bat` -> inject the built DLL from a local staging folder and
compare against the original DLL (supplied locally; not in this
repository). Regenerate counts after any ledger
change with `python tools/gen_ledger.py`.

## One-screen summary

- Functions matched/total: **126/166** ledger function rows mapped to
  **82 distinct** original addresses/blocks; 40 helper rows unmapped.
- Tests passed/total: **22/22**; 0 current failing/flaky.
- BLOCKED: **4** (SMAC prediction, post-shove anti-aim, melee id-19,
  sub_10004190 normalization).
- INFERRED: **17** ledger rows (**18** source markers).
- Riskiest for manual review:
  1. No-Spread runtime scan and indirect `SharedRandomFloat` call
     (validated offline against this client.dll only).
  2. Slot-20 hook ABI (ESI=CUserCmd, caller-EBP command check) and nested
     SetViewAngles behavior.
  3. Triggerbot/aimbot trace ABI and silent/perfect-silent state.
  4. BLOCKED Rage SMAC/anti-aim/melee paths for non-default settings.
  5. Safety deviations vs the original's unguarded paths (ESP text bounds,
     pointer guards, netprop fallback, No-Spread scan-failure skip).
