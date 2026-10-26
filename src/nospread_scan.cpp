// Exact port of the target's client.dll scan (sub_10037890, sub_100378E0 and
// the inline prologue walk at 0x10007006). Bounds are tightened to the
// module image; the target walks to address 0 instead.
#include "nospread_scan.h"

#include <windows.h>

#include <cstring>

namespace {

const uint8_t *FindSharedRandomFloat(const uint8_t *base, size_t size) {
    static const char kLiteral[] = "SharedRandomFloat";
    const size_t kLength = sizeof(kLiteral); // includes the terminator

    __try {
        const uint8_t *end = base + size;
        for (const uint8_t *p = base; p + kLength <= end; ++p) {
            if (std::memcmp(p, kLiteral, kLength) == 0)
                return p;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

const uint8_t *FindStringReference(const uint8_t *start, const uint8_t *base,
                                   const uint8_t *literal) {
    const uint32_t target =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(literal));
    const uintptr_t baseValue = reinterpret_cast<uintptr_t>(base);

    __try {
        for (uintptr_t p = reinterpret_cast<uintptr_t>(start);
             p >= baseValue; --p) {
            uint32_t candidate;
            std::memcpy(&candidate, reinterpret_cast<const void *>(p),
                        sizeof(candidate));
            if (candidate == target)
                return reinterpret_cast<const uint8_t *>(p);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

const uint8_t *FindFunctionStart(const uint8_t *reference,
                                 const uint8_t *base) {
    const uintptr_t baseValue = reinterpret_cast<uintptr_t>(base);

    __try {
        for (uintptr_t p = reinterpret_cast<uintptr_t>(reference);
             p >= baseValue; --p) {
            const uint8_t *bytes = reinterpret_cast<const uint8_t *>(p);
            if ((bytes[0] & 0xF0) == 0x50 && (bytes[1] & 0xF0) == 0x80 &&
                (bytes[2] & 0x0F) == 0x0C)
                return bytes;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

uint32_t *FindSeedGlobal(const uint8_t *function) {
    __try {
        for (size_t i = 0; i < 0x100; ++i) {
            if (function[i] != 0xA1)
                continue;
            uint32_t address;
            std::memcpy(&address, function + i + 1, sizeof(address));
            return reinterpret_cast<uint32_t *>(
                static_cast<uintptr_t>(address));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

} // namespace

bool NoSpread_ScanModule(const void *moduleBase, std::size_t moduleSize,
                         NoSpreadScanResult *result) {
    if (!moduleBase || !result || moduleSize == 0)
        return false;

    const uint8_t *base = static_cast<const uint8_t *>(moduleBase);
    const uint8_t *literal = FindSharedRandomFloat(base, moduleSize);
    if (!literal)
        return false;

    const uint8_t *reference = FindStringReference(literal + 1, base, literal);
    if (!reference)
        return false;

    const uint8_t *function = FindFunctionStart(reference, base);
    if (!function)
        return false;

    uint32_t *seed = FindSeedGlobal(function);
    if (!seed)
        return false;

    // The target does not validate; it dereferences whatever the imm32
    // holds. We require it to point back into the scanned module.
    const uintptr_t seedValue = reinterpret_cast<uintptr_t>(seed);
    const uintptr_t baseValue = reinterpret_cast<uintptr_t>(base);
    if (seedValue < baseValue ||
        seedValue + sizeof(uint32_t) > baseValue + moduleSize)
        return false;

    result->function = const_cast<uint8_t *>(function);
    result->seed = seed;
    return true;
}
