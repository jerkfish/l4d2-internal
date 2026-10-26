// ESP painter recovered from sub_1003C410, with the helpers it calls:
//   sub_1003B030  bounding box (8 projected points, radius |mins.y-maxs.y|/1.3)
//   sub_1003D7D0  box (colored outline + two black outlines)
//   sub_1003D870  health bar + numeric health (sub_1003A8C0 std::to_string)
//   sub_1003DA50  full health bar (zombies/witch)
//   sub_10001BA0  m_isGhost (called but unused in the target's draw path)
//   sub_1003AB50  weapon-ID -> display name
//   sub_10001AA0  DT_BaseCombatCharacter::m_hActiveWeapon
//   sub_100014B0  DT_BasePlayer::m_vecViewOffset + GetAbsOrigin (eye position)
//   sub_1003AA60  DT_BaseEntity::m_vecOrigin (line ESP anchor)
#include "esp.h"

#include "interfaces.h"
#include "netprop.h"
#include "overlay.h"
#include "recovered.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <xmmintrin.h>

namespace {

const float kRadToDeg = 57.2957763671875f;  // 0x100473CC
const float kDegToRad = 0.01745329238474369f; // 0x100472CC
const float kMetersPerUnit = 0.02539999969303608f; // 0x100472D0


// VGUI refuses/clips sane screen coordinates; passing cvttss2si(inf/NaN)
// (= INT_MIN) into the text renderer drove the engine into a null call
// (CRASH n=1 eip=0, phase=distance). The target ignores its visibility byte,
// so this sanity check is an INFERRED deviation: only entities whose
// projected box is finite and remotely on-screen are drawn.
bool IsBboxSane(const int bbox[4]) {
    const int kLimit = 32767;
    for (int i = 0; i < 4; ++i) {
        if (bbox[i] < -kLimit || bbox[i] > kLimit)
            return false;
    }
    if (bbox[2] < bbox[0] || bbox[3] < bbox[1])
        return false;
    return true;
}

// L4D2's player_info_t is ~332 bytes (xuid, name[128], userID, guid[33],
// friendsID, friendsName[128], flags...). The target reserves ~0x150 bytes;
// an undersized buffer lets GetPlayerInfo overwrite the caller's frame.
struct PlayerInfoRaw {
    unsigned char pad[8];
    char name[128];
    unsigned char tail[376]; // 512 bytes total
};
static_assert(sizeof(PlayerInfoRaw) == 512, "player_info buffer too small");

typedef bool(__thiscall *GetPlayerInfoFn)(void *, int, void *);
typedef void(__thiscall *GetScreenSizeFn)(void *, int *, int *);
typedef int(__thiscall *ScreenPositionFn)(void *, const float *, float *);

bool GetPlayerInfo(void *engine, int index, PlayerInfoRaw *info) {
    if (!engine || IsBadReadPtr(engine, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(engine);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 9))
        return false;
    return reinterpret_cast<GetPlayerInfoFn>(vtable[8])(engine, index, info);
}

void GetScreenSize(void *engine, int *width, int *height) {
    *width = 0;
    *height = 0;
    if (!engine || IsBadReadPtr(engine, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(engine);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 6))
        return;
    reinterpret_cast<GetScreenSizeFn>(vtable[5])(engine, width, height);
}

// IVDebugOverlay::ScreenPosition (slot 12). Returns 0 when on screen, 1 when
// the point is behind the view (0x1003B030 treats 1 as "not visible").
int ScreenPosition(const float in[3], float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!g_pDebugOverlay || IsBadReadPtr(g_pDebugOverlay, sizeof(void *)))
        return 1;
    void **vtable = *reinterpret_cast<void ***>(g_pDebugOverlay);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 13))
        return 1;
    return reinterpret_cast<ScreenPositionFn>(vtable[12])(g_pDebugOverlay, in,
                                                          out);
}

float NormalizeAngle(float angle) {
    while (angle < -180.0f)
        angle += 360.0f;
    while (angle > 180.0f)
        angle -= 360.0f;
    return angle;
}

