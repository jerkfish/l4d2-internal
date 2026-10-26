// Source-engine interface globals and helpers.
//
// Addresses and interface strings are from InitInterfaces (0x10033900) in
// original/target.dll; see NOTES/ida-recon.md §5.
#pragma once

#include "recovered.h"
#include <string>

// INFERRED: partial CGlobalVars layout. Only interval_per_tick (offset 28) is
// proven by the binary (multiplied by tick counts in Cheat_Update); the other
// names follow the standard Source CGlobalVarsBase order and are pending
// field-level verification.
struct CGlobalVars {
    float realtime;            // 0
    int framecount;            // 4
    float absoluteframetime;   // 8
    float curtime;             // 12
    float frametime;           // 16
    int maxClients;            // 20
    int tickcount;             // 24
    float interval_per_tick;   // 28  (proven)
};

typedef void *(__cdecl *CreateInterfaceFn)(const char *name, int *returnCode);

extern void *g_pPlayerInfoManager;   // 0x1004AA0C PlayerInfoManager002
extern void *g_pClient;              // 0x1004AA10 VClient016
extern void *g_pEngineCvar;          // 0x1004A9EC VEngineCvar007
extern void *g_pEngineClient;        // 0x1004A9F0 VEngineClient013
extern void *g_pClientEntityList;    // 0x1004A9E4 VClientEntityList003
extern void *g_pModelInfoClient;     // 0x1004A9F4 VModelInfoClient004
extern void *g_pModelRender;         // 0x1004AA00 VEngineModel016
extern void *g_pMaterialSystem;      // 0x1004A9FC VMaterialSystem080
extern void *g_pVGuiPanel;           // 0x1004A9F8 VGUI_Panel009
extern void *g_pVGuiSurface;         // 0x1004AA08 VGUI_Surface031
extern void *g_pEngineTrace;         // 0x1004A9E8 EngineTraceClient003
extern void *g_pDebugOverlay;        // 0x1004AA04 VDebugOverlay003
extern CGlobalVars *g_pGlobalVars;   // 0x1004A9E0 PlayerInfoManager002 slot 1

void InitInterfaces();

// Scanner call signature mirrors the std::string-by-value construction seen at
// the InitInterfaces call sites (0x10033A6F / 0x10033AC5).
uintptr_t FindPattern(std::string moduleName, std::string pattern);

// 0x100042D0: printf-style console output (UCRT stdout).
int ConsolePrint(const char *format, ...);
