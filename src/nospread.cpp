// No-Spread: faithful port of Cheat_Update 0x10006F81..0x1000712A (Rage)
// and 0x100073BA..0x1000757C (Legit).
//
// The target resolves the engine helper on first use:
//   1. GetModuleHandleA("client"), scan module memory for the
//      "SharedRandomFloat" literal (nospread_scan.cpp).
//   2. Scan backwards from the literal for the dword that holds its address
//      (the instruction operand inside the client.dll function).
//   3. Scan backwards from that operand for the function prologue
//      (55 8B EC, matched with the target's masks 0x50/0x80/0xC).
//   4. Scan the first 0x100 bytes of the function for the first 0xA1
//      (mov eax, [imm32]) and treat the imm32 as the random-seed global.
// The target performs this twice (one static pair for Rage, one for Legit).
// We do the same via function-local statics. On scan failure the target
// would call/dereference null; we skip instead (documented safety
// deviation).
#include "nospread.h"

#include "cusercmd.h"
#include "md5.h"
#include "netprop.h"
#include "nospread_scan.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

// Defined in cheat.cpp (sub_10004790).
bool Legit_WeaponAllowed(int weaponId);

namespace {

typedef float(__cdecl *SharedRandomFloatFn)(const char *, float, float, int);

struct ResolverState {
    SharedRandomFloatFn fn;
    uint32_t *seed;
};

size_t GetModuleImageSize(HMODULE module) {
    if (!module)
        return 0;

    __try {
        const uint8_t *base = reinterpret_cast<const uint8_t *>(module);
        const IMAGE_DOS_HEADER *dos =
            reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return 0;
        const IMAGE_NT_HEADERS32 *nt =
            reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base +
                                                         dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return 0;
        if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            return 0;
        return nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

ResolverState ResolveClientSharedRandomFloat() {
    ResolverState result = {nullptr, nullptr};

    HMODULE client = GetModuleHandleA("client");
    const size_t size = GetModuleImageSize(client);
    if (!client || size == 0)
        return result;

    NoSpreadScanResult scan = {};
    if (!NoSpread_ScanModule(client, size, &scan))
        return result;

    result.fn =
        reinterpret_cast<SharedRandomFloatFn>(scan.function);
    result.seed = scan.seed;
    return result;
}

const ResolverState &RageResolver() {
    static const ResolverState state = ResolveClientSharedRandomFloat();
    return state;
}

const ResolverState &LegitResolver() {
    static const ResolverState state = ResolveClientSharedRandomFloat();
    return state;
}

void ApplyNoSpread(CUserCmd *cmd, void *local, void *cachedWeapon,
                   const ResolverState &resolver) {
    if (!cmd || !local || !cachedWeapon)
        return;

    // Rage: sub_10004790(v162); Legit: the same call on the cached weapon.
    if (!Legit_WeaponAllowed(GetEntityWeaponId(cachedWeapon)))
        return;
    if ((cmd->buttons & 1) == 0)
        return;

    if (!resolver.fn || !resolver.seed)
        return;

    // The target resolves the weapon again for the spread read.
    void *weapon = GetLocalActiveWeapon(local);
    if (!weapon)
        return;

    uint8_t *spreadAddress = static_cast<uint8_t *>(weapon) + 0xD0C;
    if (IsBadReadPtr(spreadAddress, sizeof(float)))
        return;

    const uint32_t seed = MD5_PseudoRandom(
                              static_cast<uint32_t>(cmd->command_number)) &
                          0x7FFFFFFFu;
    *resolver.seed = seed;

    float spread;
    std::memcpy(&spread, spreadAddress, sizeof(spread));

    const float horizontal = resolver.fn(
        "CTerrorGun::FireBullet HorizSpread", -spread, spread, 0);
    cmd->viewangles[0] += horizontal * -1.0f;

    const float vertical = resolver.fn(
        "CTerrorGun::FireBullet VertSpread", -spread, spread, 0);
    cmd->viewangles[1] += vertical * -1.0f;
}

} // namespace

void NoSpread_Rage(CUserCmd *cmd, void *local, void *cachedWeapon) {
    ApplyNoSpread(cmd, local, cachedWeapon, RageResolver());
}

void NoSpread_Legit(CUserCmd *cmd, void *local, void *cachedWeapon) {
    ApplyNoSpread(cmd, local, cachedWeapon, LegitResolver());
}