// sub_1003B030 compiles its min/max reductions to raw minss/maxss chains,
// with each new value as the destination and the running result as the
// source. That is *not* C99 fminf NaN behaviour, so reproduce the exact
// instruction-level sequence with SSE intrinsics. Argument order mirrors the
// target: v81 (out0), v78 (out4), v75 (out1), v72 (out5), v69 (out2),
// v66 (out6), v63 (out3), v60 (out7).
float SseMin8(float v0, float v4, float v1, float v5, float v2, float v6,
              float v3, float v7) {
    __m128 m = _mm_set_ss(v0);
    m = _mm_min_ss(m, m);
    m = _mm_min_ss(_mm_set_ss(v4), m);
    m = _mm_min_ss(_mm_set_ss(v1), m);
    m = _mm_min_ss(_mm_set_ss(v5), m);
    m = _mm_min_ss(_mm_set_ss(v2), m);
    m = _mm_min_ss(_mm_set_ss(v6), m);
    m = _mm_min_ss(_mm_set_ss(v3), m);
    m = _mm_min_ss(_mm_set_ss(v7), m);
    return _mm_cvtss_f32(m);
}

float SseMax8(float v0, float v4, float v1, float v5, float v2, float v6,
              float v3, float v7) {
    __m128 m = _mm_set_ss(v0);
    m = _mm_max_ss(m, m);
    m = _mm_max_ss(_mm_set_ss(v4), m);
    m = _mm_max_ss(_mm_set_ss(v1), m);
    m = _mm_max_ss(_mm_set_ss(v5), m);
    m = _mm_max_ss(_mm_set_ss(v2), m);
    m = _mm_max_ss(_mm_set_ss(v6), m);
    m = _mm_max_ss(_mm_set_ss(v3), m);
    m = _mm_max_ss(_mm_set_ss(v7), m);
    return _mm_cvtss_f32(m);
}

// DT_BaseCombatCharacter::m_hActiveWeapon (sub_10001AA0).
int GetActiveWeaponHandle(void *entity) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseCombatCharacter",
                                      "m_hActiveWeapon");
    if (!entity || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(entity) + offset);
}

// DT_BaseCombatWeapon::m_iClip1 (sub_100046B0 / sub_100374A0 lookup).
int GetWeaponClip(void *weapon) {
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseCombatWeapon", "m_iClip1");
    if (!weapon || offset <= 0 || offset > 0x4000)
        return 0;
    if (IsBadReadPtr(static_cast<char *>(weapon) + offset, sizeof(int)))
        return 0;
    return *reinterpret_cast<int *>(static_cast<char *>(weapon) + offset);
}

// DT_BaseEntity::m_vecOrigin (sub_1003AA60), used as the line-ESP anchor.
void GetNetOrigin(void *entity, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    static int offset = -1;
    if (offset < 0)
        offset = NetpropOffsetInTable("DT_BaseEntity", "m_vecOrigin");
    if (!entity || offset <= 0 || offset > 0x4000 ||
        IsBadReadPtr(static_cast<char *>(entity) + offset, sizeof(float) * 3))
        return;
    memcpy(out, static_cast<char *>(entity) + offset, sizeof(float) * 3);
}

// sub_1003AB50 / sub_1003DB30: weapon ID -> display name.
const char *WeaponNameFromId(int id) {
    switch (id) {
    case 0: return "CS Base";
    case 1: return "Pistol";
    case 2: return "Submachine Gun";
    case 3: return "Pump Shotgun";
    case 4: return "Auto Shotgun";
    case 5: return "Assault Rifle";
    case 6: return "Sniper Rifle";
    case 7: return "Submachine Silenced";
    case 8: return "Chrome Shotgun";
    case 9: return "Desert";
    case 10: return "Military Sniper";
    case 11: return "SPAS";
    case 12: return "First Aid Kit";
    case 13: return "Molotov";
    case 14: return "Pipe Bomb";
    case 15: return "Pain Pills";
    case 16: return "Gas Can";
    case 17: return "Propane Tank";
    case 18: return "Oxygen Tank";
    case 20: return "Chainsaw";
    case 21: return "Grenade Launcher";
    case 22: return "Ammo Pack";
    case 23: return "Adrenaline";
    case 24: return "Defibrillator";
    case 25: return "Vomit Jar";
    case 26: return "AK-47";
    case 27: return "Gnome";
    case 28: return "Cola Bottles";
    case 29: return "Firework Crate";
    case 30: return "Incendiary Upgrade Pack";
    case 31: return "Explosive Upgrade Pack";
    case 32: return "Magnum Pistol";
    case 33: return "MP5";
    case 34: return "SG552";
    case 35: return "AWP";
    case 36: return "Scout";
    case 37: return "M60";
    case 38: return "Tank Claw";
    case 39: return "Hunter Claw";
    case 40: return "Charger Claw";
    case 41: return "Boomer Claw";
    case 42: return "Smoker Claw";
    case 43: return "Spitter Claw";
    case 44: return "Jockey Claw";
    default: return "Unknown Weapon";
    }
}

