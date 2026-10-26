// Per-tick cheat update, entered from the IVEngineClient slot-20 detour with
// the caller's CUserCmd. Recovered incrementally; only Bunny Hop is ported.
#include "cusercmd.h"
#include "interfaces.h"
#include "netprop.h"
#include "nospread.h"
#include "recovered.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

bool g_BunnyHopEnabled = false;  // byte_1004ABF0, auto-jump state machine
bool g_AutoStrafer = false;      // byte_1004ABF1, config-only strafe block
bool g_FovChangerEnabled = false;
int g_FovViewModel = 51;  // dword_1004AB90 default
int g_FovDesired = 51;    // dword_1004AB8C default

// Legit tab state (sub_1003A300 defaults are zero; dword_1004AB8C = 51).
bool g_LegitEnabled = false;            // byte_1004AB3C
int g_LegitAimKey = 0;                  // dword_1004AB40
int g_LegitAimHitbox = 0;               // dword_1004AB44
int g_LegitHitboxMode = 0;              // dword_1004AB48
bool g_LegitShotgunMode = false;        // byte_1004AB4C
bool g_LegitNoRecoil = false;           // byte_1004AB4D
bool g_LegitNoSpread = false;           // byte_1004AB4E
int g_LegitAimFov = 0;                  // dword_1004AB50
int g_LegitSmoothX = 0;                 // dword_1004AB54
int g_LegitSmoothY = 0;                 // dword_1004AB58
bool g_LegitPositionAdjustment = false; // byte_1004AB5C
bool g_LegitIgnoreWitch = false;        // byte_1004AB5D
bool g_LegitIgnoreTeam = false;         // byte_1004AB5E
bool g_LegitHitscan = false;            // byte_1004AB5F
bool g_LegitPrioritySpecial = false;    // byte_1004AB60
bool g_LegitAutoShove = false;          // byte_1004AB61
int g_LegitShoveFov = 0;                // dword_1004AB64
bool g_LegitFovCircle = false;          // byte_1004AB7A
bool g_LegitTriggerbot = false;         // byte_1004AFFC
int g_LegitTriggerKey = 0;              // dword_1004ABA4
int g_LegitTriggerDelay = 0;            // dword_1004B000
bool g_LegitAimHead = false;            // byte_1004AFFD
bool g_LegitAimBody = false;            // byte_1004AFFE
float g_LegitTriggerLimbs = 0.0f;       // dword_1004B004 (dead, no writer)
int g_LegitTriggerDelayCounter = 0;     // dword_1004B184
bool g_AspectRatioEnabled = false;  // byte_1004AB80
int g_AspectRatioValue = 1;         // dword_1004AB84 (sub_1003A300 default)
bool g_NoVisualRecoil = false;  // byte_1004AB97
bool g_PlayerChams = false;     // byte_1004ABE0
int g_PlayerChamR = 0;          // dword_1004AFD8 (color defaults not set by
int g_PlayerChamG = 0;          // dword_1004AFDC  the target initializer;
int g_PlayerChamB = 0;          // dword_1004AFE0  INFERRED: 0 until loaded)
bool g_WeaponChams = false;       // byte_1004ABEC
bool g_HandWeaponChams = false;   // byte_1004ABED
bool g_SurvivorChams = false;     // byte_1004ABEE (not used by this hook)
bool g_InfectedChams = false;     // byte_1004ABEF
bool g_HandChams = false;       // byte_1004ABE2
bool g_XQZChams = false;        // byte_1004ABE1
int g_HandChamR = 0;            // dword_1004AFE4 (color defaults not set by
int g_HandChamG = 0;            // dword_1004AFE8  the target initializer;
int g_HandChamB = 0;            // dword_1004AFEC  INFERRED: 0 until loaded)
bool g_RemoveVomit = false;     // byte_1004B008 low byte
bool g_RemoveFog = false;       // byte_1004ABDE
bool g_FullBright = false;      // byte_1004ABDF
bool g_NoFallDamage = false;    // byte_1004B00D
bool g_InstantWeaponSwitch = false;  // byte_1004B00B
bool g_InstantRevive = false;        // byte_1004B00C
bool g_ChatSpam = false;             // byte_1004AC14
int g_ChatSpamMode = 0;              // dword_1004AC18
bool g_NameStealer = false;          // byte_1004B015
bool g_LagExploit = false;           // byte_1004B03C
int g_LagFast = 0;                   // dword_1004B038 (sub_1003A300 default)
int g_LagKey = 0;                    // dword_1004B034 (sub_1003A300 default)
bool g_Indicators = false;           // byte_1004B06C
bool g_InvisibleJockey = false;      // byte_1004B014
bool g_AutoPistol = false;           // byte_1004AB7C
bool g_ServerLagger = false;         // byte_1004B03D
bool g_EspEnable = false;            // byte_1004ABCE
bool g_EspShowTeam = false;          // byte_1004ABCF
bool g_EspName = false;              // byte_1004ABD5
bool g_EspBox = false;               // byte_1004ABD2
bool g_EspDistance = false;          // byte_1004ABD7
bool g_EspHealth = false;            // byte_1004ABD8
bool g_EspHealthBar = false;         // byte_1004ABD9
bool g_EspWeapons = false;           // byte_1004ABDA
bool g_EspWeapons2 = false;          // byte_1004ABDB
bool g_EspAmmo = false;              // byte_1004ABDC
bool g_EspWitchRage = false;         // byte_1004ABD1
bool g_EspLines = false;             // byte_1004ABD3
bool g_EspBotTag = false;            // byte_1004ABD6
bool g_Radar = false;                // byte_1004AC30
bool g_RadarNames = false;           // byte_1004AC31
bool g_Thirdperson = false;          // byte_1004ABF2
int g_ThirdpersonKey = 0;            // dword_1004ABF4 (config-only keybind)
static bool g_ThirdpersonToggle = false;  // byte_1004B1EC
bool g_SpeedHack = false;            // byte_1004B024
bool g_AirStuck = false;             // byte_1004AC1D
bool g_Teleport = false;             // byte_1004AC1C
bool g_FakeLag = false;              // byte_1004AC0E (PENDING slot-20 hack)
int g_TeleportKey = 0;               // dword_1004ABAC (config "TeleportKey")
int g_AirStuckKey = 0;               // dword_1004ABA8 (config "AirstuckKey")
int g_SpeedHackValue = 0;            // dword_1004B02C (sub_1003A300 default)
int g_SpeedHackKey = 115;            // dword_1004B028 ('S', sub_1003A300)
int g_FakeLagValue = 14;             // dword_1004AC10 (sub_1003A300 default)
bool g_AntiSmoker = false;           // byte_1004B00E
int g_SmokerMode = 0;                // dword_1004B010 (sub_1003A300 default)
bool g_RageEnabled = false;     // byte_1004ABFC
bool g_SmacMode = false;        // word_1004AC0C low byte
int g_RagePitch = 0;            // dword_1004AC00
int g_RageRealYaw = 0;          // dword_1004AC04
int g_RageDesyncYaw = 0;        // dword_1004AC08
bool g_RageAimbotEnabled = false;      // byte_1004AB7B
int g_RageAimKey = 0;                  // dword_1004AB9C
int g_RageHitbox = 0;                  // dword_1004AB6C
bool g_RageAutoShoot = false;          // byte_1004ABC0
bool g_RageSilentAim = false;          // byte_1004ABC2
bool g_RagePerfectSilentAim = false;   // byte_1004ABC3
bool g_RageShotgunMode = false;        // byte_1004AB79
bool g_RageAutoShove = false;          // byte_1004ABC4
bool g_RageAutoMelee = false;          // byte_1004ABC5
int g_RageAimFov = 0;                  // dword_1004ABB0
bool g_RageNoRecoil = false;           // byte_1004ABBE
bool g_RageNoSpread = false;           // byte_1004ABBF
bool g_RagePositionAdjustment = false; // byte_1004AB7E
bool g_RagePredictionFix = false;      // byte_1004AB99
bool g_RagePrioritySpecial = false;    // byte_1004ABC1
bool g_RageHitscan = false;            // byte_1004AB78
bool g_RageIgnoreTeam = false;         // byte_1004ABBD
bool g_RageIgnoreWitch = false;        // byte_1004ABC6
int g_RageAimHitboxResolved = 0;       // dword_1004AB68 "AimHitbox"
int g_RageSmooth = 0;                  // dword_1004AB70 "Smooth"

// Legit/Rage aimbot shared runtime state (target .data; Rage sub_10003380
// writes the same health/name fields). Layout per target addresses:
//   eye 0x1004B1A8, view angles 0x1004B3D0, forward 0x1004B3E0,
//   target index 0x1004B188, best distance 0x1004B18C,
//   aim point 0x1004B190, stored point 0x1004B19C,
//   stored cmd angles 0x1004B1B4, health 0x1004B060, max health 0x1004B064,
//   class name pointer 0x1004B05C.
float g_AimEyePos[3] = {};
float g_AimForward[3] = {};
float g_AimViewAngles[3] = {};
float g_AimBestDistance = 99999.9f;
int g_AimTargetIndex = -1;
float g_AimPoint[3] = {};
float g_AimStoredPoint[3] = {};
float g_AimStoredCmdAngles[3] = {};
int g_AimTargetHealth = 0;
int g_AimTargetMaxHealth = 0;
const char *g_AimTargetClassName = nullptr;

// Rage aimbot runtime state (target .data). The health/name fields above are
// shared with the Legit aimbot.
//   eye 0x1004B120, view angles 0x1004B25C, forward 0x1004B248,
//   target index 0x1004B100, best distance 0x1004B104,
//   aim point 0x1004B108, stored point 0x1004B114,
//   stored cmd angles 0x1004B12C, has-target 0x1004B058,
//   attack-suppressed 0x1004B057, SMAC flags 0x1004A92C/0x1004A8E8.
float g_RageEyePos[3] = {};
float g_RageForward[3] = {};
float g_RageViewAngles[3] = {};
float g_RageBestDistance = 99999.9f;
int g_RageTargetIndex = -1;
float g_RageAimPoint[3] = {};
float g_RageStoredPoint[3] = {};
float g_RageStoredCmdAngles[3] = {};
bool g_RageHasTarget = false;
bool g_RageAttackSuppressed = false;
unsigned char g_RageSmacFlagA = 0;  // byte_1004A92C
unsigned char g_RageSmacFlagB = 0;  // byte_1004A8E8

