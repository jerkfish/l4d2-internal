// Radar overlay: sub_1003BFB0 (frame), sub_1003B820 (entity dots), and
// sub_1003C1D0 (rotate/clamp/project, draw dot + optional name).
//
// Target behaviour notes:
//  - The frame is centered at (window.w*0.5+left, window.h*0.5+top+10); the
//    dots are centered 10px higher (sub_1003C1D0 does not add the +10).
//  - Only entities whose team != 2 are considered (threats/other team).
//  - Dot label x is the saved EDI argument, which the caller loads with
//    viewangles.z (roll) - in practice 0, so labels draw at x=2. This quirk
//    is reproduced.
#include "radar.h"

#include "interfaces.h"
#include "netprop.h"
#include "overlay.h"
#include "recovered.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"

// Target 0x1004A4B4/A4B8 (window Pos) and 0x1004A4BC/A4C0 (window Size),
// copied out of the movable radar window each EndScene.
float g_RadarWindowPos[2] = {};
float g_RadarWindowSize[2] = {};

namespace {

const float kRadarHalfExtent = 3300.0f;  // 0x10047428 / 0x10047458
const float kRadarScale = 110.0f;        // 0x100473E8
const double kDegToRadDouble = 0.017453293005625408;  // 0x10047330
const float kRadarBox = 100.0f;          // clamp radius around center

typedef void(__thiscall *GetViewAnglesFn)(void *, float *);

void GetViewAngles(float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 20))
        return;
    reinterpret_cast<GetViewAnglesFn>(vtable[19])(g_pEngineClient, out);
}

// sub_1003C1D0. cos/sin use the target's float->double->float sequence.
void DrawRadarDot(float entityX, float entityY, float localX, float localY,
                  const float angles[3], int red, int green, int blue,
                  const char *label, int centerX, int centerY) {
    float dx = entityX - localX;
    float dy = entityY - localY;

    float angleRad =
        static_cast<float>(static_cast<double>(angles[1]) * kDegToRadDouble);
    float cosA = static_cast<float>(std::cos(static_cast<double>(angleRad)));
    float sinA = static_cast<float>(std::sin(static_cast<double>(angleRad)));

    float v19 = sinA * dx - cosA * dy;
    // target: (-cos)*dx - sin*dy
    float v20 = (-cosA) * dx - sinA * dy;

    if (std::fabs(v19) > kRadarHalfExtent ||
        std::fabs(v20) > kRadarHalfExtent) {
        float negated = -v19;
        if (v20 <= v19) {
            if (v20 <= negated) {
                v19 = (v19 * -kRadarHalfExtent) / v20;
                v20 = -kRadarHalfExtent;
            } else {
                v20 = (v20 * kRadarHalfExtent) / v19;
                v19 = kRadarHalfExtent;
            }
        } else if (v20 <= negated) {
            v20 = (v20 * -kRadarHalfExtent) / v19;
            v19 = -kRadarHalfExtent;
        } else {
            v19 = (v19 * kRadarHalfExtent) / v20;
            v20 = kRadarHalfExtent;
        }
    }

    int x = centerX -
            static_cast<int>((v19 / -kRadarHalfExtent) * kRadarScale);
    int y = centerY -
            static_cast<int>((v20 / -kRadarHalfExtent) * kRadarScale);

    if (x < centerX - static_cast<int>(kRadarBox))
        x = centerX - static_cast<int>(kRadarBox) + 1;
    else if (x > centerX + static_cast<int>(kRadarBox))
        x = centerX + static_cast<int>(kRadarBox) - 3;
    if (y < centerY - static_cast<int>(kRadarBox))
        y = centerY - static_cast<int>(kRadarBox) + 2;
    else if (y > centerY + static_cast<int>(kRadarBox))
        y = centerY + static_cast<int>(kRadarBox) - 4;

    OverlaySetColor(red, green, blue, 255);
    OverlayFilledRect(x - 3, y - 3, x + 3, y + 3);

    if (g_RadarNames && label && label[0]) {
        unsigned int rollBits = 0;
        memcpy(&rollBits, &angles[2], sizeof(rollBits));
        int labelX = static_cast<int>(rollBits) + 2;  // target's EDI quirk
        OverlayText(g_FontArial14, true, labelX, y + 4, 255, 255, 255, 255,
                    "%s", label);
    }
}

} // namespace