// sub_1003AB50: weapon entity vtable slot 383 -> display name.
const char *WeaponName(void *weapon) {
    if (!weapon)
        return "Unknown Weapon";
    return WeaponNameFromId(GetEntityWeaponId(weapon));
}

// sub_1003B030. Returns false when any corner is behind the view; the target
// computes the box regardless and its ESP painter ignores this flag.
bool BuildBoundingBox(void *entity, void *local, int bbox[4]) {
    bbox[0] = bbox[1] = bbox[2] = bbox[3] = 0;

    float origin[3];
    float mins[3];
    float maxs[3];
    float localEye[3];
    GetEntityOrigin(entity, origin);
    GetEntityMins(entity, mins);
    GetEntityMaxs(entity, maxs);
    GetEntityEyePosition(local, localEye);

    // Target: (int)((float)(int)sqrt((mins.y-maxs.y)^2 + 0 + 0) / 1.3).
    float heightSq = (mins[1] - maxs[1]) * (mins[1] - maxs[1]);
    int heightInt = static_cast<int>(std::sqrt(heightSq));
    int radius = static_cast<int>(static_cast<float>(heightInt) / 1.3f);

    float dx = origin[0] - localEye[0];
    float dy = origin[1] - localEye[1];
    float dz = origin[2] - localEye[2];
    float horizontal = std::sqrt(dx * dx + dy * dy);

    // libm atan returns a double in the target; the result is truncated to
    // float before the 57.295776 multiply (mulss).
    float pitch =
        static_cast<float>(std::atan(dz / horizontal)) * kRadToDeg;
    float yaw = static_cast<float>(std::atan(dy / dx)) * kRadToDeg;
    if (dx >= 0.0f)
        yaw += 180.0f;
    pitch = NormalizeAngle(pitch);
    yaw = NormalizeAngle(yaw);

    float pitchRad = pitch * kDegToRad;
    float yawRad = (yaw + 45.0f) * kDegToRad;
    float cosPitch = std::cos(pitchRad);

    float points[8][3];
    for (int i = 0; i < 4; ++i) {
        float cosYaw = std::cos(yawRad);
        float sinYaw = std::sin(yawRad);
        float x = origin[0] + cosYaw * cosPitch * static_cast<float>(radius);
        float y = origin[1] + sinYaw * cosPitch * static_cast<float>(radius);
        points[i][0] = x;
        points[i][1] = y;
        points[i][2] = origin[2] + maxs[2];
        points[i + 4][0] = x;
        points[i + 4][1] = y;
        points[i + 4][2] = origin[2] + mins[2];
        yawRad += 90.0f * kDegToRad;
    }

    // Target call order in sub_1003B030 (points: 0..3 = top, 4..7 = bottom):
    // top3, top0, top2, bottom2, bottom1, bottom0, top1, bottom3.
    static const int kCallOrder[8] = {3, 0, 2, 6, 5, 4, 1, 7};
    bool visible = true;
    float out[8][3] = {};
    for (int i = 0; i < 8; ++i) {
        if (ScreenPosition(points[kCallOrder[i]], out[i]) == 1)
            visible = false;
    }

    float minX = SseMin8(out[0][0], out[4][0], out[1][0], out[5][0],
                         out[2][0], out[6][0], out[3][0], out[7][0]);
    float minY = SseMin8(out[0][1], out[4][1], out[1][1], out[5][1],
                         out[2][1], out[6][1], out[3][1], out[7][1]);
    float maxX = SseMax8(out[0][0], out[4][0], out[1][0], out[5][0],
                         out[2][0], out[6][0], out[3][0], out[7][0]);
    float maxY = SseMax8(out[0][1], out[4][1], out[1][1], out[5][1],
                         out[2][1], out[6][1], out[3][1], out[7][1]);

    bbox[0] = static_cast<int>(minX);
    bbox[1] = static_cast<int>(minY);
    bbox[2] = static_cast<int>(maxX);
    bbox[3] = static_cast<int>(maxY);

    return visible;
}