// sub_10004790: weapon ids the aimbot/spread blocks accept. Kept at external
// linkage because nospread.cpp applies the same gate as the target.
bool Legit_WeaponAllowed(int weaponId) {
    switch (weaponId) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 26:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
        return true;
    default:
        return false;
    }
}

namespace {

// ConVar access through VEngineCvar007 (vtable slot 13 = FindVar,
// ConVar slot 15 = SetValue(int)), as in Cheat_Update 0x10005C17.
void SetConVarInt(const char *name, int value) {
    if (!g_pEngineCvar || IsBadReadPtr(g_pEngineCvar, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineCvar);
    if (IsBadReadPtr(vtable, sizeof(void *) * 14))
        return;

    typedef void *(__thiscall * FindVarFn)(void *, const char *);
    void *var = reinterpret_cast<FindVarFn>(vtable[13])(g_pEngineCvar, name);
    if (!var || IsBadReadPtr(var, sizeof(void *)))
        return;

    void **conVarVtable = *reinterpret_cast<void ***>(var);
    if (IsBadReadPtr(conVarVtable, sizeof(void *) * 16))
        return;

    typedef void(__thiscall * SetValueIntFn)(void *, int);
    reinterpret_cast<SetValueIntFn>(conVarVtable[15])(var, value);
}

// sub_100022B0: per-call-site cooldown timer. Returns true (and restarts) once
// `ms` milliseconds have elapsed; the target's static starts at zero, so the
// first call always fires.
bool TimerElapsedMs(LARGE_INTEGER *last, unsigned int ms) {
    LARGE_INTEGER now;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);

    if (last->QuadPart != 0) {
        __int64 elapsedMs =
            (now.QuadPart - last->QuadPart) * 1000 / freq.QuadPart;
        if (elapsedMs < static_cast<__int64>(ms))
            return false;
    }

    *last = now;
    return true;
}

// Target .data state: 0x1004B218 (jump request), 0x1004B1E0 (jump state),
// 0x1004A121 (AutoStrafer latch, initial value 1) and 0x1004A970 (Fake Lag
// choke counter, BSS zero).
uint8_t g_BhopRequest = 0;
uint8_t g_BhopState = 0;
bool g_AutoStrafeActive = true;
int g_FakeLagCounter = 0;

// 0x1004A97C: last weaponselect seen by Instant Weapon Switch (initial -1).
int g_LastWeaponSelect = -1;

// Target Legit gate sub_100046B0: the active weapon must have m_iClip1 > 0.
// Offsets are resolved once; on resolution failure we fall back to true so
// the ported Legit features remain testable (safety/INFERRED).
bool LocalWeaponHasClip(void *local) {
    static int handleOffset = -2;
    static int clipOffset = -2;
    if (handleOffset == -2) {
        handleOffset = NetpropOffsetInTable("DT_BaseCombatCharacter",
                                            "m_hActiveWeapon");
        clipOffset = NetpropOffsetInTable("DT_BaseCombatWeapon", "m_iClip1");
    }
    if (handleOffset <= 0x100 || handleOffset >= 0x4000 ||
        IsBadReadPtr(static_cast<char *>(local) + handleOffset, 4))
        return true;
    int handle = *reinterpret_cast<int *>(static_cast<char *>(local) +
                                          handleOffset);
    void *weapon = GetClientEntityFromHandle(handle);
    if (!weapon || IsBadReadPtr(weapon, 0x100))
        return false;
    if (clipOffset <= 0x100 || clipOffset >= 0x4000 ||
        IsBadReadPtr(static_cast<char *>(weapon) + clipOffset, 4))
        return true;
    return *reinterpret_cast<int *>(static_cast<char *>(weapon) + clipOffset) >
           0;
}

// VEngineClient013 vtable slot 74 (offset 0x128). Lag Exploit, Instant Weapon
// Switch, and Instant Revive add to the dword at result+8. The target calls
// it unguarded; the checks below only prevent a crash on a null/invalid
// interface and do not alter behavior when the engine is healthy.
void AddEngineSlot74Field8(int amount) {
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 75))
        return;

    typedef void *(__thiscall * Slot74Fn)(void *);
    void *target = reinterpret_cast<Slot74Fn>(vtable[74])(g_pEngineClient);
    if (!target ||
        IsBadReadPtr(static_cast<char *>(target) + 8, sizeof(int)))
        return;

    *reinterpret_cast<int *>(static_cast<char *>(target) + 8) += amount;
}

// 0x100057DA: IVEngineClient::ExecuteClientCmd (vtable slot 107 / 0x1AC),
// used by Chat Spam. Guards only reject an invalid interface.
void ExecuteClientCmd(const char *command) {
    if (!command || !g_pEngineClient ||
        IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 108))
        return;

    typedef void(__thiscall * ExecuteClientCmdFn)(void *, const char *);
    reinterpret_cast<ExecuteClientCmdFn>(vtable[107])(g_pEngineClient, command);
}

// IVEngineClient::GetPlayerInfo (vtable slot 8 / 0x20), used by Name Stealer.
bool GetPlayerInfo(int index, void *info) {
    if (!info || !g_pEngineClient ||
        IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 9))
        return false;

    typedef bool(__thiscall * GetPlayerInfoFn)(void *, int, void *);
    return reinterpret_cast<GetPlayerInfoFn>(vtable[8])(g_pEngineClient, index,
                                                        info);
}

// --- Legit triggerbot (sub_100353F0) -------------------------------------
// Trace ABI proven from the target's stack frame: Ray_t occupies 0x48 bytes
// (four 16-byte vectors, the world-axis pointer, then two flags) and the
// trace_t buffer occupies 0x58. In L4D2's trace_t the hitgroup sits at +0x44
// and the hit entity pointer at +0x4C; +0x48 is physicsbone/worldSurfaceIndex.
struct Ray_t {
    float start[3];
    float startW;
    float delta[3];
    float deltaW;
    float startOffset[3];
    float startOffsetW;
    float end[3];
    float endW;
    void *worldAxisTransform;
    bool isRay;
    bool isSwept;
    char pad[2];
};
static_assert(sizeof(Ray_t) == 0x48, "Ray_t layout");

struct TraceResult {
    float startpos[3];        // 0x00
    float endpos[3];          // 0x0C
    float planeNormal[3];     // 0x18
    float planeDist;          // 0x24
    unsigned char planeType;  // 0x28
    unsigned char signBits;   // 0x29
    unsigned char pad0[2];    // 0x2A
    float fraction;           // 0x2C (sub_100026E0 reads this)
    int contents;             // 0x30
    unsigned short dispFlags; // 0x34
    bool allSolid;            // 0x36
    bool startSolid;          // 0x37
    unsigned char unknown38[12]; // 0x38..0x43
    int hitgroup;             // 0x44
    int physicsBone;          // 0x48 (name INFERRED from the Source tail)
    void *entity;             // 0x4C
    unsigned char tail[8];    // 0x50..0x57
};
static_assert(sizeof(TraceResult) == 0x58, "trace_t layout");
static_assert(offsetof(TraceResult, fraction) == 0x2C, "trace fraction");
static_assert(offsetof(TraceResult, hitgroup) == 0x44, "trace hitgroup");
static_assert(offsetof(TraceResult, entity) == 0x4C, "trace entity");

// Target ITraceFilter at 0x10041538: slot 0 returns entity != stored local,
// slot 1 returns TRACE_EVERYTHING (0).
struct TriggerTraceFilter {
    void *skipEntity;  // +4, matching the target's filter object
    explicit TriggerTraceFilter(void *entity) : skipEntity(entity) {}
    virtual bool ShouldHitEntity(void *entity, int /*contentsMask*/) {
        return entity != skipEntity;
    }
    virtual int GetTraceType() const { return 0; }
};

// 0x100353F0: trace from the local eye position along the command view
// angles with mask 0x46004003; if the hit entity passes the player filter
// (sub_10001EC0, shared with Player Chams) and is not a survivor, press
// IN_ATTACK when the hitgroup matches the Triggerbot selection.
// Target quirks preserved: Delay 0 never fires; Delay 1..15 yields a tick
// threshold of 0 (Delay/30) and therefore fires immediately; the limb flag
// dword_1004B004 has no writer, so hitgroups 4..7 are never accepted.
void Legit_Triggerbot(CUserCmd *cmd, void *local) {
    if (!cmd || !local)
        return;

    const float pitch = cmd->viewangles[0] * 0.017453292f;
    const float yaw = cmd->viewangles[1] * 0.017453292f;
    const float cosPitch = cosf(pitch);
    const float sinPitch = sinf(pitch);
    const float cosYaw = cosf(yaw);
    const float sinYaw = sinf(yaw);

    float eye[3];
    GetEntityEyePosition(local, eye);
    if (eye[0] == 0.0f && eye[1] == 0.0f && eye[2] == 0.0f)
        return;  // INFERRED safety: failed origin lookup would trace from 0,0,0

    Ray_t ray;
    ray.start[0] = eye[0];
    ray.start[1] = eye[1];
    ray.start[2] = eye[2];
    ray.startW = 0.0f;
    // The target computes delta as (start + forward * 8192) - start; the
    // round-trip is kept because /fp:precise must not fold it.
    const float forwardX = cosYaw * cosPitch * 8192.0f;
    const float forwardY = sinYaw * cosPitch * 8192.0f;
    const float forwardZ = -sinPitch * 8192.0f;
    ray.delta[0] = (eye[0] + forwardX) - eye[0];
    ray.delta[1] = (eye[1] + forwardY) - eye[1];
    ray.delta[2] = (eye[2] + forwardZ) - eye[2];
    ray.deltaW = 0.0f;
    ray.startOffset[0] = ray.startOffset[1] = ray.startOffset[2] = 0.0f;
    ray.startOffsetW = 0.0f;
    ray.end[0] = ray.end[1] = ray.end[2] = 0.0f;
    ray.endW = 0.0f;
    ray.worldAxisTransform = nullptr;
    ray.isRay = true;
    ray.isSwept = ray.delta[0] != 0.0f || ray.delta[1] != 0.0f ||
                  ray.delta[2] != 0.0f;
    ray.pad[0] = ray.pad[1] = 0;

    TraceResult trace;
    memset(&trace, 0, 0x24);

    if (!g_pEngineTrace || IsBadReadPtr(g_pEngineTrace, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineTrace);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 6))
        return;

    TriggerTraceFilter filter(local);
    typedef void(__thiscall * TraceRayFn)(void *, const Ray_t *,
                                          unsigned int,
                                          TriggerTraceFilter *,
                                          TraceResult *);
    reinterpret_cast<TraceRayFn>(vtable[5])(g_pEngineTrace, &ray, 0x46004003u,
                                            &filter, &trace);

    void *hit = trace.entity;
    if (!hit)
        return;
    if (!IsPlayerChamsEligible(hit))
        return;
    const int hitTeam = GetEntityTeam(hit);
    if (hitTeam == 2)
        return;

    const int hitgroup = trace.hitgroup;
    const bool wanted =
        (g_LegitAimHead && hitgroup == 1) ||
        (g_LegitAimBody && (hitgroup == 2 || hitgroup == 3)) ||
        (g_LegitTriggerLimbs != 0.0f && hitgroup >= 4 && hitgroup <= 7);

    if (g_LegitTriggerDelay >= 1) {
        const int threshold = g_LegitTriggerDelay / 30;
        if (g_LegitTriggerDelayCounter < threshold) {
            ++g_LegitTriggerDelayCounter;
        } else if (wanted) {
            cmd->buttons |= 1;
            g_LegitTriggerDelayCounter = 0;
        }
    }
}

