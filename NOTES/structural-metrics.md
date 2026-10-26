# Structural metrics — recovered functions vs target

Sizes are MSVC `/O2 /GL` byte counts from `build/Release/VR.map` versus the
target's functions in `original/target.dll`. Differences come from guards,
inlining decisions, and target blocks that remain unported (listed in
`README.md`).

| Function | Rebuild | Target | Target address | Notes |
|---|---:|---:|---|---|
| `FindPattern` | 0x1C3 | 0x18F | 0x100376C0 | earlier measurement |
| `ConsolePrint` | 0x2A | 0x30 | 0x100042D0 | earlier measurement |
| `InitInterfaces` | 0x1D2 | 0x207 | — | earlier measurement |
| `Legit_Triggerbot` | 0x3A0 | 0x341 | 0x100353F0 | trace ABI identical; extra guards |
| `Legit_Aimbot` | 0xD00 | 0xC71 | 0x10035BF0 | shared helpers outlined |
| `Rage_Aimbot` | 0xEB0 | 0xE08 | 0x10003380 | shared helpers outlined |
| `Rage_AutoShove` | 0x5B0 | 0x5CD | 0x10002D90 | helper-free port |
| `Cheat_Update` | 0x1380 | 0x20EA | 0x100055B0 | Rage SMAC / melee blocks unported; No-Spread is in separate functions |
| `MD5_PseudoRandom` | 0x1B0 | 0xE2 + 0x67A | 0x10037080 + 0x10036A00 | target unrolls the transform; ours is loop-based |
| `NoSpread_ScanModule` | 0x80 | 0x45 + 0x37 + inline walk | 0x10037890 + 0x100378E0 | LTCG outlines the scan helpers; exact masks preserved |

Notes:

- `Rage_AutoShove` is closest (0x5B0 vs 0x5CD) because it is a self-contained
  function with the same filter order and arithmetic.
- `Cheat_Update` is smaller because the Rage SMAC-mode prediction block,
  the post-shove anti-aim movement adjustment, and the melee weapon-id-19
  path are not implemented (see README "BLOCKED"). The two No-Spread
  blocks live in `src/nospread.cpp`; their exact scan semantics are
  documented in INF-035.
- All measured functions were compiled with the matched toolchain; always do
  a clean build before measuring (`/GL` reuses stale IL otherwise).
