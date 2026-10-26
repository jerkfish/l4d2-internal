"""Offline replay of the No-Spread runtime scan against a client.dll file.

Reproduces the target's scan on a mapped-image view (sections placed at
their virtual addresses) without loading the DLL:
  1. find "SharedRandomFloat" + NUL
  2. walk backwards to the dword holding its VA
  3. walk backwards to the masked function prologue
  4. find the first 0xA1 within 0x100 bytes -> seed global VA
Optionally disassembles the resolved function (capstone required).
"""

import struct
import sys

IMAGE_BASE_OFFSET = 28
IMAGE_SIZE_OFFSET = 56


def parse_pe(data):
    if data[:2] != b"MZ":
        raise SystemExit("not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("missing PE signature")
    coff = pe + 4
    nsections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10B:
        raise SystemExit("not a PE32 image")
    image_base = struct.unpack_from("<I", data, opt + IMAGE_BASE_OFFSET)[0]
    size_of_image = struct.unpack_from("<I", data, opt + IMAGE_SIZE_OFFSET)[0]
    sections = []
    sec = opt + opt_size
    for i in range(nsections):
        name = data[sec:sec + 8].rstrip(b"\0").decode("latin-1")
        vsize, va, raw_size, raw_ptr = struct.unpack_from(
            "<IIII", data, sec + 8)
        sections.append((name, va, vsize, raw_ptr, raw_size))
        sec += 40
    return image_base, size_of_image, sections


def mapped_image(data, image_base, size_of_image, sections):
    image = bytearray(size_of_image)
    for name, va, vsize, raw_ptr, raw_size in sections:
        if va >= size_of_image:
            continue
        copy_size = min(vsize or raw_size, raw_size, size_of_image - va)
        image[va:va + copy_size] = data[raw_ptr:raw_ptr + copy_size]
    return image


def find_literal(image):
    return image.find(b"SharedRandomFloat\0")


def find_reference(image, literal_rva, literal_va, image_base):
    # The target starts at literal+1 and walks down.
    for p in range(literal_rva + 1, -1, -1):
        if image[p:p + 4] == struct.pack("<I", literal_va):
            return p
    return None


def find_function_start(image, reference_rva):
    for p in range(reference_rva, -1, -1):
        if ((image[p] & 0xF0) == 0x50 and
                (image[p + 1] & 0xF0) == 0x80 and
                (image[p + 2] & 0x0F) == 0x0C):
            return p
    return None


def find_seed(image, function_rva):
    for i in range(0x100):
        if image[function_rva + i] == 0xA1:
            seed_va = struct.unpack_from("<I", image, function_rva + i + 1)[0]
            return i, seed_va
    return None, None


def section_for(sections, rva):
    for name, va, vsize, raw_ptr, raw_size in sections:
        if va <= rva < va + max(vsize, raw_size):
            return name
    return "?"


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else (
        r"C:\Program Files (x86)\Steam\steamapps\common"
        r"\Left 4 Dead 2\left4dead2\bin\client.dll")
    data = open(path, "rb").read()
    image_base, size_of_image, sections = parse_pe(data)
    image = mapped_image(data, image_base, size_of_image, sections)

    print(f"file       : {path}")
    print(f"image base : 0x{image_base:08X}")
    print(f"image size : 0x{size_of_image:X}")

    literal_rva = find_literal(image)
    if literal_rva < 0:
        print("SharedRandomFloat: NOT FOUND")
        return 1
    literal_va = image_base + literal_rva
    print(f"literal    : RVA 0x{literal_rva:X} ({section_for(sections, literal_rva)}), "
          f"VA 0x{literal_va:08X}")

    reference_rva = find_reference(image, literal_rva, literal_va, image_base)
    if reference_rva is None:
        print("reference  : NOT FOUND")
        return 1
    print(f"reference  : RVA 0x{reference_rva:X} ({section_for(sections, reference_rva)}), "
          f"distance -0x{literal_rva - reference_rva:X}")

    function_rva = find_function_start(image, reference_rva)
    if function_rva is None:
        print("prologue   : NOT FOUND")
        return 1
    print(f"prologue   : RVA 0x{function_rva:X} ({section_for(sections, function_rva)}), "
          f"bytes {image[function_rva:function_rva + 8].hex(' ')}")
    print(f"           : -0x{reference_rva - function_rva:X} before reference")

    a1_offset, seed_va = find_seed(image, function_rva)
    if seed_va is None:
        print("seed A1    : NOT FOUND")
        return 1
    seed_rva = seed_va - image_base
    inside = 0 <= seed_rva < size_of_image
    print(f"seed A1    : function+0x{a1_offset:X}, immediate 0x{seed_va:08X} "
          f"({section_for(sections, seed_rva) if inside else 'OUTSIDE MODULE'}, "
          f"RVA 0x{seed_rva:X})")

    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    except ImportError:
        print("capstone not installed; skipping disassembly")
        return 0

    print("\nfunction disassembly:")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    code = bytes(image[function_rva:function_rva + 0x100])
    count = 0
    for insn in md.disasm(code, image_base + function_rva):
        marker = "  <-- A1 seed" if insn.address == image_base + function_rva + a1_offset else ""
        print(f"  {insn.address:08X}: {insn.bytes.hex():<16} "
              f"{insn.mnemonic:<8} {insn.op_str}{marker}")
        count += 1
        if count >= 60 or insn.mnemonic == "ret":
            break
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
