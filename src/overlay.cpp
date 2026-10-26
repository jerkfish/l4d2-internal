// VGUI overlay drawing recovered from the target: font creation (EndScene
// init), sub_1000C390 formatted text, sub_1000C2A0 border rect,
// sub_1000C1A0 bar, and the sub_1003AD40 "Monitor" panel.
#include "overlay.h"

#include "interfaces.h"
#include "netprop.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

int g_FontArial14 = 0;    // 0x1004A9C8
int g_FontVisitor13 = 0;  // 0x1004A9D0
int g_FontTahoma24 = 0;   // 0x1004A9CC
static bool g_FontsCreated = false;

typedef int(__thiscall *CreateFontFn)(void *);
typedef void(__thiscall *SetFontFn)(void *, int, const char *, int, int, int,
                                    int, int, int, int);
typedef void(__thiscall *SetColorFn)(void *, int, int, int, int);
typedef void(__thiscall *FilledRectFn)(void *, int, int, int, int);
typedef void(__thiscall *OutlinedRectFn)(void *, int, int, int, int);
typedef void(__thiscall *DrawLineFn)(void *, int, int, int, int);
typedef void(__thiscall *SetTextFontFn)(void *, int);
typedef void(__thiscall *SetTextColorFn)(void *, int, int, int, int);
typedef void(__thiscall *SetTextPosFn)(void *, int, int);
// VGUI_Surface031 slot 22: DrawPrintText(const wchar_t *text, int textLen,
// FontDrawType_t drawType). The target pushes exactly three arguments
// (0x1000C65E: push 0 / push len / push text). Passing a fourth argument is
// callee-clean stack corruption and crashed every text draw.
typedef void(__thiscall *DrawPrintTextFn)(void *, const wchar_t *, int, int);
typedef void(__thiscall *GetTextSizeFn)(void *, int, const wchar_t *, int *,
                                        int *);

bool OverlayFontsReady() { return g_FontsCreated; }

void OverlaySetColor(int r, int g, int b, int a) {
    if (!g_pVGuiSurface || IsBadReadPtr(g_pVGuiSurface, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 13))
        return;
    reinterpret_cast<SetColorFn>(vtable[11])(g_pVGuiSurface, r, g, b, a);
}

void OverlayFilledRect(int x0, int y0, int x1, int y1) {
    if (!g_pVGuiSurface || IsBadReadPtr(g_pVGuiSurface, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 13))
        return;
    reinterpret_cast<FilledRectFn>(vtable[12])(g_pVGuiSurface, x0, y0, x1,
                                               y1);
}

void OverlayOutlinedRect(int x0, int y0, int x1, int y1) {
    if (!g_pVGuiSurface || IsBadReadPtr(g_pVGuiSurface, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 15))
        return;
    reinterpret_cast<OutlinedRectFn>(vtable[14])(g_pVGuiSurface, x0, y0, x1,
                                                 y1);
}

void OverlayLine(int x0, int y0, int x1, int y1) {
    if (!g_pVGuiSurface || IsBadReadPtr(g_pVGuiSurface, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 16))
        return;
    reinterpret_cast<DrawLineFn>(vtable[15])(g_pVGuiSurface, x0, y0, x1, y1);
}

// sub_1000C390: printf-style VGUI text (CP_UTF8 -> UTF-16).
void OverlayTextV(int font, bool centered, int x, int y, int r, int g, int b,
                  int a, const char *format, va_list args) {
    if (!g_pVGuiSurface || !format || !g_FontsCreated || font <= 0)
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 73))
        return;

    // VGUI text with cvttss2si(inf/NaN) (INT_MIN) positions drove the engine
    // into a null call; never forward absurd coordinates (INFERRED safety
    // guard, see NOTES/inferences.md INF-015).
    const int kCoordLimit = 32767;
    if (x < -kCoordLimit || x > kCoordLimit || y < -kCoordLimit ||
        y > kCoordLimit)
        return;

    char buffer[260];
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);

    int drawX = x;

    int wideCount = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
    if (wideCount <= 1 || wideCount > 260)
        return;
    // Target helper uses a fixed stack wide-char buffer; avoid heap churn on
    // the render thread during horde-scale ESP drawing.
    wchar_t wideText[260];
    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wideText, wideCount);

    if (centered) {
        int width = 0;
        int height = 0;
        reinterpret_cast<GetTextSizeFn>(vtable[72])(
            g_pVGuiSurface, font, wideText, &width, &height);
        drawX = x - width / 2;
        if (drawX < -kCoordLimit || drawX > kCoordLimit)
            return;
    }

    reinterpret_cast<SetTextColorFn>(vtable[19])(g_pVGuiSurface, r, g, b, a);
    reinterpret_cast<SetTextFontFn>(vtable[17])(g_pVGuiSurface, font);
    reinterpret_cast<SetTextPosFn>(vtable[20])(g_pVGuiSurface, drawX, y);
    reinterpret_cast<DrawPrintTextFn>(vtable[22])(
        g_pVGuiSurface, wideText, wideCount - 1, 0);
}

void OverlayText(int font, bool centered, int x, int y, int r, int g, int b,
                 int a, const char *format, ...) {
    va_list args;
    va_start(args, format);
    OverlayTextV(font, centered, x, y, r, g, b, a, format, args);
    va_end(args);
}