// sub_1003D7D0: colored outline, black outer 1px, black inner 1px.
void DrawBox(const int bbox[4], int r, int g, int b) {
    OverlaySetColor(r, g, b, 255);
    OverlayOutlinedRect(bbox[0], bbox[1], bbox[2], bbox[3]);
    OverlaySetColor(0, 0, 0, 255);
    OverlayOutlinedRect(bbox[0] - 1, bbox[1] - 1, bbox[2] + 1, bbox[3] + 1);
    OverlaySetColor(0, 0, 0, 255);
    OverlayOutlinedRect(bbox[0] + 1, bbox[1] + 1, bbox[2] - 1, bbox[3] - 1);
}

// sub_1003D870: bar anchored at the bottom of the box; the black backdrop
// covers only the filled part. Numeric health is drawn when Player Health
// ESP (byte_1004ABD8) is on.
void DrawHealthBar(const int bbox[4], void *entity) {
    int health = GetEntityHealth(entity);
    int maxHealth = GetEntityMaxHealth(entity);
    if (health > maxHealth)
        health = maxHealth;

    int height = bbox[3] - bbox[1];
    float fraction = 0.0f;
    if (maxHealth > 0)
        fraction = static_cast<float>(health) * static_cast<float>(height) /
                   static_cast<float>(maxHealth);
    int filled = static_cast<int>(fraction);
    int x = static_cast<int>(static_cast<float>(bbox[0]) - 6.0f);

    OverlaySetColor(0, 0, 0, 255);
    OverlayFilledRect(x, bbox[3] - filled, x + 4, bbox[3]);
    OverlaySetColor(0, 255, 0, 255);
    OverlayFilledRect(x + 1, bbox[3] - filled + 1, x + 4, bbox[3]);

    if (g_EspHealth) {
        OverlayText(g_FontArial14, true, x - 10, bbox[3] - filled, 255, 255,
                    255, 255, "%d", health);
    }
}

// sub_1003DA50: full-height bar with no numeric health (zombies/witch).
void DrawFullHealthBar(const int bbox[4]) {
    int height = bbox[3] - bbox[1];
    if (height < 0)
        height = -height;
    int x = static_cast<int>(static_cast<float>(bbox[0]) - 6.0f);

    OverlaySetColor(0, 0, 0, 255);
    OverlayFilledRect(x, bbox[1], x + 4, bbox[1] + height);
    OverlaySetColor(0, 255, 0, 255);
    OverlayFilledRect(x, bbox[1], x + 4, bbox[1] + height - 2);
}

void DrawDistance(void *entity, void *local, int centerX, int y) {
    float localEye[3];
    float entityEye[3];
    GetEntityEyePosition(local, localEye);
    GetEntityEyePosition(entity, entityEye);
    float dx = localEye[0] - entityEye[0];
    float dy = localEye[1] - entityEye[1];
    float dz = localEye[2] - entityEye[2];
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    int meters = static_cast<int>(distance * kMetersPerUnit);
    OverlayText(g_FontArial14, true, centerX, y, 255, 255, 255, 255, "[%d]",
                meters);
}

// Line ESP: entity origin -> screen bottom-center, red.
bool DrawLineToCenter(void *entity) {
    float origin[3];
    GetNetOrigin(entity, origin);
    float screen[3];
    if (ScreenPosition(origin, screen) != 0)
        return false;
    const float kCoordLimit = 32767.0f;
    if (screen[0] < -kCoordLimit || screen[0] > kCoordLimit ||
        screen[1] < -kCoordLimit || screen[1] > kCoordLimit)
        return false;

    int width = 0;
    int height = 0;
    GetScreenSize(g_pEngineClient, &width, &height);
    if (width <= 0 || width > 32768 || height <= 0 || height > 32768)
        return false;

    OverlaySetColor(255, 0, 0, 255);
    OverlayLine(static_cast<int>(screen[0]), static_cast<int>(screen[1]),
                static_cast<int>(static_cast<float>(width) * 0.5f), height);
    return true;
}

void DrawWeaponText(void *weaponEntity, int centerX, int y) {
    const char *name = WeaponName(weaponEntity);
    if (!name)
        return;

    if (weaponEntity && GetWeaponClip(weaponEntity) > 0 && g_EspAmmo) {
        char buffer[64];
        sprintf_s(buffer, sizeof(buffer), "%s [%d]", name,
                  GetWeaponClip(weaponEntity));
        OverlayText(g_FontArial14, true, centerX, y, 255, 255, 255, 255, "%s",
                    buffer);
    } else {
        OverlayText(g_FontArial14, true, centerX, y, 255, 255, 255, 255, "%s",
                    name);
    }
}

} // namespace

