#!/usr/bin/env python3
"""Reproducible Phase 0 PE recon for original/target.dll.

Usage:
    uv run --with pefile --with capstone tools/pe_recon.py [path]

Prints: headers, sections, data directories, exports/imports, TLS, load
config, Rich-header decode, CodeView/PDB path, version resource, RTTI strings,
and an entry-point disassembly.
"""

from __future__ import annotations

import datetime as _dt
import re
import struct
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

DEFAULT_PATH = r"original\target.dll"


def hx(value: int) -> str:
    return f"0x{value:X}"


def print_headers(pe: pefile.PE) -> None:
    fh, oh = pe.FILE_HEADER, pe.OPTIONAL_HEADER
    ts = _dt.datetime.fromtimestamp(fh.TimeDateStamp, _dt.UTC)
    print("== Headers ==")
    print(f"Machine:            {hx(fh.Machine)} (0x14C=i386, 0x8664=x64)")
    print(f"Magic:              {hx(oh.Magic)} (0x10B=PE32, 0x20B=PE32+)")
    print(f"TimeDateStamp:      {hx(fh.TimeDateStamp)} = {ts:%Y-%m-%d %H:%M:%S} UTC")
    print(f"Characteristics:    {hx(fh.Characteristics)}")
    print(f"Linker version:     {oh.MajorLinkerVersion}.{oh.MinorLinkerVersion}")
    print(f"Subsystem:          {oh.Subsystem}")
    print(f"DllCharacteristics: {hx(oh.DllCharacteristics)}")
    print(f"ImageBase:          {hx(oh.ImageBase)}")
    print(f"SizeOfImage:        {hx(oh.SizeOfImage)}")
    print(f"EntryPoint RVA:     {hx(oh.AddressOfEntryPoint)}")
    print("Sections:")
    for s in pe.sections:
        name = s.Name.rstrip(b"\0").decode("ascii", "replace")
        print(
            f"  {name:8s} VA={hx(s.VirtualAddress):>10s} "
            f"vsize={hx(s.Misc_VirtualSize):>10s} "
            f"raw={hx(s.PointerToRawData):>10s} "
            f"rsize={hx(s.SizeOfRawData):>10s} "
            f"flags={hx(s.Characteristics):>10s}"
        )


def print_directories(pe: pefile.PE) -> None:
    names = [
        "EXPORT", "IMPORT", "RESOURCE", "EXCEPTION", "SECURITY", "BASERELOC",
        "DEBUG", "ARCHITECTURE", "GLOBALPTR", "TLS", "LOAD_CONFIG",
        "BOUND_IMPORT", "IAT", "DELAY_IMPORT", "CLR", "RESERVED",
    ]
    print("\n== Data directories ==")
    for i, dd in enumerate(pe.OPTIONAL_HEADER.DATA_DIRECTORY):
        if dd.VirtualAddress or dd.Size:
            label = names[i] if i < len(names) else str(i)
            print(f"  {label:12s} RVA={hx(dd.VirtualAddress):>10s} size={hx(dd.Size)}")


def print_exports_imports(pe: pefile.PE) -> None:
    print("\n== Exports ==")
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        print("  none (no export directory)")
    else:
        exp = pe.DIRECTORY_ENTRY_EXPORT
        print(f"  DLL name: {exp.name}")
        for sym in exp.symbols:
            print(f"  {sym.ordinal:4d} {sym.name} {hx(sym.address)}")

    print("\n== Imports ==")
    if not hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        print("  none")
        return
    for entry in pe.DIRECTORY_ENTRY_IMPORT:
        names = [
            imp.name.decode("ascii", "replace")
            if imp.name
            else f"#{imp.ordinal}"
            for imp in entry.imports
        ]
        print(f"  {entry.dll.decode('ascii', 'replace')} ({len(names)})")
        print("    " + ", ".join(names))


def print_tls_loadcfg(pe: pefile.PE) -> None:
    print("\n== TLS ==")
    if not hasattr(pe, "DIRECTORY_ENTRY_TLS"):
        print("  no TLS directory")
    else:
        tls = pe.DIRECTORY_ENTRY_TLS
        va = tls.struct.AddressOfCallBacks
        print(f"  AddressOfCallBacks VA={hx(va)}")
        try:
            raw = pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, 32)
            dwords = struct.unpack("<8I", raw)
            print("  callback dwords:", [hx(v) for v in dwords])
            if not any(dwords):
                print("  -> no TLS callbacks (zero-filled array)")
        except Exception as exc:  # noqa: BLE001 - recon script
            print("  callback read failed:", exc)

    print("\n== Load config ==")
    if not hasattr(pe, "DIRECTORY_ENTRY_LOAD_CONFIG"):
        print("  none")
        return
    lc = pe.DIRECTORY_ENTRY_LOAD_CONFIG.struct
    print(f"  Size:            {lc.Size}")
    print(f"  SecurityCookie:  {hx(lc.SecurityCookie)}")
    print(f"  SEHandlerTable:  {hx(getattr(lc, 'SEHandlerTable', 0))}")
    print(f"  SEHandlerCount:  {getattr(lc, 'SEHandlerCount', 0)}")
    print(f"  GuardFlags:      {hx(getattr(lc, 'GuardFlags', 0))}")