void DrawRadar(void *localEntity) {
    if (!localEntity)
        return;

    if (g_RadarWindowSize[0] < 2.0f || g_RadarWindowSize[1] < 2.0f)
        return;
    const int frameCenterX = static_cast<int>(
        g_RadarWindowSize[0] * 0.5f + g_RadarWindowPos[0]);
    const int frameCenterY = static_cast<int>(
        g_RadarWindowSize[1] * 0.5f + g_RadarWindowPos[1] + 10.0f);
    const int dotCenterX = static_cast<int>(
        g_RadarWindowSize[0] * 0.5f + g_RadarWindowPos[0]);
    const int dotCenterY = static_cast<int>(
        g_RadarWindowSize[1] * 0.5f + g_RadarWindowPos[1]);

    // sub_1003BFB0: 292x306 background, 292x306 + 280x280 borders, crosshair.
    OverlaySetColor(35, 35, 35, 255);
    OverlayFilledRect(frameCenterX - 146, frameCenterY - 160,
                      frameCenterX - 146 + 292, frameCenterY - 160 + 306);
    DrawBorderRect(frameCenterX - 146, frameCenterY - 160, 292, 306, 255, 255,
                   255, 0);
    DrawBorderRect(frameCenterX - 140, frameCenterY - 140, 280, 280, 255, 255,
                   255, 0);
    OverlaySetColor(255, 255, 255, 255);
    OverlayFilledRect(frameCenterX, frameCenterY - 140, frameCenterX + 1,
                      frameCenterY - 140 + 280);
    OverlaySetColor(255, 255, 255, 255);
    OverlayFilledRect(frameCenterX - 140, frameCenterY,
                      frameCenterX - 140 + 280, frameCenterY + 1);

    float angles[3];
    GetViewAngles(angles);
    float localOrigin[3];
    GetEntityOrigin(localEntity, localOrigin);

    int highest = GetHighestEntityIndex();
    for (int index = 1; index <= highest; ++index) {
        void *entity = GetClientEntityByIndex(index);
        if (!entity)
            continue;
        if (IsEntityDormant(entity))
            continue;
        if (!GetEntityRenderableModelId(entity))
            continue;
        if (GetEntityTeam(entity) == 2)
            continue;

        int category = GetEntityCategory(entity);
        if (!category && !IsPlayerChamsEligible(entity))
            continue;

        float origin[3];
        GetEntityOrigin(entity, origin);

        if (entity == localEntity) {
            DrawRadarDot(localOrigin[0], localOrigin[1], localOrigin[0],
                         localOrigin[1], angles, 255, 255, 255, "You",
                         dotCenterX, dotCenterY);
        }

        int classId = GetEntityClassId(entity);
        if (category == 4) {
            // Target passes the empty global named "Default".
            DrawRadarDot(origin[0], origin[1], localOrigin[0], localOrigin[1],
                         angles, 255, 123, 0, "", dotCenterX, dotCenterY);
        }
        if (category == 1 && classId == 276) {
            DrawRadarDot(origin[0], origin[1], localOrigin[0], localOrigin[1],
                         angles, 255, 0, 0, "Tank", dotCenterX, dotCenterY);
        }
        if (category == 2 && classId == 277) {
            float rage = GetEntityRage(entity);
            if (rage == 1.0f) {
                DrawRadarDot(origin[0], origin[1], localOrigin[0],
                             localOrigin[1], angles, 255, 0, 0, "Witch",
                             dotCenterX, dotCenterY);
            } else {
                DrawRadarDot(origin[0], origin[1], localOrigin[0],
                             localOrigin[1], angles, 16, 252, 8, "Witch",
                             dotCenterX, dotCenterY);
            }
        }
        if (category == 3) {
            const char *label = nullptr;
            switch (classId) {
            case 270: label = "Smoker"; break;
            case 265: label = "Jockey"; break;
            case 99: label = "Charger"; break;
            case 272: label = "Spitter"; break;
            case 263: label = "Hunter"; break;
            default: break;
            }
            if (label) {
                DrawRadarDot(origin[0], origin[1], localOrigin[0],
                             localOrigin[1], angles, 255, 0, 0, label,
                             dotCenterX, dotCenterY);
                }
        }
    }

}