// --- Legit aimbot (sub_10035BF0) -----------------------------------------
// sub_100026E0: line-of-sight trace from the local eye to a hitbox midpoint,
// skipping the local player. Visible when the trace ends on the candidate,
// reaches fraction 1.0, or (target quirk) the candidate itself is a
// CBreakableSurface - the class check reads the candidate, not the hit entity.
bool Legit_TraceVisible(const float start[3], const float point[3],
                        void *local, void *target) {
    float delta[3] = {point[0] - start[0], point[1] - start[1],
                      point[2] - start[2]};

    Ray_t ray;
    ray.start[0] = start[0];
    ray.start[1] = start[1];
    ray.start[2] = start[2];
    ray.startW = 0.0f;
    ray.delta[0] = delta[0];
    ray.delta[1] = delta[1];
    ray.delta[2] = delta[2];
    ray.deltaW = 0.0f;
    ray.startOffset[0] = ray.startOffset[1] = ray.startOffset[2] = 0.0f;
    ray.startOffsetW = 0.0f;
    ray.end[0] = ray.end[1] = ray.end[2] = 0.0f;
    ray.endW = 0.0f;
    ray.worldAxisTransform = nullptr;
    ray.isRay = true;
    ray.isSwept = delta[0] != 0.0f || delta[1] != 0.0f || delta[2] != 0.0f;
    ray.pad[0] = ray.pad[1] = 0;

    TraceResult trace;
    memset(&trace, 0, 0x24);

    if (!g_pEngineTrace || IsBadReadPtr(g_pEngineTrace, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineTrace);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 6))
        return false;

    TriggerTraceFilter filter(local);
    typedef void(__thiscall * TraceRayFn)(void *, const Ray_t *,
                                          unsigned int,
                                          TriggerTraceFilter *,
                                          TraceResult *);
    reinterpret_cast<TraceRayFn>(vtable[5])(g_pEngineTrace, &ray, 0x46004003u,
                                            &filter, &trace);

    if (trace.entity == target || trace.fraction == 1.0f)
        return true;
    const char *name = GetEntityNetworkName(target);
    if (!name)
        return false;
    return strcmp(name, "CBreakableSurface") == 0;
}

// sub_10001190: mstudiohitboxset_t::numhitboxes from the studio header.
void *GetStudiomodel(int modelIndex) {
    if (!g_pModelInfoClient ||
        IsBadReadPtr(g_pModelInfoClient, sizeof(void *)))
        return nullptr;
    void **vtable = *reinterpret_cast<void ***>(g_pModelInfoClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 31))
        return nullptr;
    typedef void *(__thiscall * GetStudiomodelFn)(void *, int);
    return reinterpret_cast<GetStudiomodelFn>(vtable[30])(g_pModelInfoClient,
                                                          modelIndex);
}

// entity+4 vtable slot 13: SetupBones(matrix3x4_t *out, int maxBones,
// int boneMask, float currentTime).
bool SetupEntityBones(void *entity, void *bones, int maxBones, int boneMask,
                      float currentTime) {
    if (!entity || IsBadReadPtr(entity, 0x10))
        return false;
    char *object = static_cast<char *>(entity) + 4;
    void **vtable = *reinterpret_cast<void ***>(object);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 14))
        return false;
    typedef bool(__thiscall * SetupBonesFn)(void *, void *, int, int, float);
    return reinterpret_cast<SetupBonesFn>(vtable[13])(object, bones, maxBones,
                                                      boneMask, currentTime);
}

// sub_100352B0: pick a hitbox index by group. Shotgun Mode forces group 3
// for weapon ids 11/8/3/4; otherwise the group is dword_1004AB44
// (g_LegitAimHitbox). Hitscan + Auto returns the first hitbox immediately.
int Legit_FindHitboxIndex(void *entity) {
    int group = g_LegitAimHitbox;
    if (g_LegitShotgunMode) {
        void *local = GetLocalPlayerEntity();
        void *weapon = local ? GetLocalActiveWeapon(local) : nullptr;
        int weaponId = weapon ? GetEntityWeaponId(weapon) : -1;
        if (weaponId == 11 || weaponId == 8 || weaponId == 3 ||
            weaponId == 4)
            group = 3;
    }

    if (!entity || IsBadReadPtr(entity, 0x10))
        return 0;
    int model = GetEntityRenderableModelId(entity);
    void *header = GetStudiomodel(model);
    if (!header || IsBadReadPtr(header, 0xB4))
        return 0;

    int setOffset = *reinterpret_cast<int *>(static_cast<char *>(header) +
                                              176);
    if (setOffset <= 0 || setOffset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(header) + setOffset, 12))
        return 0;
    char *set = static_cast<char *>(header) + setOffset;
    int count = *reinterpret_cast<int *>(set + 4);
    int arrayOffset = *reinterpret_cast<int *>(set + 8);
    if (count <= 0 || count > 128 || arrayOffset <= 0 ||
        arrayOffset > 0x40000)
        return 0;

    for (int i = 0; i < count; ++i) {
        char *hitbox = set + arrayOffset + 68 * i;
        if (IsBadReadPtr(hitbox, 68))
            return 0;
        int hitboxGroup = *reinterpret_cast<int *>(hitbox + 4);
        if (group == 0) {
            if (g_LegitHitscan)
                return i;
            if (hitboxGroup == 0)
                return i;
        } else if (group == 1) {
            if (hitboxGroup == 1)
                return i;
        } else if (group == 2) {
            if (hitboxGroup == 2)
                return i;
        } else if (group == 3) {
            if (hitboxGroup == 3)
                return i;
        } else if (group == 4) {
            if (hitboxGroup == 4 || hitboxGroup == 5)
                return i;
        }
    }
    return 0;
}

// sub_10035740: transform the hitbox bbox corners by the bone matrix, use
// the midpoint as the aim point, and require line of sight.
bool Legit_HitboxAimPoint(void *local, void *target, float out[3],
                          int hitboxIndex, const unsigned char *bones) {
    int model = GetEntityRenderableModelId(target);
    void *header = GetStudiomodel(model);
    if (!header || IsBadReadPtr(header, 0xB4))
        return false;

    int setOffset = *reinterpret_cast<int *>(static_cast<char *>(header) +
                                              176);
    if (setOffset <= 0 || setOffset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(header) + setOffset, 12))
        return false;
    char *set = static_cast<char *>(header) + setOffset;
    int arrayOffset = *reinterpret_cast<int *>(set + 8);
    if (arrayOffset <= 0 || arrayOffset > 0x40000)
        return false;

    char *hitbox = set + arrayOffset + 68 * hitboxIndex;
    if (IsBadReadPtr(hitbox, 32))
        return false;
    int bone = *reinterpret_cast<int *>(hitbox);
    if (bone < 0 || bone >= 128)
        return false;

    const float *matrix =
        reinterpret_cast<const float *>(bones + 48 * bone);
    const float *bbmin = reinterpret_cast<const float *>(hitbox + 8);
    const float *bbmax = reinterpret_cast<const float *>(hitbox + 20);

    float corner1[3];
    float corner2[3];
    for (int row = 0; row < 3; ++row) {
        corner1[row] = matrix[row * 4 + 0] * bbmin[0] +
                       matrix[row * 4 + 1] * bbmin[1] +
                       matrix[row * 4 + 2] * bbmin[2] + matrix[row * 4 + 3];
        corner2[row] = matrix[row * 4 + 0] * bbmax[0] +
                       matrix[row * 4 + 1] * bbmax[1] +
                       matrix[row * 4 + 2] * bbmax[2] + matrix[row * 4 + 3];
    }

    const float eps = 1.1920929e-7f;
    if (corner1[0] > -eps && corner1[0] < eps && corner1[1] > -eps &&
        corner1[1] < eps && corner1[2] > -eps && corner1[2] < eps)
        return false;
    if (corner2[0] > -eps && corner2[0] < eps && corner2[1] > -eps &&
        corner2[1] < eps && corner2[2] > -eps && corner2[2] < eps)
        return false;

    out[0] = (corner1[0] + corner2[0]) * 0.5f;
    out[1] = (corner2[1] + corner1[1]) * 0.5f;
    out[2] = (corner2[2] + corner1[2]) * 0.5f;
    return Legit_TraceVisible(g_AimEyePos, out, local, target);
}

