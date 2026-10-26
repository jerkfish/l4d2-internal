// VGUI overlay helpers recovered from the target's PaintTraverse path.
#pragma once

#include <cstdarg>

// Font handles created by CreateOverlayFonts (0x1004A9C8/0x1004A9D0/0x1004A9CC).
extern int g_FontArial14;
extern int g_FontVisitor13;
extern int g_FontTahoma24;

// Creates the four live fonts (two Tahoma-24 handles; the target's Tahoma-13
// handle is created and then overwritten, which is reproduced).
void CreateOverlayFonts();
bool OverlayFontsReady();

// sub_100045A0: m_lifeState == 0 && m_iHealth > 0.
bool IsLocalPlayerAlive(void *entity);

// VGUI_Surface031 primitives (slots 11/12/14/15) and sub_1000C390 text.
void OverlaySetColor(int r, int g, int b, int a);
void OverlayFilledRect(int x0, int y0, int x1, int y1);
void OverlayOutlinedRect(int x0, int y0, int x1, int y1);
void OverlayLine(int x0, int y0, int x1, int y1);
void OverlayText(int font, bool centered, int x, int y, int r, int g, int b,
                 int a, const char *format, ...);
void OverlayTextV(int font, bool centered, int x, int y, int r, int g, int b,
                  int a, const char *format, va_list args);

// sub_1000C2A0 (1px border) and sub_1000C1A0 (30x160 bar + Visitor label).
void DrawBorderRect(int x, int y, int width, int height, int r, int g, int b,
                    int a);
void DrawBar(int x, int y, int value, const char *label);

// sub_1003AD40: "Monitor" panel with Recoil/Speed bars. The target only calls
// this when dword_1004B048 != 0, and nothing in the binary ever writes that
// gate, so the shipped behaviour is "never drawn"; kept for reference.
void DrawIndicatorsPanel(void *localEntity);
