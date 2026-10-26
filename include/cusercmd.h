// CUserCmd layout proven from Cheat_Update (0x100055B0) field accesses.
// No vtable and no aimdirection in this engine revision.
#pragma once

struct CUserCmd {
    int crc;                    // 0
    int command_number;         // 4
    int tick_count;             // 8
    float viewangles[3];        // 12,16,20
    float forwardmove;          // 24
    float sidemove;             // 28
    float upmove;               // 32
    int buttons;                // 36
    unsigned char impulse;      // 40
    unsigned char pad[3];       // 41..43
    int weaponselect;           // 44
    int weaponsubtype;          // 48
    int random_seed;            // 52
    short mousedx;              // 56
    short mousedy;              // 58
    bool hasbeenpredicted;      // 60
};
static_assert(sizeof(CUserCmd) == 64, "CUserCmd layout");
