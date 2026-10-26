# IDA batch script: dump "EA SIZE NAME" for every function.
# Used to compare rebuilt function sizes against original/target.dll.
import ida_auto
import idautils
import ida_funcs
import idc

ida_auto.auto_wait()

out_path = r"C:\Users\Ahmad\Desktop\old_dll\build\Release\functions.txt"
with open(out_path, "w", encoding="utf-8") as out:
    for ea in idautils.Functions():
        f = ida_funcs.get_func(ea)
        if f:
            out.write(f"{f.start_ea:08X} {f.size():X} {idc.get_func_name(f.start_ea)}\n")
idc.qexit(0)
