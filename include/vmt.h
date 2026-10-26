// Virtual-method-table hook helper.
//
// Reconstructed from VmtHook_Init (0x10007750). IDA-verified layout:
//   sizeof(VmtHook) == 12, offsets: iface 0, origVtable 4, copyVtable 8.
// The copied table reserves one leading dword for the RTTI complete-object
// locator (copied from origVtable[-1]); the interface's vtable pointer is then
// redirected to copyVtable + 1.
#pragma once

class VmtHook {
public:
    explicit VmtHook(void *interfacePointer);

    void **entry(int index) { return &copyVtable[1 + index]; }

    void *iface;
    void **origVtable;
    void **copyVtable;
};
