# IDA analysis directory

This publishable copy keeps only the generated function inventories:

- `functions.tsv` - demangled project symbols from the build map
- `functions_lines.tsv` - the same inventory with source definition lines

`tools/gen_ledger.py` reads `functions_lines.tsv`.

Raw IDA/decompiler snapshots (`Cheat_Update.c`, `sub_*.c`, `*.json`,
`*.asm.json`, `target_disasm.txt`, `imgui/`) are intentionally excluded.
They are large and contain third-party decompiled/disassembled code; they
remain in the local working tree only. The textual findings and addresses
are preserved in `../inferences.md`, `../ida-naming.md`,
`../structural-metrics.md` and `ledger.csv`/`ledger.json`.
