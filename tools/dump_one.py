# IDA batch script: dump disassembly (+ pseudocode if available) of one symbol.
# Usage: idat -A -S"dump_one.py <symbol>" <binary>
import sys
import ida_auto
import ida_funcs
import ida_hexrays
import idautils
import idc

ida_auto.auto_wait()

symbol = sys.argv[1] if len(sys.argv) > 1 else "?InitInterfaces@@YAXXZ"
base = r"C:\Users\Ahmad\Desktop\old_dll\build\Release"
ea = idc.get_name_ea_simple(symbol)
if ea == idc.BADADDR:
    # Fall back to a substring match.
    for f in idautils.Functions():
        n = idc.get_func_name(f)
        if symbol in n:
            ea = f
            break
f = ida_funcs.get_func(ea)
with open(base + r"\one.asm", "w", encoding="utf-8") as out:
    out.write(f"# {idc.get_func_name(f.start_ea)} {f.start_ea:08X} size={f.size():X}\n")
    for item in idautils.FuncItems(f.start_ea):
        out.write(f"{item:08X} {idc.GetDisasm(item)}\n")
try:
    cf = ida_hexrays.decompile(f.start_ea)
    with open(base + r"\one.c", "w", encoding="utf-8") as out:
        out.write(str(cf) if cf else "// decompile failed\n")
except Exception as exc:
    with open(base + r"\one.c", "w", encoding="utf-8") as out:
        out.write(f"// hexrays unavailable: {exc}\n")
idc.qexit(0)
