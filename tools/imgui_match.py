#!/usr/bin/env python3
"""Match rebuilt ImGui functions (PDB names) to target sub_* addresses by
normalized instruction sequences.

Normalization masks absolute addresses/branch targets but keeps mnemonics,
registers, stack/struct displacements and small constants.
"""
import json
import re
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

BASE = 0x10000000
MD = Cs(CS_ARCH_X86, CS_MODE_32)
MD.detail = False

HEX = re.compile(r"0x[0-9a-fA-F]+")


def norm(op_str: str, mnemonic: str) -> str:
    def repl(m):
        v = int(m.group(0), 16)
        if mnemonic.startswith(("j", "call", "loop")) or v >= 0x10000:
            return "A"
        return m.group(0)

    s = HEX.sub(repl, op_str)
    s = s.replace("dword ptr ", "").replace("word ptr ", "").replace("byte ptr ", "")
    s = re.sub(r"\s+", " ", s).strip()
    return mnemonic + " " + s


def disasm_func(pe, ea, size):
    try:
        off = pe.get_offset_from_rva(ea - BASE)
        data = pe.__data__[off:off + size]
    except Exception:
        return None
    out = []
    for insn in MD.disasm(data, ea):
        out.append(norm(insn.op_str, insn.mnemonic))
    return out


def main():
    tgt_pe = pefile.PE(r"original\target.dll", fast_load=True)
    our_pe = pefile.PE(r"build\Release\VR.dll", fast_load=True)

    tgt = json.load(open(r"NOTES\ida\functions.json", encoding="utf-8"))
    tgt = tgt.get("result", tgt)
    if isinstance(tgt, dict):
        tgt = tgt["data"]
    tgt_fns = [(int(f["address"], 16), int(f["size"], 16), f["name"]) for f in tgt]

    our_fns = []
    for line in open(r"build\Release\functions.txt", encoding="utf-8"):
        p = line.split(None, 2)
        if len(p) == 3:
            our_fns.append((int(p[0], 16), int(p[1], 16), p[2]))

    def key_of(seq):
        return (len(seq), "\n".join(seq)) if seq else None

    tgt_map = {}
    for ea, size, name in tgt_fns:
        seq = disasm_func(tgt_pe, ea, size)
        k = key_of(seq)
        if k:
            tgt_map.setdefault(k, []).append((ea, name, size))

    matched = {}
    for ea, size, name in our_fns:
        if "ImGui@@" not in name:
            continue
        seq = disasm_func(our_pe, ea, size)
        k = key_of(seq)
        if not k or k not in tgt_map:
            continue
        # Prefer an unambiguous target in the ImGui address range.
        cands = tgt_map[k]
        pref = [c for c in cands if 0x1000E360 <= c[0] < 0x10032AD0]
        pick = (pref or cands)[0]
        matched[name] = {"our_ea": hex(ea), "our_size": hex(size),
                         "target_ea": hex(pick[0]), "target_name": pick[1],
                         "target_size": hex(pick[2]), "ambig": len(cands)}

    print("matched", len(matched), "ImGui functions")
    for key in ("?NewFrame@", "?EndFrame@", "?Render@ImGui", "?Begin@ImGui",
                "?End@ImGui", "?Checkbox@", "?SliderFloat@", "?Button@",
                "?RenderDrawData", "?CreateContext@", "?GetIO@"):
        for name, m in matched.items():
            if key in name:
                print(f"  {name[:56]:56s} -> target {m['target_ea']} "
                      f"(ours {m['our_ea']}, {m['target_name']}, ambig {m['ambig']})")
                break
    json.dump(matched, open(r"NOTES\ida\imgui_match.json", "w"), indent=1)
    print("saved NOTES/ida/imgui_match.json")


if __name__ == "__main__":
    sys.exit(main())