// VEngineCvar007 FindVar (slot 13) + ConVar GetFloat (slot 11).
float GetConVarFloat(const char *name) {
    if (!g_pEngineCvar || IsBadReadPtr(g_pEngineCvar, sizeof(void *)))
        return 0.0f;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineCvar);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 14))
        return 0.0f;
    typedef void *(__thiscall * FindVarFn)(void *, const char *);
    void *var = reinterpret_cast<FindVarFn>(vtable[13])(g_pEngineCvar, name);
    if (!var || IsBadReadPtr(var, sizeof(void *)))
        return 0.0f;
    void **conVarVtable = *reinterpret_cast<void ***>(var);
    if (!conVarVtable || IsBadReadPtr(conVarVtable, sizeof(void *) * 12))
        return 0.0f;
    typedef float(__thiscall * GetFloatFn)(void *);
    return reinterpret_cast<GetFloatFn>(conVarVtable[11])(var);
}

// IVEngineClient slot 19 GetViewAngles / slot 20 SetViewAngles.
bool EngineGetViewAngles(float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 20))
        return false;
    typedef void(__thiscall * GetViewAnglesFn)(void *, float *);
    reinterpret_cast<GetViewAnglesFn>(vtable[19])(g_pEngineClient, out);
    return true;
}

void EngineSetViewAngles(const float angles[3]) {
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 21))
        return;
    typedef void(__thiscall * SetViewAnglesFn)(void *, const float *);
    reinterpret_cast<SetViewAnglesFn>(vtable[20])(g_pEngineClient, angles);
}

float NormalizeAngle180(float angle) {
    while (angle < -180.0f)
        angle += 360.0f;
    while (angle > 180.0f)
        angle -= 360.0f;
    return angle;
}

// sub_10035BF0: scan every entity, keep the best visible hitbox target
// inside the FOV cone, then aim at it with optional smoothing and push the
// result through IVEngineClient::SetViewAngles.
void Legit_Aimbot(CUserCmd *cmd, void *local) {
    if (!cmd || !local)
        return;

    // The target calls GetEntityTeam(local) first (netprop warm-up).
    GetEntityTeam(local);
    GetEntityEyePosition(local, g_AimEyePos);
    EngineGetViewAngles(g_AimViewAngles);

    const float pitchRad = g_AimViewAngles[0] * 0.017453292f;
    const float yawRad = g_AimViewAngles[1] * 0.017453292f;
    const float cosPitch = cosf(pitchRad);
    const float sinPitch = sinf(pitchRad);
    const float cosYaw = cosf(yawRad);
    const float sinYaw = sinf(yawRad);
    g_AimForward[0] = cosYaw * cosPitch;
    g_AimForward[1] = sinYaw * cosPitch;
    g_AimForward[2] = -sinPitch;

    g_AimTargetIndex = -1;
    g_AimBestDistance = 99999.9f;
    float bestPriority = -1.0f;
    g_AimTargetHealth = 0;
    g_AimTargetMaxHealth = 0;
    g_AimTargetClassName = nullptr;
    g_AimPoint[0] = g_AimPoint[1] = g_AimPoint[2] = 0.0f;
    g_AimStoredPoint[0] = g_AimStoredPoint[1] = g_AimStoredPoint[2] = 0.0f;

    const int highest = GetHighestEntityIndex();
    for (int i = 1; i <= highest; ++i) {
        void *entity = GetClientEntityByIndex(i);
        if (!entity || entity == local)
            continue;
        if (!IsPlayerChamsEligible(entity))
            continue;
        if (!GetEntityRenderableModelId(entity))
            continue;

        const int category = GetEntityCategory(entity);
        if (category == 5 && GetEntityTeam(entity) == 3) {
            const int ghost = GetEntityIsGhostInt(entity);
            if (ghost == 1 || ghost == 25165825 || ghost == 8388609 ||
                ghost == 58851329 || ghost == 25296897 ||
                ghost == 92274689 || ghost == 131073 || ghost == 58720257)
                continue;
        }
        if (g_LegitIgnoreTeam && GetEntityTeam(entity) == 2)
            continue;
        if (g_LegitIgnoreWitch && category == 2 &&
            GetEntityWitchRage(entity) != 1.0f)
            continue;
        if (GetEntityTeam(entity) == 4)
            continue;
        if (IsEntityDormant(entity))
            continue;
        if (GetEntitySolidFlags(entity) == 20)
            continue;

        GetEntityCategory(entity);  // target re-reads the class, result unused
        unsigned char bones[6144];
        if (!SetupEntityBones(entity, bones, 128, 256, 0.0f))
            continue;

        const int hitbox = Legit_FindHitboxIndex(entity);
        if (!Legit_HitboxAimPoint(local, entity, g_AimPoint, hitbox, bones))
            continue;

        float priority = 1.0f;
        if (category == 3)
            priority = 2.0f;
        else if (category == 5)
            priority = 3.0f;

        const float dx = g_AimPoint[0] - g_AimEyePos[0];
        const float dy = g_AimPoint[1] - g_AimEyePos[1];
        const float dz = g_AimPoint[2] - g_AimEyePos[2];
        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        const float inverse = 1.0f / (distance + 1.1920929e-7f);
        float dot = (dx * inverse) * g_AimForward[0] +
                    (dy * inverse) * g_AimForward[1] +
                    (dz * inverse) * g_AimForward[2];
        dot = dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
        const float angle = acosf(dot) * 57.295776f;
        if (angle > static_cast<float>(g_LegitAimFov))
            continue;
        if (distance > g_AimBestDistance)
            continue;

        bool selected = false;
        if (g_LegitPrioritySpecial) {
            if (priority > bestPriority) {
                bestPriority = priority;
                selected = true;
            }
        } else {
            g_AimBestDistance = distance;
            selected = true;
        }
        if (selected) {
            g_AimStoredCmdAngles[0] = cmd->viewangles[0];
            g_AimStoredCmdAngles[1] = cmd->viewangles[1];
            g_AimStoredCmdAngles[2] = cmd->viewangles[2];
            g_AimStoredPoint[0] = g_AimPoint[0];
            g_AimStoredPoint[1] = g_AimPoint[1];
            g_AimStoredPoint[2] = g_AimPoint[2];
            g_AimTargetIndex = i;
        }

        g_AimTargetClassName = GetEntityNetworkName(entity);
        if (category == 4) {
            g_AimTargetMaxHealth = 100;
            g_AimTargetHealth = 100;
        } else {
            g_AimTargetMaxHealth = GetEntityMaxHealth(entity);
            g_AimTargetHealth = GetEntityHealth(entity);
        }

        if (g_LegitPositionAdjustment) {
            float interp = GetConVarFloat("cl_interp");
            float ratioOverRate = GetConVarFloat("cl_interp_ratio") /
                                  GetConVarFloat("cl_updaterate");
            if (interp <= ratioOverRate)
                ratioOverRate = GetConVarFloat("cl_interp_ratio") /
                                GetConVarFloat("cl_updaterate");
            else
                ratioOverRate = GetConVarFloat("cl_interp");
            if (g_pGlobalVars && g_pGlobalVars->interval_per_tick > 0.0f) {
                const float simulationTime = GetEntitySimulationTime(local);
                cmd->tick_count =
                    static_cast<int>((simulationTime + ratioOverRate) /
                                         g_pGlobalVars->interval_per_tick +
                                     0.5f);
            }
        }

        if (g_LegitHitboxMode != 0) {
            if (g_LegitHitboxMode == 1)
                g_LegitAimHitbox = 1;
        } else {
            const int classId = GetEntityClassId(entity);
            if (classId == 264 || classId == 0)
                g_LegitAimHitbox = 2;
            else if (classId == 99)
                g_LegitAimHitbox = 1;
            else if (classId == 263)
                g_LegitAimHitbox = 2;
            else if (classId == 265)
                g_LegitAimHitbox = 1;
            else if (classId == 270)
                g_LegitAimHitbox = 3;
            else if (classId == 276 || classId == 277)
                g_LegitAimHitbox = 0;
        }
    }

    if (g_AimTargetIndex == -1)
        return;

    const float dx = g_AimEyePos[0] - g_AimStoredPoint[0];
    const float dy = g_AimEyePos[1] - g_AimStoredPoint[1];
    const float dz = g_AimEyePos[2] - g_AimStoredPoint[2];
    const float horizontal = sqrtf(dx * dx + dy * dy);
    const float pitch = atanf(dz / horizontal) * 57.295776f;
    float yaw = atanf(dy / dx) * 57.295776f;
    cmd->viewangles[1] = yaw;
    if (dx >= 0.0f)
        cmd->viewangles[1] = yaw + 180.0f;
    cmd->viewangles[0] = NormalizeAngle180(pitch);
    cmd->viewangles[1] = NormalizeAngle180(cmd->viewangles[1]);

    if (g_LegitSmoothY > 0 && g_LegitSmoothX > 0) {
        g_AimViewAngles[0] = NormalizeAngle180(g_AimViewAngles[0]);
        g_AimViewAngles[1] = NormalizeAngle180(g_AimViewAngles[1]);
        const float pitchDelta =
            NormalizeAngle180(cmd->viewangles[0] - g_AimViewAngles[0]);
        const float yawDelta =
            NormalizeAngle180(cmd->viewangles[1] - g_AimViewAngles[1]);
        const float newPitch =
            pitchDelta / static_cast<float>(g_LegitSmoothY) +
            g_AimViewAngles[0];
        const float newYaw =
            yawDelta / static_cast<float>(g_LegitSmoothX) +
            g_AimViewAngles[1];
        cmd->viewangles[0] = NormalizeAngle180(newPitch);
        cmd->viewangles[1] = NormalizeAngle180(newYaw);
    }

    EngineSetViewAngles(cmd->viewangles);
}

