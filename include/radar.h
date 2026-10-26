// Radar overlay recovered from sub_1003BFB0 (frame) + sub_1003B820
// (entity dots) + sub_1003C1D0 (relative projection/dot/label).
#pragma once

// Called from PaintTraverse after the ESP painter when byte_1004AC30 (Radar)
// is enabled. local is the local player entity (may be null).
void DrawRadar(void *localEntity);

// Radar window geometry, refreshed each frame from the movable ImGui "Radar"
// window (target Hook_EndScene 0x10009B4D: 292x306, WindowBg overridden to
// 0.2,0.2,0.2,alpha 0). sub_1003BFB0 centers its frame on
// dword_1004A4B4/A4B8 + size/2 and the dots on pos + size/2 without the
// +10 y offset.
extern float g_RadarWindowPos[2];
extern float g_RadarWindowSize[2];