void DrawEsp(void *local) {
    if (!local || !g_pClientEntityList)
        return;

    int localTeam = GetEntityTeam(local);
    int highest = GetHighestEntityIndex();


    for (int index = 1; index <= highest; ++index) {

        void *entity = GetClientEntityByIndex(index);
        if (!entity || entity == local)
            continue;
        if (IsEntityDormant(entity))
            continue;
        if (!GetEntityRenderableModelId(entity))
            continue;
        // 0x1003c4ef: skip entities whose m_usSolidFlags == 0x14.
        if (GetEntitySolidFlags(entity) == 20)
            continue;

        int team = GetEntityTeam(entity);
        if (!g_EspShowTeam && team == localTeam)
            continue;

        int category = GetEntityCategory(entity);
        if (!category)
            continue;
        if (!IsPlayerChamsEligible(entity))
            continue;

        int bbox[4];
        bool visible = BuildBoundingBox(entity, local, bbox);

        if (!IsBboxSane(bbox))
            continue;

        int weaponHandle = GetActiveWeaponHandle(entity);
        void *weaponEntity = GetClientEntityFromHandle(weaponHandle);

        int centerX = bbox[0] + (bbox[2] - bbox[0]) / 2;

        PlayerInfoRaw info{};
        GetPlayerInfo(g_pEngineClient, index, &info);
        info.name[127] = '\0';

        if (!g_EspEnable)
            continue;
        if (team != 1 && team != 2 && team != 3)
            continue;

        if (category == 5) {
            if (team == 3) {
                if (g_EspName) {
                    OverlayText(g_FontArial14, true, centerX, bbox[1] - 15,
                                230, 212, 50, 255, "%s", info.name);
                }
                if (g_EspDistance)
                    DrawDistance(entity, local, centerX, bbox[3] + 1);
                if (g_EspHealthBar)
                    DrawHealthBar(bbox, entity);
                if (g_EspBox)
                    DrawBox(bbox, 132, 66, 245);
                if (g_EspLines)
                    DrawLineToCenter(entity);
            }
            if (team == 2) {
                if (g_EspName) {
                    // Target passes the raw name buffer as the format string
                    // here; "%s" is used instead to avoid reading unsupplied
                    // varargs (INFERRED safety deviation).
                    OverlayText(g_FontArial14, true, centerX, bbox[1] - 15,
                                230, 212, 50, 255, "%s", info.name);
                }
                if (g_EspDistance)
                    DrawDistance(entity, local, centerX, bbox[3] + 1);
                if (g_EspHealthBar)
                    DrawHealthBar(bbox, entity);
                if (g_EspBox)
                    DrawBox(bbox, 93, 245, 66);
                if (g_EspWeapons) {
                    int y = g_EspDistance ? bbox[3] + 15 : bbox[3] + 1;
                    DrawWeaponText(weaponEntity, centerX, y);
                }
                if (g_EspLines)
                    DrawLineToCenter(entity);
            }
        }

        if (category == 4) {
            if (g_EspName) {
                if (GetEntityMobRush(entity))
                    OverlayText(g_FontArial14, true, centerX, bbox[1] - 15,
                                230, 212, 50, 255, "zombie - RUSH");
                else
                    OverlayText(g_FontArial14, true, centerX, bbox[1] - 15,
                                230, 212, 50, 255, "zombie");
            }
            if (g_EspDistance)
                DrawDistance(entity, local, centerX, bbox[3] + 1);
            if (g_EspBox)
                DrawBox(bbox, 255, 123, 0);
            if (g_EspHealthBar)
                DrawFullHealthBar(bbox);
        }

        if (category != 3)
            goto category1;

        if (g_EspName) {
            OverlayText(g_FontArial14, true, centerX, bbox[1] - 15, 230, 212,
                        50, 255, "%s", info.name);
        }
        if (g_EspBotTag) {
            OverlayText(g_FontArial14, true, centerX, bbox[1] - 15, 252, 3, 3,
                        255, "\n\n\n\n[BOT]");
        }
        if (g_EspDistance)
            DrawDistance(entity, local, centerX, bbox[3] + 1);
        if (g_EspHealthBar)
            DrawHealthBar(bbox, entity);
        if (g_EspBox)
            DrawBox(bbox, 132, 66, 245);
        if (g_EspLines && !DrawLineToCenter(entity))
            continue; // target jumps to LABEL_124, skipping the weapon text
        if (g_EspWeapons) {
            int y = g_EspDistance ? bbox[3] + 15 : bbox[3] + 1;
            DrawWeaponText(weaponEntity, centerX, y);
        }

    category1:
        if (category != 1)
            goto category2;

        if (g_EspName) {
            OverlayText(g_FontArial14, true, centerX, bbox[1] - 15, 230, 212,
                        50, 255, "Tank");
        }
        if (g_EspDistance)
            DrawDistance(entity, local, centerX, bbox[3] + 1);
        if (g_EspHealthBar)
            DrawHealthBar(bbox, entity);
        if (g_EspBox)
            DrawBox(bbox, 132, 66, 245);
        if (g_EspLines)
            DrawLineToCenter(entity);

    category2:
        if (category != 2)
            continue;

        {
            float rage = GetEntityRage(entity);
            if (g_EspName) {
                OverlayText(g_FontArial14, true, centerX, bbox[1] - 15,
                            230, 212, 50, 255, "Witch - Rage: %.2f",
                            static_cast<double>(rage));
            }
            if (g_EspBotTag) {
                OverlayText(g_FontArial14, true, centerX, bbox[1] - 15, 252,
                            3, 3, 255, "\n\n\n\n\n\nDANGER!");
            }
            if (g_EspDistance)
                DrawDistance(entity, local, centerX, bbox[3] + 5);
            if (g_EspHealthBar)
                DrawFullHealthBar(bbox);
            if (g_EspBox)
                DrawBox(bbox, 132, 66, 245);
            if (g_EspWitchRage) {
                int barX = bbox[0] + 1;
                int barY = static_cast<int>(static_cast<float>(bbox[3]) + 6.0f) + 1;
                int barWidth = static_cast<int>(
                    rage * static_cast<float>(std::abs(bbox[2] - bbox[0])));
                OverlaySetColor(255, 255, 255, 255);
                OverlayFilledRect(barX, barY, barX + barWidth - 1, barY + 3);
            }
            if (g_EspLines)
                DrawLineToCenter(entity);
        }
    }

}