// --- Rage aimbot (sub_10003380) ------------------------------------------
// sub_10002310: same hitbox-group lookup as the Legit variant, using the
// Rage resolved group (dword_1004AB68), Shotgun Mode (AB79) and Hitscan
// (AB78).
int Rage_FindHitboxIndex(void *entity) {
    int group = g_RageAimHitboxResolved;
    if (g_RageShotgunMode) {
        void *local = GetLocalPlayerEntity();
        void *weapon = local ? GetLocalActiveWeapon(local) : nullptr;
        int weaponId = weapon ? GetEntityWeaponId(weapon) : -1;
        if (weaponId == 11 || weaponId == 8 || weaponId == 3 ||
            weaponId == 4)
            group = 3;
    }

    if (!entity || IsBadReadPtr(entity, 0x10))
        return 0;
    int model = GetEntityRenderableModelId(entity);
    void *header = GetStudiomodel(model);
    if (!header || IsBadReadPtr(header, 0xB4))
        return 0;

    int setOffset = *reinterpret_cast<int *>(static_cast<char *>(header) +
                                              176);
    if (setOffset <= 0 || setOffset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(header) + setOffset, 12))
        return 0;
    char *set = static_cast<char *>(header) + setOffset;
    int count = *reinterpret_cast<int *>(set + 4);
    int arrayOffset = *reinterpret_cast<int *>(set + 8);
    if (count <= 0 || count > 128 || arrayOffset <= 0 ||
        arrayOffset > 0x40000)
        return 0;

    for (int i = 0; i < count; ++i) {
        char *hitbox = set + arrayOffset + 68 * i;
        if (IsBadReadPtr(hitbox, 68))
            return 0;
        int hitboxGroup = *reinterpret_cast<int *>(hitbox + 4);
        if (group == 0) {
            if (g_RageHitscan)
                return i;
            if (hitboxGroup == 0)
                return i;
        } else if (group == 1) {
            if (hitboxGroup == 1)
                return i;
        } else if (group == 2) {
            if (hitboxGroup == 2)
                return i;
        } else if (group == 3) {
            if (hitboxGroup == 3)
                return i;
        } else if (group == 4) {
            if (hitboxGroup == 4 || hitboxGroup == 5)
                return i;
        }
    }
    return 0;
}

// sub_10002450: hitbox bbox transform and LOS check with the Rage eye.
bool Rage_HitboxAimPoint(void *local, void *target, float out[3],
                         int hitboxIndex, const unsigned char *bones) {
    int model = GetEntityRenderableModelId(target);
    void *header = GetStudiomodel(model);
    if (!header || IsBadReadPtr(header, 0xB4))
        return false;

    int setOffset = *reinterpret_cast<int *>(static_cast<char *>(header) +
                                              176);
    if (setOffset <= 0 || setOffset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(header) + setOffset, 12))
        return false;
    char *set = static_cast<char *>(header) + setOffset;
    int arrayOffset = *reinterpret_cast<int *>(set + 8);
    if (arrayOffset <= 0 || arrayOffset > 0x40000)
        return false;

    char *hitbox = set + arrayOffset + 68 * hitboxIndex;
    if (IsBadReadPtr(hitbox, 32))
        return false;
    int bone = *reinterpret_cast<int *>(hitbox);
    if (bone < 0 || bone >= 128)
        return false;

    const float *matrix =
        reinterpret_cast<const float *>(bones + 48 * bone);
    const float *bbmin = reinterpret_cast<const float *>(hitbox + 8);
    const float *bbmax = reinterpret_cast<const float *>(hitbox + 20);

    float corner1[3];
    float corner2[3];
    for (int row = 0; row < 3; ++row) {
        corner1[row] = matrix[row * 4 + 0] * bbmin[0] +
                       matrix[row * 4 + 1] * bbmin[1] +
                       matrix[row * 4 + 2] * bbmin[2] + matrix[row * 4 + 3];
        corner2[row] = matrix[row * 4 + 0] * bbmax[0] +
                       matrix[row * 4 + 1] * bbmax[1] +
                       matrix[row * 4 + 2] * bbmax[2] + matrix[row * 4 + 3];
    }

    const float eps = 1.1920929e-7f;
    if (corner1[0] > -eps && corner1[0] < eps && corner1[1] > -eps &&
        corner1[1] < eps && corner1[2] > -eps && corner1[2] < eps)
        return false;
    if (corner2[0] > -eps && corner2[0] < eps && corner2[1] > -eps &&
        corner2[1] < eps && corner2[2] > -eps && corner2[2] < eps)
        return false;

    out[0] = (corner1[0] + corner2[0]) * 0.5f;
    out[1] = (corner2[1] + corner1[1]) * 0.5f;
    out[2] = (corner2[2] + corner1[2]) * 0.5f;
    return Legit_TraceVisible(g_RageEyePos, out, local, target);
}

// sub_100022B0 elapsed-check used by Prediction Fix Auto Shoot: the target
// initializes its timer on first use and fires once 120 ms have elapsed.
bool RagePredictionTimerElapsed() {
    static LARGE_INTEGER last = {};
    static bool initialized = false;
    if (!initialized) {
        QueryPerformanceCounter(&last);
        initialized = true;
    }
    return TimerElapsedMs(&last, 120);
}

// sub_10003380: Rage aimbot. Same candidate scan and hitbox math as the Legit
// aimbot, but with the Rage settings, the shared health/name fields, the
// auto-resolved hitbox (dword_1004AB68), the silent/perfect-silent view
// handling and the Auto Shoot button logic.
void Rage_Aimbot(CUserCmd *cmd, void *local) {
    if (!cmd || !local)
        return;

    GetEntityTeam(local);
    GetEntityEyePosition(local, g_RageEyePos);
    EngineGetViewAngles(g_RageViewAngles);

    const float pitchRad = g_RageViewAngles[0] * 0.017453292f;
    const float yawRad = g_RageViewAngles[1] * 0.017453292f;
    const float cosPitch = cosf(pitchRad);
    const float sinPitch = sinf(pitchRad);
    const float cosYaw = cosf(yawRad);
    const float sinYaw = sinf(yawRad);
    g_RageForward[0] = cosYaw * cosPitch;
    g_RageForward[1] = sinYaw * cosPitch;
    g_RageForward[2] = -sinPitch;

    g_RageTargetIndex = -1;
    g_RageBestDistance = 99999.9f;
    float bestPriority = -1.0f;
    g_AimTargetHealth = 0;
    g_AimTargetMaxHealth = 0;
    g_AimTargetClassName = nullptr;
    g_RageAimPoint[0] = g_RageAimPoint[1] = g_RageAimPoint[2] = 0.0f;
    g_RageStoredPoint[0] = g_RageStoredPoint[1] =
        g_RageStoredPoint[2] = 0.0f;

    const int highest = GetHighestEntityIndex();
    for (int i = 1; i <= highest; ++i) {
        void *entity = GetClientEntityByIndex(i);
        if (!entity || entity == local)
            continue;
        if (!IsPlayerChamsEligible(entity))
            continue;
        if (!GetEntityRenderableModelId(entity))
            continue;

        const int category = GetEntityCategory(entity);
        if (category == 5 && GetEntityTeam(entity) == 3) {
            const int ghost = GetEntityIsGhostInt(entity);
            if (ghost == 1 || ghost == 25165825 || ghost == 8388609 ||
                ghost == 58851329 || ghost == 25296897 ||
                ghost == 92274689 || ghost == 131073 || ghost == 58720257)
                continue;
        }
        if (g_RageIgnoreTeam && GetEntityTeam(entity) == 2)
            continue;
        if (g_RageIgnoreWitch && category == 2 &&
            GetEntityWitchRage(entity) != 1.0f)
            continue;
        if (GetEntityTeam(entity) == 4)
            continue;
        if (IsEntityDormant(entity))
            continue;
        if (GetEntitySolidFlags(entity) == 20)
            continue;

        GetEntityCategory(entity);
        unsigned char bones[6144];
        if (!SetupEntityBones(entity, bones, 128, 256, 0.0f))
            continue;

        const int hitbox = Rage_FindHitboxIndex(entity);
        if (!Rage_HitboxAimPoint(local, entity, g_RageAimPoint, hitbox,
                                 bones))
            continue;

        float priority = 1.0f;
        if (category == 3)
            priority = 2.0f;
        else if (category == 5)
            priority = 3.0f;

        const float dx = g_RageAimPoint[0] - g_RageEyePos[0];
        const float dy = g_RageAimPoint[1] - g_RageEyePos[1];
        const float dz = g_RageAimPoint[2] - g_RageEyePos[2];
        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        const float inverse = 1.0f / (distance + 1.1920929e-7f);
        float dot = (dx * inverse) * g_RageForward[0] +
                    (dy * inverse) * g_RageForward[1] +
                    (dz * inverse) * g_RageForward[2];
        dot = dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
        const float angle = acosf(dot) * 57.295776f;
        if (angle > static_cast<float>(g_RageAimFov))
            continue;
        if (distance > g_RageBestDistance)
            continue;

        bool selected = false;
        if (g_RagePrioritySpecial) {
            if (priority > bestPriority) {
                bestPriority = priority;
                selected = true;
            }
        } else {
            g_RageBestDistance = distance;
            selected = true;
        }
        if (selected) {
            g_RageStoredCmdAngles[0] = cmd->viewangles[0];
            g_RageStoredCmdAngles[1] = cmd->viewangles[1];
            g_RageStoredCmdAngles[2] = cmd->viewangles[2];
            g_RageStoredPoint[0] = g_RageAimPoint[0];
            g_RageStoredPoint[1] = g_RageAimPoint[1];
            g_RageStoredPoint[2] = g_RageAimPoint[2];
            g_RageTargetIndex = i;
        }

        g_AimTargetClassName = GetEntityNetworkName(entity);
        if (category == 4) {
            g_AimTargetMaxHealth = 100;
            g_AimTargetHealth = 100;
        } else {
            g_AimTargetMaxHealth = GetEntityMaxHealth(entity);
            g_AimTargetHealth = GetEntityHealth(entity);
        }

        if (g_RagePositionAdjustment && !g_SmacMode) {
            float interp = GetConVarFloat("cl_interp");
            float ratioOverRate = GetConVarFloat("cl_interp_ratio") /
                                  GetConVarFloat("cl_updaterate");
            if (interp <= ratioOverRate)
                ratioOverRate = GetConVarFloat("cl_interp_ratio") /
                                GetConVarFloat("cl_updaterate");
            else
                ratioOverRate = GetConVarFloat("cl_interp");
            if (g_pGlobalVars && g_pGlobalVars->interval_per_tick > 0.0f) {
                const float simulationTime = GetEntitySimulationTime(local);
                cmd->tick_count =
                    static_cast<int>((simulationTime + ratioOverRate) /
                                         g_pGlobalVars->interval_per_tick +
                                     0.5f);
            }
        }

        if (g_RageHitbox != 0) {
            if (g_RageHitbox == 1)
                g_RageAimHitboxResolved = 1;
        } else {
            const int classId = GetEntityClassId(entity);
            if (classId == 264 || classId == 0)
                g_RageAimHitboxResolved = 2;
            else if (classId == 99)
                g_RageAimHitboxResolved = 1;
            else if (classId == 263)
                g_RageAimHitboxResolved = 2;
            else if (classId == 265)
                g_RageAimHitboxResolved = 1;
            else if (classId == 270)
                g_RageAimHitboxResolved = 3;
            else if (classId == 276 || classId == 277)
                g_RageAimHitboxResolved = 0;
        }
    }

    if (g_RageTargetIndex == -1) {
        g_RageHasTarget = false;
        return;
    }

    g_RageHasTarget = true;

    const float dx = g_RageEyePos[0] - g_RageStoredPoint[0];
    const float dy = g_RageEyePos[1] - g_RageStoredPoint[1];
    const float dz = g_RageEyePos[2] - g_RageStoredPoint[2];
    const float horizontal = sqrtf(dx * dx + dy * dy);
    const float pitch = atanf(dz / horizontal) * 57.295776f;
    float yaw = atanf(dy / dx) * 57.295776f;
    cmd->viewangles[1] = yaw;
    if (dx >= 0.0f)
        cmd->viewangles[1] = yaw + 180.0f;
    cmd->viewangles[0] = NormalizeAngle180(pitch);
    cmd->viewangles[1] = NormalizeAngle180(cmd->viewangles[1]);

    // Target condition: !Silent && !PerfectSilent && !AutoShoot && Smooth > 0.
    if (!g_RageSilentAim && !g_RagePerfectSilentAim && !g_RageAutoShoot &&
        g_RageSmooth > 0) {
        g_RageViewAngles[0] = NormalizeAngle180(g_RageViewAngles[0]);
        g_RageViewAngles[1] = NormalizeAngle180(g_RageViewAngles[1]);
        const float pitchDelta =
            NormalizeAngle180(cmd->viewangles[0] - g_RageViewAngles[0]);
        const float yawDelta =
            NormalizeAngle180(cmd->viewangles[1] - g_RageViewAngles[1]);
        const float newPitch =
            pitchDelta / static_cast<float>(g_RageSmooth) +
            g_RageViewAngles[0];
        const float newYaw =
            yawDelta / static_cast<float>(g_RageSmooth) +
            g_RageViewAngles[1];
        cmd->viewangles[0] = NormalizeAngle180(newPitch);
        cmd->viewangles[1] = NormalizeAngle180(newYaw);
    }

    if (g_SmacMode || !g_RageSilentAim)
        EngineSetViewAngles(cmd->viewangles);

    if (g_RageAutoShoot) {
        if (g_RagePredictionFix && GetAsyncKeyState(g_SpeedHackKey) < 0) {
            if (RagePredictionTimerElapsed())
                cmd->buttons |= 1;
        } else {
            cmd->buttons |= 1;
            if (g_SmacMode) {
                if (g_RageSmacFlagA || !g_RageSmacFlagB) {
                    if ((GetEntityFlags(local) & 1) != 0) {
                        g_RageSmacFlagB = 1;
                        g_RageSmacFlagA = 1;
                    } else {
                        cmd->buttons &= ~1;
                        g_RageSmacFlagA = 0;
                    }
                } else {
                    cmd->buttons |= 1;
                    g_RageSmacFlagB = 0;
                }
            }
        }
    }
}