// sub_1000C2A0: 1px border drawn with four filled rects.
void DrawBorderRect(int x, int y, int width, int height, int r, int g, int b,
                    int a) {
    OverlaySetColor(r, g, b, a);
    OverlayFilledRect(x, y, x + width, y + 1);
    OverlaySetColor(r, g, b, a);
    OverlayFilledRect(x, y, x + 1, y + height);
    OverlaySetColor(r, g, b, a);
    OverlayFilledRect(x + width, y, x + width + 1, y + height + 1);
    OverlaySetColor(r, g, b, a);
    OverlayFilledRect(x, y + height, x + width, y + height + 1);
}

// sub_1000C1A0: 30x160 green->red bar plus a Visitor TT2 BRK label below it.
void DrawBar(int x, int y, int value, const char *label) {
    if (value > 100)
        value = 100;
    else if (value < 0)
        value = 0;

    OverlaySetColor(static_cast<int>(static_cast<double>(value) * 2.55),
                    255 - static_cast<int>(static_cast<double>(value) * 2.55),
                    0, 255);
    OverlayFilledRect(x, y, x + 30, y + 160);

    OverlaySetColor(0, 0, 0, 255);
    OverlayFilledRect(
        x, y, x + 30,
        y + static_cast<int>(160.0 - static_cast<double>(value) * 1.6));

    OverlayText(g_FontVisitor13, false, x, y + 169, 255, 255, 255, 255, "%s",
                label);
}

void CreateOverlayFonts() {
    if (g_FontsCreated || !g_pVGuiSurface ||
        IsBadReadPtr(g_pVGuiSurface, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pVGuiSurface);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 65))
        return;

    CreateFontFn createFont = reinterpret_cast<CreateFontFn>(vtable[63]);
    SetFontFn setFont = reinterpret_cast<SetFontFn>(vtable[64]);

    // Target order: Arial 14/700, Visitor TT2 BRK 13/500, Tahoma 13/700
    // (handle immediately overwritten), Tahoma 24/700, Tahoma 24/700.
    g_FontArial14 = createFont(g_pVGuiSurface);
    setFont(g_pVGuiSurface, g_FontArial14, "Arial", 14, 700, 0, 0, 512, 0, 0);

    g_FontVisitor13 = createFont(g_pVGuiSurface);
    setFont(g_pVGuiSurface, g_FontVisitor13, "Visitor TT2 BRK", 13, 500, 0, 0,
            512, 0, 0);

    int tahoma13 = createFont(g_pVGuiSurface);
    setFont(g_pVGuiSurface, tahoma13, "Tahoma", 13, 700, 0, 0, 512, 0, 0);

    // Target overwrites the Tahoma-13 global with this Tahoma-24 handle.
    int tahoma24a = createFont(g_pVGuiSurface);
    setFont(g_pVGuiSurface, tahoma24a, "Tahoma", 24, 700, 0, 0, 512, 0, 0);

    g_FontTahoma24 = createFont(g_pVGuiSurface);
    setFont(g_pVGuiSurface, g_FontTahoma24, "Tahoma", 24, 700, 0, 0, 512, 0,
            0);

    g_FontsCreated = true;
}

bool IsLocalPlayerAlive(void *entity) {
    return entity && GetEntityLifeState(entity) == 0 &&
           GetEntityHealth(entity) > 0;
}

// This panel is never reached in the original: PaintTraverse only calls it
// when dword_1004B048 != 0, and no writer for that gate exists. The geometry
// is kept as recovered reference (target position globals dword_1004B04C/
// dword_1004B050 default to 400.0f).
void DrawIndicatorsPanel(void *localEntity) {
    if (!localEntity || !g_pVGuiSurface || !g_FontsCreated)
        return;

    float posX = 400.0f;
    float posY = 400.0f;

    int innerY = static_cast<int>(posY + 50.0f) - 100;
    int innerX = static_cast<int>(posX + 25.0f) - 50;
    int outerX = static_cast<int>(posX + 25.0f) - 56;
    int outerY = innerY - 20;

    OverlaySetColor(35, 35, 35, 255);
    OverlayFilledRect(outerX, outerY, outerX + 112, outerY + 226);
    DrawBorderRect(outerX, outerY, 112, 226, 0, 0, 0, 254);

    OverlayText(g_FontArial14, false, innerX, outerY + 1, 255, 255, 255, 255,
                "Monitor");

    OverlaySetColor(50, 50, 50, 255);
    OverlayFilledRect(innerX, innerY, innerX + 100, innerY + 200);
    DrawBorderRect(innerX, innerY, 100, 200, 0, 0, 0, 254);

    float velocity[3];
    GetEntityVelocity(localEntity, velocity);
    float speed = std::sqrt(velocity[0] * velocity[0] +
                            velocity[1] * velocity[1]);

    float punch[3];
    GetEntityPunchAngle(localEntity, punch);
    float punchMag = std::sqrt(punch[0] * punch[0] + punch[1] * punch[1] +
                               punch[2] * punch[2]);

    float interval = g_pGlobalVars ? g_pGlobalVars->interval_per_tick : 0.0f;
    float decay = (punchMag * 0.5f + 10.0f) * interval;
    float factor = (1.0f / (punchMag + 1.1920928955078125e-07f)) * punch[0];
    float recoilF = -((punchMag - decay) * factor * 17.77777777777778 * 1.25);
    int recoil = static_cast<int>(recoilF);

    int speedValue =
        static_cast<int>(static_cast<double>(static_cast<int>(speed)) * 0.2);

    DrawBar(innerX + 9, innerY + 12, recoil, "Recoil");
    DrawBar(innerX + 59, innerY + 59, speedValue, "Speed");
}