// sub_1003DB30: world weapon-spawn ESP. Iterates every entity, skips the
// local player and dormant entities, and requires the 8-corner bbox to be
// fully visible (sub_1003B030's visibility flag). Only class 260
// (CWeaponSpawn) is labelled, at the center of the projected box, with the
// same weapon-ID table as sub_1003AB50. ID 0 falls through to "Unknown
// Weapon" because the target guards the switch with `if (weaponID)`.
void DrawWeaponSpawnEsp(void *localEntity) {
    if (!localEntity || !g_pClientEntityList)
        return;

    static int weaponIdOffset = -1;
    if (weaponIdOffset < 0)
        weaponIdOffset = NetpropOffsetInTable("DT_WeaponSpawn", "m_weaponID");
    if (weaponIdOffset <= 0 || weaponIdOffset > 0x4000)
        return;

    int highest = GetHighestEntityIndex();
    for (int index = 1; index <= highest; ++index) {
        void *entity = GetClientEntityByIndex(index);
        if (!entity || entity == localEntity)
            continue;
        if (IsEntityDormant(entity))
            continue;

        int bbox[4];
        bool visible = BuildBoundingBox(entity, localEntity, bbox);
        if (!visible)
            continue;
        if (!IsBboxSane(bbox))
            continue;

        if (GetEntityClassId(entity) != 260)
            continue;

        int centerX = (bbox[0] + bbox[2]) / 2;
        int centerY = (bbox[1] + bbox[3]) / 2;

        int weaponId = 0;
        if (!IsBadReadPtr(static_cast<char *>(entity) + weaponIdOffset,
                          sizeof(int))) {
            weaponId =
                *reinterpret_cast<int *>(static_cast<char *>(entity) +
                                         weaponIdOffset);
        }
        const char *name =
            weaponId ? WeaponNameFromId(weaponId) : "Unknown Weapon";
        OverlayText(g_FontArial14, true, centerX, centerY, 255, 255, 255, 255,
                    "%s", name);
    }
}