// sub_10002D90: Rage Auto Shove. Runs when the Rage aim key is NOT held, the
// local player is not incapacitated and not jumping. Scans entities for an
// infected within 92 units (eye-to-eye), snaps the view to its eye position,
// zeroes roll, and presses IN_ATTACK2. Every qualifying entity is processed
// in index order (the target does not stop at the first hit, so the last one
// wins). The target's fdclass checks (finite pitch, yaw and forwardmove) are
// reproduced with isfinite.
void Rage_AutoShove(CUserCmd *cmd, void *local) {
    if (!cmd || !local)
        return;

    float localEye[3];
    GetEntityEyePosition(local, localEye);
    if (GetEntityIsIncapacitated(local) != 0)
        return;
    if ((cmd->buttons & 0x20) != 0)  // IN_JUMP
        return;
    if (GetAsyncKeyState(g_RageAimKey) < 0)
        return;

    const int highest = GetHighestEntityIndex();
    for (int i = 1; i <= highest; ++i) {
        void *entity = GetClientEntityByIndex(i);
        if (!entity || entity == local)
            continue;
        if (!IsPlayerChamsEligible(entity))
            continue;
        if (!GetEntityRenderableModelId(entity))
            continue;
        const int team = GetEntityTeam(entity);
        if (team == 4 || team == 2)
            continue;
        if (IsEntityDormant(entity))
            continue;

        unsigned char bones[6144];
        if (!SetupEntityBones(entity, bones, 128, 256, 0.0f))
            continue;

        const int category = GetEntityCategory(entity);
        if (category == 5 && team == 3) {
            const int ghost = GetEntityIsGhostInt(entity);
            if (ghost == 1 || ghost == 25165825 || ghost == 8388609 ||
                ghost == 58851329 || ghost == 25296897 ||
                ghost == 92274689 || ghost == 131073 || ghost == 58720257)
                continue;
        }
        if (GetEntitySolidFlags(entity) == 20)
            continue;
        if (category != 4 && category != 3 && category != 5)
            continue;

        float entityEye[3];
        GetEntityEyePosition(entity, entityEye);
        const float dx = localEye[0] - entityEye[0];
        const float dy = localEye[1] - entityEye[1];
        const float dz = localEye[2] - entityEye[2];
        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (!(distance < 92.0f))
            continue;

        const float horizontal = sqrtf(dx * dx + dy * dy);
        float pitch = atanf(dz / horizontal) * 57.295776f;
        float yaw = atanf(dy / dx) * 57.295776f;
        if (dx >= 0.0f)
            yaw += 180.0f;
        pitch = NormalizeAngle180(pitch);
        yaw = NormalizeAngle180(yaw);
        if (!std::isfinite(pitch) || !std::isfinite(yaw) ||
            !std::isfinite(cmd->forwardmove))
            continue;

        cmd->buttons |= 0x800;  // IN_ATTACK2 (shove)
        cmd->viewangles[0] = pitch;
        cmd->viewangles[1] = yaw;
        cmd->viewangles[2] = 0.0f;
    }
}

} // namespace

