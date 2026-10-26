// VmtHook implementation, recovered from 0x10007750.
#include "vmt.h"

#include <cstdlib>
#include <cstring>

VmtHook::VmtHook(void *interfacePointer)
    : iface(interfacePointer), origVtable(nullptr), copyVtable(nullptr) {
    origVtable = *reinterpret_cast<void ***>(interfacePointer);

    unsigned int count = 0;
    while (origVtable[count])
        ++count;

    copyVtable = reinterpret_cast<void **>(
        malloc(sizeof(void *) * (count + 1)));
    memcpy(copyVtable + 1, origVtable, sizeof(void *) * count);
    copyVtable[0] = origVtable[-1];  // RTTI complete object locator
    *reinterpret_cast<void ***>(interfacePointer) = copyVtable + 1;
}