def print_debug(pe: pefile.PE) -> None:
    print("\n== Debug directory ==")
    if not hasattr(pe, "DIRECTORY_ENTRY_DEBUG"):
        print("  none")
        return
    for dbg in pe.DIRECTORY_ENTRY_DEBUG:
        print(f"  type={dbg.struct.Type} size={dbg.struct.SizeOfData} "
              f"rva={hx(dbg.struct.AddressOfRawData)}")
        entry = getattr(dbg, "entry", None)
        if entry is not None and hasattr(entry, "PdbFileName"):
            print(f"    PDB: {entry.PdbFileName}")


def print_version(pe: pefile.PE) -> None:
    print("\n== Version resource ==")
    try:
        for file_info in pe.FileInfo:
            for item in file_info:
                if item.Key != b"StringFileInfo":
                    continue
                for table in item.StringTable:
                    for key, value in table.entries.items():
                        print(
                            f"  {key.decode(errors='replace')} = "
                            f"{value.decode(errors='replace')}"
                        )
    except Exception as exc:  # noqa: BLE001 - recon script
        print("  version parse failed:", exc)


def decode_rich(data: bytes) -> None:
    print("\n== Rich header ==")
    rich = data.find(b"Rich")
    if rich < 0:
        print("  not present")
        return
    key = struct.unpack_from("<I", data, rich + 4)[0]
    print(f"  offset={hx(rich)} key={hx(key)}")
    entries: list[int] = []
    pos = rich - 4
    while pos >= 0:
        value = struct.unpack_from("<I", data, pos)[0] ^ key
        entries.append(value)
        if value == 0x536E6144:  # DanS
            break
        pos -= 4
    entries = list(reversed(entries[:-1]))  # strip DanS, restore order
    print("  prod/build : count")
    # The stream is [pad, pad, pad, compid, count, ...] -> pair from index 3.
    for i in range(3, len(entries) - 1, 2):
        compid, count = entries[i], entries[i + 1]
        print(
            f"  {hx(compid >> 16):>6s}/{compid & 0xFFFF:<6d}: {count}"
        )


def print_strings_evidence(data: bytes) -> None:
    print("\n== RTTI type descriptors ==")
    for m in re.finditer(rb"\.\?A[VU][^\x00]{1,120}", data):
        print(f"  {m.start():08X}  {m.group().decode('latin1')}")

    print("\n== PDB / source paths ==")
    for m in re.finditer(rb"[ -~]{4,}\.pdb\b", data):
        print(f"  {m.start():08X}  {m.group().decode('latin1')}")

    print("\n== UTF-16LE module/interface strings ==")
    # Lookahead so matches can start at every offset. Keep 4-byte-aligned
    # starts that are not a continuation of the previous 4-byte chunk; this
    # suppresses suffix matches and the classic "ASCII terminator becomes a
    # UTF-16 char" overlap.
    def _is_u16_char(off: int) -> bool:
        return data[off + 1] == 0 and 32 <= data[off] < 127

    for m in re.finditer(rb"(?=((?:[ -~]\x00){4,}))", data):
        start = m.start()
        if start % 4:
            continue
        if start >= 4 and _is_u16_char(start - 4) and _is_u16_char(start - 2):
            continue
        text = m.group(1).decode("utf-16le")
        if re.search(r"(?i)\.dll$|^V[A-Za-z]+0\d\d$|PlayerInfoManager", text):
            print(f"  {start:08X}  {text}")


def print_entry(pe: pefile.PE, data: bytes) -> None:
    print("\n== Entry point ==")
    ep = pe.OPTIONAL_HEADER.AddressOfEntryPoint
    base = pe.OPTIONAL_HEADER.ImageBase
    code = pe.get_data(ep, 256)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for ins in md.disasm(code, base + ep):
        print(f"  {ins.address:08X}  {ins.bytes.hex():<18s} "
              f"{ins.mnemonic} {ins.op_str}")
        if ins.mnemonic.startswith("ret"):
            break


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PATH
    data = open(path, "rb").read()
    pe = pefile.PE(data=data)
    print(f"File: {path} ({len(data)} bytes)\n")
    print_headers(pe)
    print_directories(pe)
    print_exports_imports(pe)
    print_tls_loadcfg(pe)
    print_debug(pe)
    print_version(pe)
    decode_rich(data)
    print_strings_evidence(data)
    print_entry(pe, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