// 0x100055B0 (bunny-hop block at 0x10005E20).
void __fastcall Cheat_Update(CUserCmd *cmd, void * /*edx*/) {
    if (!cmd)
        return;

    // Target sets g_MenuState byte 3 to 1 at the start of every update
    // (0x100055E5); anti-aim's desync checks read it.
    g_MenuState = (g_MenuState & 0x00FFFFFFu) | 0x01000000u;

    // 0x1000564F: Name Stealer (byte_1004B015). Every ~1 ms, collect other
    // same-team player-category entities (class IDs 232/275), pick one at
    // random, read its name via engine GetPlayerInfo (slot 8), and issue
    // `setinfo name "<name>\r"` through ExecuteClientCmd.
    // SAFETY DEVIATION: the target formats an uninitialized player-info
    // buffer if GetPlayerInfo fails; ours is zeroed and the command is
    // skipped when the name is empty.
    if (g_NameStealer) {
        static LARGE_INTEGER timer = {};
        if (TimerElapsedMs(&timer, 1)) {
            void *local = GetLocalPlayerEntity();
            if (local && !IsBadReadPtr(local, 0x100)) {
                int localTeam = GetEntityTeam(local);
                int highest = GetHighestEntityIndex();
                std::vector<int> players;
                for (int i = 1; i <= highest; ++i) {
                    void *entity = GetClientEntityByIndex(i);
                    if (entity && entity != local &&
                        GetEntityCategory(entity) == 5 &&
                        GetEntityTeam(entity) == localTeam)
                        players.push_back(i);
                }
                if (!players.empty()) {
                    int index =
                        players[rand() % static_cast<int>(players.size())];
                    char info[256] = {};
                    if (GetPlayerInfo(index, info) && info[0] != '\0') {
                        char command[256];
                        sprintf_s(command, sizeof(command),
                                  "setinfo name \"%s%c\"", info, 0x0D);
                        ExecuteClientCmd(command);
                    }
                }
            }
        }
    }

    // 0x100056B1: Chat Spam (byte_1004AC14; mode dword_1004AC18).
    // Mode 0 "Advertise" sends "say : " + the UC tag; mode 1 "Chat Jammer"
    // sends "say : " + 21 CRs + a space. With Name Stealer off the target
    // sends on every update; with it on each mode is throttled to 3000 ms
    // by its own per-mode timer (target quirk).
    if (g_ChatSpam) {
        static const char kAdvertise[] =
            "say : "
            "\r\r\r\r\r\r\r"
            "\r\r\r\r\r\r\r"
            "\r\r\r\r\r\r\r"
            " [www.unknowncheats.me] @space785";
        static const char kChatJammer[] =
            "say : "
            "\r\r\r\r\r\r\r"
            "\r\r\r\r\r\r\r"
            "\r\r\r\r\r\r\r"
            " ";

        if (g_ChatSpamMode == 0) {
            static LARGE_INTEGER timer = {};
            if (!g_NameStealer || TimerElapsedMs(&timer, 3000)) {
                // Target reseeds rand and picks one of ten identical strings.
                srand(static_cast<unsigned int>(time(nullptr)));
                (void)(rand() % 10);
                ExecuteClientCmd(kAdvertise);
            }
        } else if (g_ChatSpamMode == 1) {
            static LARGE_INTEGER timer = {};
            if (!g_NameStealer || TimerElapsedMs(&timer, 3000))
                ExecuteClientCmd(kChatJammer);
        }
    }

    if (g_FovChangerEnabled) {
        SetConVarInt("cl_viewmodelfovsurvivor", g_FovViewModel);
        SetConVarInt("fov_desired", g_FovDesired);
    }

    // 0x10005C6B: Aspect Ratio. Written only while the option is enabled and
    // SMAC mode is off; when disabled the target leaves r_aspectratio at its
    // last value (quirk preserved).
    if (g_AspectRatioEnabled && !g_SmacMode)
        SetConVarInt("r_aspectratio", g_AspectRatioValue);

    // 0x10005BC1: No Visual Recoil.
    SetConVarInt("z_gun_vertical_punch", g_NoVisualRecoil ? 0 : 1);
    SetConVarInt("cl_viewbob", g_NoVisualRecoil ? 0 : 1);

    // 0x100058B2 / 0x10005A21: Remove Fog + Full Bright.
    SetConVarInt("fog_override", g_RemoveFog ? 1 : 0);
    SetConVarInt("fog_enable", g_RemoveFog ? 0 : 1);
    SetConVarInt("mat_fullbright", g_FullBright ? 1 : 0);

    // 0x10005A36: Thirdperson. While the option is on, the target resolves
    // "deadflag" in DT_BasePlayer (nested DT_PlayerState) and zeroes the two
    // dwords after it, then forces z_view_distance to -100. With a keybind
    // (ThirdpersonKey) the convar is toggled on key press instead; with no
    // keybind it is set every update.
    if (g_Thirdperson) {
        static int deadflagOffset = -2;
        if (deadflagOffset == -2)
            deadflagOffset =
                NetpropOffsetInTable("DT_BasePlayer", "deadflag");
        void *local = GetLocalPlayerEntity();
        if (local && deadflagOffset > 0x100 && deadflagOffset < 0x4000) {
            char *base = static_cast<char *>(local) + deadflagOffset;
            *reinterpret_cast<unsigned int *>(base + 4) = 0;
            *reinterpret_cast<unsigned int *>(base + 8) = 0;
        }
        if (g_ThirdpersonKey) {
            if ((GetAsyncKeyState(g_ThirdpersonKey) & 1) != 0) {
                g_ThirdpersonToggle = !g_ThirdpersonToggle;
                SetConVarInt("z_view_distance",
                             g_ThirdpersonToggle ? -100 : 0);
            }
        } else {
            SetConVarInt("z_view_distance", -100);
        }
    } else {
        SetConVarInt("z_view_distance", 0);
    }

    // 0x10005C8E: Teleport (byte_1004AC1C). While enabled and the
    // TeleportKey (if bound) is held, both move axes are set to 0x7FC00000
    // (quiet NaN). The engine's movement code then produces a bogus velocity,
    // which is the target's teleport exploit. Written before AutoStrafer so
    // the strafe block can overwrite the NaN, matching target order.
    if (g_Teleport) {
        if (!g_TeleportKey || (GetAsyncKeyState(g_TeleportKey) & 0x8000) != 0) {
            *reinterpret_cast<uint32_t *>(&cmd->forwardmove) = 0x7FC00000u;
            *reinterpret_cast<uint32_t *>(&cmd->sidemove) = 0x7FC00000u;
        }
    }

    // 0x10005CC5: Air Stuck (byte_1004AC1D, high byte of word_1004AC1C).
    // While enabled, airborne (!FL_ONGROUND) and the AirStuckKey (if bound)
    // is held, forwardmove is set to the same NaN, freezing horizontal motion
    // in the air. Only forwardmove is written (target quirk).
    if (g_AirStuck) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100) &&
            (GetEntityFlags(local) & 1) == 0 &&
            (!g_AirStuckKey || (GetAsyncKeyState(g_AirStuckKey) & 0x8000) != 0)) {
            *reinterpret_cast<uint32_t *>(&cmd->forwardmove) = 0x7FC00000u;
        }
    }

    // 0x10005D00: Invisible Jockey (byte_1004B014). Target quirk preserved:
    // the distance guard is inverted (the huge forwardmove write only runs
    // when 0 > distance, i.e. never), so the feature is inert while still
    // performing the entity scan when enabled.
    if (g_InvisibleJockey) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100)) {
            float eye[3];
            GetEntityEyePosition(local, eye);
            const bool localIsCategory3 = GetEntityCategory(local) == 3;
            const int highest = GetHighestEntityIndex();
            for (int i = 1; i <= highest; ++i) {
                void *entity = GetClientEntityByIndex(i);
                if (!entity || entity == local)
                    continue;
                if (!IsPlayerChamsEligible(entity))
                    continue;
                if (GetEntityCategory(entity) != 5 && !localIsCategory3)
                    continue;
                float entityEye[3];
                GetEntityEyePosition(entity, entityEye);
                const float dx = eye[0] - entityEye[0];
                const float dy = eye[1] - entityEye[1];
                const float dz = eye[2] - entityEye[2];
                const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
                if (0.0f > distance) {
                    *reinterpret_cast<uint32_t *>(&cmd->forwardmove) =
                        0x7CF0BDC2u;
                }
            }
        }
    }

    // 0x10005E20: AutoStrafer, gated by byte_1004ABF1 (config key
    // "AutoStrafer", no menu checkbox). byte_1004A121 starts at 1 and latches
    // to 0 the first time any WASD key is held, disabling the block for the
    // rest of the process (target quirk).
    if (g_AutoStrafer) {
        if ((GetKeyState('A') & 0x8000) || (GetKeyState('W') & 0x8000) ||
            (GetKeyState('S') & 0x8000) || (GetKeyState('D') & 0x8000))
            g_AutoStrafeActive = false;

        if (GetAsyncKeyState(VK_SPACE) && g_AutoStrafeActive) {
            void *local = GetLocalPlayerEntity();
            if (local && !IsBadReadPtr(local, 0x100) &&
                (GetEntityFlags(local) & 1) == 0) {
                short mouseDx = cmd->mousedx;
                if (static_cast<unsigned short>(mouseDx + 1) > 2) {
                    cmd->sidemove =
                        (static_cast<float>(mouseDx) >= 0.0f) ? 450.0f
                                                              : -450.0f;
                } else {
                    float velocity[3];
                    GetEntityVelocity(local, velocity);
                    float speed =
                        std::sqrt(velocity[0] * velocity[0] +
                                  velocity[1] * velocity[1]);
                    float forward = 7200.0f / speed;
                    cmd->forwardmove = forward;
                    cmd->sidemove =
                        (cmd->command_number & 1) ? 450.0f : -450.0f;
                    if (forward > 450.0f)
                        cmd->forwardmove = 450.0f;
                }
            }
        }
    }

    // 0x10005F4A: Fake Lag (byte_1004AC0E, slider dword_1004AC10). While
    // enabled, choke for `value` consecutive updates and release for one,
    // repeating. HIBYTE(g_MenuState) was set to 1 at the start of this
    // update; clearing it here makes the slot-20 detour AND the engine's
    // send-packet byte with 0, holding the packet back. Target order: after
    // AutoStrafer and before the anti-aim block, which also reads HIBYTE.
    if (g_FakeLag) {
        if (g_FakeLagCounter <= 0) {
            g_FakeLagCounter = g_FakeLagValue;
        } else {
            --g_FakeLagCounter;
            g_MenuState &= 0x00FFFFFFu;
        }
    }

    // 0x10005F8A: anti-aim viewangle manipulation. Runs after Fake Lag so
    // the desync test sees the choke-cleared HIBYTE(g_MenuState), as in the
    // target.
    if (g_RageEnabled && !g_SmacMode && (cmd->buttons & 0x21) == 0) {
        switch (g_RagePitch) {
        case 1:
            cmd->viewangles[0] = -271.0f;
            break;
        case 2:
            cmd->viewangles[0] = 270.0f;
            break;
        case 3:
            cmd->viewangles[0] = -1000.0f;
            break;
        }

        bool desyncActive = (g_MenuState >> 24) != 0;
        switch (g_RageRealYaw) {
        case 1:
            cmd->viewangles[1] = -90.0f;
            if (desyncActive || g_RageDesyncYaw == 1)
                break;
            if (g_RageRealYaw == 2)
                cmd->viewangles[1] = 90.0f;
            break;
        case 2:
            if (desyncActive) {
                cmd->viewangles[1] = 90.0f;
                break;
            }
            if (g_RageDesyncYaw == 1) {
                cmd->viewangles[1] = -90.0f;
                break;
            }
            cmd->viewangles[1] = 90.0f;
            break;
        case 3:
            if (!desyncActive && g_RageDesyncYaw == 1) {
                cmd->viewangles[1] = -90.0f;
                break;
            }
            cmd->viewangles[1] = 180.0f;
            break;
        }
    }

    // 0x100060E8: Bunny Hop auto-jump, gated by byte_1004ABF0 (menu
    // "Bunny Hop" / config "AUTOBH"). State bytes 0x1004B218 (request) and
    // 0x1004B1E0 (state) reproduce the target's jump pulse; the target skips
    // clearing the request byte on the branch where jump is held.
    if (g_BunnyHopEnabled && !g_SmacMode) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100)) {
            bool jumpHeldBranch = false;
            if (g_BhopState || !g_BhopRequest) {
                if ((cmd->buttons & 2) != 0) {
                    jumpHeldBranch = true;
                    if ((GetEntityFlags(local) & 1) != 0) {
                        g_BhopRequest = 1;
                        g_BhopState = 1;
                    } else {
                        cmd->buttons &= ~2u;
                        g_BhopState = 0;
                    }
                } else {
                    g_BhopState = 0;
                }
            } else {
                cmd->buttons |= 2u;
            }
            if (!jumpHeldBranch)
                g_BhopRequest = 0;
        }
    }

    // 0x1000613E: Lag Exploit (byte_1004B03C; slider dword_1004B038, key
    // dword_1004B034). While the key is held and the slider value is > 0,
    // add the slider value to engine slot-74 field +8 on every update.
    if (g_LagExploit && g_LagFast > 0 && GetAsyncKeyState(g_LagKey))
        AddEngineSlot74Field8(g_LagFast);

    // 0x10006184: Instant Weapon Switch (menu flag byte_1004B00B; the target
    // has no config key for it). On a weaponselect change, add 100 to engine
    // slot-74 field +8. The cached selection updates only while the option
    // is enabled (target behavior).
    if (g_InstantWeaponSwitch) {
        if (g_LastWeaponSelect != cmd->weaponselect)
            AddEngineSlot74Field8(100);
        g_LastWeaponSelect = cmd->weaponselect;
    }

    // 0x100061BD: Instant Revive (menu-only flag byte_1004B00C; no config
    // key in the target). While IN_USE (0x20) is held, add 500 to engine
    // slot-74 field +8 every 200 ms.
    if (g_InstantRevive && (cmd->buttons & 0x20) != 0) {
        static LARGE_INTEGER timer = {};
        if (TimerElapsedMs(&timer, 200))
            AddEngineSlot74Field8(500);
    }

    // 0x10006232: No Fall Damage. While airborne, the target bails out of the
    // rest of Cheat_Update for an infected player (team 3) or a held jump
    // button; otherwise, if fall velocity exceeds 360, forwardmove becomes
    // NaN for one tick, rate-limited to 300 ms. The original build's feature
    // is non-functional in practice (verified against original/target.dll);
    // the code below is a faithful port of that behavior.
    if (g_NoFallDamage) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100) &&
            (GetEntityFlags(local) & 1) == 0) {
            if (GetEntityTeam(local) == 3 || (cmd->buttons & 2) != 0)
                return;
            if (GetEntityFallVelocity(local) > 360.0f) {
                static LARGE_INTEGER timer = {};
                if (TimerElapsedMs(&timer, 300)) {
                    // 0x7FC00000 = quiet NaN, matching the target's
                    // `mov dword ptr [this+24], 7FC00000h`.
                    *reinterpret_cast<uint32_t *>(&cmd->forwardmove) =
                        0x7FC00000u;
                }
            }
        }
    }

    // 0x10006385: Anti Smoker (byte_1004B00E). If the local player has
    // DT_TerrorPlayer::m_isHangingFromTongue set: mode 0 "Always" writes
    // 0x1000000 to cmd->tick_count; mode 1 "Reach Smoker" writes NaN to
    // cmd->forwardmove. Other mode values do nothing (target static is -1).
    if (g_AntiSmoker) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100) &&
            GetEntityHangingFromTongue(local) != 0) {
            if (g_SmokerMode == 0) {
                cmd->tick_count = 0x1000000;
            } else if (g_SmokerMode == 1) {
                *reinterpret_cast<uint32_t *>(&cmd->forwardmove) =
                    0x7FC00000u;
            }
        }
    }

    // 0x1000647C..0x10006F60: Rage aimbot section, gated on Rage Aimbot
    // Enabled (byte_1004AB7B). Auto Shove (ABC4) is ported; the melee
    // weapon-id-19 path (0x100067B7..0x10006A02) and the SMAC-mode angle
    // prediction block (0x10006C2D..0x10006D97) remain unported.
    bool skipLegit = false;
    if (g_RageAimbotEnabled) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100)) {
            void *weapon = GetLocalActiveWeapon(local);
            int weaponId = weapon ? GetEntityWeaponId(weapon) : -1;

            // 0x10006489..0x100067B7: Rage Auto Shove, before the clip
            // check. The target's SMAC-mode prediction branch and the
            // post-shove anti-aim movement adjustments remain PENDING.
            if (g_RageAutoShove && Legit_WeaponAllowed(weaponId))
                Rage_AutoShove(cmd, local);

            bool weaponHasClip = LocalWeaponHasClip(local);
            if (weaponHasClip) {
                // 0x10006B14: save the command angles and movement before
                // any aimbot/prediction modification (used by PSilent).
                const float savedAngles[3] = {cmd->viewangles[0],
                                              cmd->viewangles[1],
                                              cmd->viewangles[2]};
                const float savedForward = cmd->forwardmove;
                const float savedSide = cmd->sidemove;

                bool canFire = false;
                if (weapon && g_pGlobalVars) {
                    const float curTime =
                        static_cast<float>(GetEntityTickBase(local) + 1) *
                        g_pGlobalVars->interval_per_tick;
                    canFire =
                        GetEntityNextPrimaryAttack(weapon) <= curTime;
                }

                if (GetEntityTeam(local) == 2) {
                    if (Legit_WeaponAllowed(weaponId) &&
                        (GetAsyncKeyState(g_RageAimKey) & 0x8000) != 0) {
                        const bool shotgunType =
                            weaponId == 11 || weaponId == 8 ||
                            weaponId == 3 || weaponId == 4;
                        if (shotgunType || canFire) {
                            // 0x10006E1C: perfect silent only without Fake
                            // Lag and without SMAC mode; the command is
                            // restored after the call unless a shot went out.
                            if (g_RagePerfectSilentAim && !g_FakeLag &&
                                !g_SmacMode) {
                                Rage_Aimbot(cmd, local);
                                if ((cmd->buttons & 1) != 0 && canFire) {
                                    g_RageAttackSuppressed = false;
                                } else {
                                    g_RageAttackSuppressed = true;
                                    cmd->viewangles[0] = savedAngles[0];
                                    cmd->viewangles[1] = savedAngles[1];
                                    cmd->viewangles[2] = savedAngles[2];
                                    cmd->forwardmove = savedForward;
                                    cmd->sidemove = savedSide;
                                }
                            } else {
                                Rage_Aimbot(cmd, local);
                            }
                        } else {
                            // Target 0x10006E16: skips Rage No-Recoil and
                            // the whole Legit section.
                            skipLegit = true;
                        }
                    }
                    if (!skipLegit && g_RageNoRecoil) {
                        float punch[3];
                        GetEntityPunchAngle(local, punch);
                        cmd->viewangles[0] -= punch[0];
                        cmd->viewangles[1] -= punch[1];
                        cmd->viewangles[2] -= punch[2];
                    }

                    // 0x10006F81: Rage No-Spread, after No-Recoil. The
                    // target re-checks the weapon whitelist and IN_ATTACK
                    // here; the port applies those gates itself. The
                    // canFire-failure path returns before this point.
                    if (!skipLegit && g_RageNoSpread)
                        NoSpread_Rage(cmd, local, weapon);
                }
            }
        }
    }

    // 0x10007222..0x100073B3: Legit block. The target gates it on
    // byte_1004AB3C (Legit "Enabled"), team == 2 (survivor) and a non-empty
    // active-weapon clip (sub_100046B0). Triggerbot (sub_100353F0), Legit
    // aimbot (sub_10035BF0), No-Recoil and No-Spread are ported; the target
    // never reads byte_1004AFFC ("Enabled##trigger"), so the trigger key
    // alone arms it. Auto Shove (sub_100359D0) is dead by construction.
    if (!skipLegit && g_LegitEnabled) {
        void *local = GetLocalPlayerEntity();
        if (local && !IsBadReadPtr(local, 0x100)) {
            int team = GetEntityTeam(local);
            bool weaponHasClip = LocalWeaponHasClip(local);
            if (team == 2 && weaponHasClip) {
                // 0x1000725B: save the command yaw before any aimbot
                // modification; the movement fix at the tail uses it.
                const float savedYaw = cmd->viewangles[1];
                void *weapon = GetLocalActiveWeapon(local);
                int weaponId = weapon ? GetEntityWeaponId(weapon) : -1;

                // 0x1000725B..0x100072A9 can-fire gate. The target also
                // reads byte_1004B2E4, which no code ever sets, so only the
                // next-primary-attack comparison has an effect.
                bool canFire = false;
                if (weapon && g_pGlobalVars) {
                    const float curTime =
                        static_cast<float>(GetEntityTickBase(local) + 1) *
                        g_pGlobalVars->interval_per_tick;
                    canFire =
                        GetEntityNextPrimaryAttack(weapon) <= curTime;
                }

                if ((GetAsyncKeyState(g_LegitTriggerKey) & 0x8000) != 0)
                    Legit_Triggerbot(cmd, local);

                // 0x100072DA..0x1000734B: whitelist + aim-key + can-fire
                // gate around the aimbot. A held aim key on a non-shotgun
                // weapon that cannot fire skips the No-Recoil block too.
                bool skipNoRecoil = false;
                if (Legit_WeaponAllowed(weaponId) &&
                    (GetAsyncKeyState(g_LegitAimKey) & 0x8000) != 0) {
                    const bool shotgunType =
                        weaponId == 11 || weaponId == 8 || weaponId == 3 ||
                        weaponId == 4;
                    if (shotgunType || canFire) {
                        Legit_Aimbot(cmd, local);
                    } else {
                        skipNoRecoil = true;
                    }
                }

                if (!skipNoRecoil && g_LegitNoRecoil) {
                    float punch[3];
                    GetEntityPunchAngle(local, punch);
                    cmd->viewangles[0] -= punch[0];
                    cmd->viewangles[1] -= punch[1];
                    cmd->viewangles[2] -= punch[2];
                }

                // 0x100073BA: Legit No-Spread, after No-Recoil and before
                // the movement fix. The target's whitelist and IN_ATTACK
                // gates are re-applied inside the port; the canFire-failure
                // path returns before reaching this block.
                if (!skipNoRecoil && g_LegitNoSpread)
                    NoSpread_Legit(cmd, local, weapon);

                // 0x1000757C..0x1000767A: movement fix. Re-express the
                // command movement vector in the (possibly aimbot-rotated)
                // view frame so the world-space direction is preserved.
                // sub_10036890's atan2 legs and the 90/270 zero-move special
                // case are reproduced; its pitch result is unused here.
                if (!skipNoRecoil) {
                    const float forward = cmd->forwardmove;
                    const float side = cmd->sidemove;
                    const float magnitude =
                        sqrtf(forward * forward + side * side);
                    float moveYaw = 0.0f;
                    if (!(forward == 0.0f && side == 0.0f)) {
                        moveYaw = atan2f(side, forward) * 57.295776f;
                        if (moveYaw < 0.0f)
                            moveYaw += 360.0f;
                    }
                    const float moveAngle =
                        (cmd->viewangles[1] - savedYaw + moveYaw) *
                        0.017453292f;
                    cmd->forwardmove = cosf(moveAngle) * magnitude;
                    cmd->sidemove = sinf(moveAngle) * magnitude;
                }
            }
        }
    }
}
