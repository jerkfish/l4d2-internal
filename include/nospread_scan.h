// Runtime scan shared by the No-Spread blocks. Reproduces the target's
// client.dll walk on an arbitrary mapped image (base + SizeOfImage):
// "SharedRandomFloat" -> address reference -> function prologue -> first
// 0xA1 immediate (seed global). Used by src/nospread.cpp and by the offline
// verifier tools/nospread_scan_check.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

struct NoSpreadScanResult {
    void *function;    // resolved client.dll function entry
    uint32_t *seed;    // random-seed global inside the module
};

// Returns false if any step of the target's scan fails. The caller must
// still validate that seed points inside the module if it dereferences it.
bool NoSpread_ScanModule(const void *moduleBase, std::size_t moduleSize,
                         NoSpreadScanResult *result);
