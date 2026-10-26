// Engine hooks. Bodies are being recovered incrementally; every stub below is
// a deliberate placeholder and is listed in STATE.md as PENDING.
#include "recovered.h"
#include "cusercmd.h"
#include "esp.h"
#include "interfaces.h"
#include "netprop.h"
#include "overlay.h"
#include "radar.h"
#include "vmt.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <cmath>
#include <cstring>

void __fastcall Cheat_Update(CUserCmd *cmd, void *edx);

// ImGui 1.74 intentionally leaves the WndProc handler declaration commented
// out in imgui_impl_win32.h to avoid dragging in <windows.h>; declare it here.
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                       LPARAM lParam);

// --- Detour prototypes (signatures from the target's call sites) -----------

// IPanel::PaintTraverse slot 41. The target detour forwards to the original
// before touching ECX, so the interface pointer survives in ECX (thiscall).
typedef void(__thiscall *PaintTraverseFn)(void *thisptr, unsigned int panel,
                                          bool forceRepaint, bool allowForce);

// IVModelRender::DrawModelExecute slot 19. The target detour's epilogue is
// `retn 0Ch`: this engine build passes this + three stack arguments
// (render context, DrawModelState, ModelRenderInfo). The target ignores the
// incoming this and passes the global g_pModelRender explicitly.
typedef void(__thiscall *DrawModelExecuteFn)(void *thisptr, void *ctx,
                                             void *state, void *info);

typedef char(__thiscall *EngineSlot20Fn)(void *thisptr, int arg);

PaintTraverseFn g_pfnPaintTraverse_Orig = nullptr;
DrawModelExecuteFn g_pfnDrawModelExecute_Orig = nullptr;
EngineSlot20Fn g_pfnEngineClientSlot20_Orig = nullptr;

VmtHook *g_pVmtHook_Panel = nullptr;
VmtHook *g_pVmtHook_ModelRender = nullptr;
VmtHook *g_pVmtHook_EngineClient = nullptr;

// 0x100076A0: IVEngineClient slot-20 detour. The target reads the caller's
// ESI as the CUserCmd and compares its command number (cmd+4) with the
// caller's first stack argument (caller EBP+8, i.e. CInput::CreateMove's
// sequence_number). Only that call site is the command the engine is about
// to serialize; the same slot is also invoked with other ESI values
// (SetViewAngles from other callers), which the target skips.
//
// Preserve that condition exactly: a plain "command number changed" test can
// fire on a stack backup cmd first and then skip the real one, so Teleport /
// Air Stuck never reached the command the engine used.

// 0x1004A96C: SpeedHack rate counter (target .data BSS, starts at 0).
static int g_SpeedHackCounter = 0;

// SAFETY (INFERRED): the target walks these pointers unguarded; only the null
// and readability checks are ours, so valid engine pointers behave the same.
static bool ReadPointer(const void *address, void **value) {
    if (!address || IsBadReadPtr(address, sizeof(void *)))
        return false;
    *value = *reinterpret_cast<void *const *>(address);
    return true;
}

static void __cdecl EngineSlot20_Process(void *thisptr, int arg, void *cmdRaw,
                                         void *callerEBP) {
    if (g_pfnEngineClientSlot20_Orig)
        g_pfnEngineClientSlot20_Orig(thisptr, arg);

    CUserCmd *cmd = nullptr;
    if (cmdRaw && !IsBadReadPtr(cmdRaw, sizeof(CUserCmd)))
        cmd = static_cast<CUserCmd *>(cmdRaw);

    int callerArg = 0;
    bool callerArgValid = false;
    if (callerEBP && !IsBadReadPtr(static_cast<char *>(callerEBP) + 8, 4)) {
        callerArg = *reinterpret_cast<int *>(static_cast<char *>(callerEBP) + 8);
        callerArgValid = true;
    }

    if (cmd && callerArgValid && cmd->command_number == callerArg) {
        // 0x100076DD: SpeedHack. While enabled with value > 0 and the key is
        // down (the target tests the whole AX result, i.e. held or pressed
        // since the previous call, not just the 0x8000 bit), walk the caller's
        // stack chain and subtract 5 from the dword at the final pointer + 4.
        // dword_1004A96C counts down; on expiry (or key up) it reloads from
        // the slider value, so the subtraction is applied `value` times in a
        // row and skipped once.
        if (g_SpeedHack && g_SpeedHackValue > 0) {
            if (GetAsyncKeyState(g_SpeedHackKey) != 0) {
                int oldCounter = g_SpeedHackCounter--;
                if (oldCounter > 0) {
                    void *p1 = nullptr;
                    void *p2 = nullptr;
                    void *p3 = nullptr;
                    if (ReadPointer(callerEBP, &p1) &&
                        ReadPointer(p1, &p2) && ReadPointer(p2, &p3) && p3 &&
                        !IsBadReadPtr(static_cast<char *>(p3) + 4, 4)) {
                        *reinterpret_cast<int *>(static_cast<char *>(p3) + 4) -=
                            5;
                    } else {
                        g_SpeedHackCounter = g_SpeedHackValue;
                    }
                } else {
                    g_SpeedHackCounter = g_SpeedHackValue;
                }
            } else {
                g_SpeedHackCounter = g_SpeedHackValue;
            }
        }

        Cheat_Update(cmd, nullptr);
        // Target 0x10007735: write the command number back into the caller's
        // stack slot after the update.
        *reinterpret_cast<int *>(static_cast<char *>(callerEBP) + 8) =
            cmd->command_number;

        // Target 0x10007738: AND the byte at (second pointer after the caller
        // EBP) - 0x1D with HIBYTE(g_MenuState). Outside Fake Lag's choke
        // window byte 3 is always 1, so this is a no-op; it becomes the packet
        // choke mask once Fake Lag is ported.
        void *p1 = nullptr;
        void *p2 = nullptr;
        if (ReadPointer(callerEBP, &p1) && ReadPointer(p1, &p2) && p2 &&
            !IsBadReadPtr(static_cast<char *>(p2) - 0x1D, 1)) {
            *reinterpret_cast<unsigned char *>(static_cast<char *>(p2) - 0x1D) &=
                static_cast<unsigned char>(g_MenuState >> 24);
        }
    }
}

// The hooked function is IVEngineClient::SetViewAngles(const QAngle&). The
// engine's CInput::CreateMove keeps the CUserCmd in ESI and passes
// &cmd->viewangles in the stack argument. Capture ESI plus the caller's EBP
// (the target reads [callerEBP+8]) with a naked shim.
__declspec(naked) char __stdcall Hook_EngineClient_Slot20(int arg) {
    (void)arg;
    __asm {
        push ebp
        mov ebp, esp                ; [ebp] = caller's EBP, [ebp+8] = arg
        push esi
        push edi
        mov eax, esi                ; CUserCmd captured from the caller
        push dword ptr [ebp]        ; arg4: caller EBP
        push eax                    ; arg3: CUserCmd
        push dword ptr [ebp+8]      ; arg2: original stack argument
        push dword ptr [g_pEngineClient]
        call EngineSlot20_Process   ; __cdecl
        add esp, 16
        pop edi
        pop esi
        pop ebp
        ret 4
    }
}

// 0x10004FC0: PaintTraverse detour. The target forwards to the original
// first, caches the FocusOverlayPanel id, and draws its VGUI overlays only
// for that panel. Ported here: Indicators (sub_1003AD40).
void __fastcall Hook_PaintTraverse(void *thisptr, void * /*edx*/,
                                   unsigned int panel, bool forceRepaint,
                                   bool allowForce) {
    if (g_pfnPaintTraverse_Orig)
        g_pfnPaintTraverse_Orig(thisptr, panel, forceRepaint, allowForce);

    if (!g_pVGuiPanel || IsBadReadPtr(g_pVGuiPanel, sizeof(void *)))
        return;

    static unsigned int focusPanel = 0;
    static bool focusFound = false;
    if (!focusFound) {
        void **vtable = *reinterpret_cast<void ***>(g_pVGuiPanel);
        if (vtable && !IsBadReadPtr(vtable, sizeof(void *) * 37)) {
            // IPanel vtable slot 36 (144/4): GetName(panel).
            typedef const char *(__thiscall * GetNameFn)(void *,
                                                         unsigned int);
            const char *name = reinterpret_cast<GetNameFn>(vtable[36])(
                g_pVGuiPanel, panel);
            if (name && !IsBadReadPtr(name, 1) &&
                std::strstr(name, "FocusOverlayPanel") != nullptr) {
                focusPanel = panel;
                focusFound = true;
            }
        }
    }

    if (!focusFound || panel != focusPanel)
        return;

    CreateOverlayFonts();

    void *local = GetLocalPlayerEntity();
    if (!local)
        return;

    // Target PaintTraverse 0x10005136: call the ESP painter when any of
    // Player Box / Player Name / Player Distance ESP is enabled; the painter
    // itself then requires Enable ESP before drawing anything.
    if (g_EspBox || g_EspName || g_EspDistance)
        DrawEsp(local);

    // Target PaintTraverse 0x10005158: radar frame + entity dots.
    if (g_Radar)
        DrawRadar(local);

    // Target PaintTraverse 0x1000516F: world weapon-spawn labels.
    if (g_EspWeapons2)
        DrawWeaponSpawnEsp(local);

    // PENDING: speed text (dword_1004AFF4) and the sub_1003AD40 indicators
    // panel (gated by dword_1004B048, which nothing ever sets).
}

// 0x10004A5D / 0x10004A87: IMaterialSystem080 vtable slot 71
// (284/4). The target calls (name, texture group, 1, 0).
static void *CreateRecoveredMaterial(const char *name, const char *group) {
    if (!g_pMaterialSystem || IsBadReadPtr(g_pMaterialSystem, sizeof(void *)))
        return nullptr;
    void **vtable = *reinterpret_cast<void ***>(g_pMaterialSystem);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 72))
        return nullptr;

    typedef void *(__thiscall * CreateMaterialFn)(void *, const char *,
                                                  const char *, int, int);
    return reinterpret_cast<CreateMaterialFn>(vtable[71])(g_pMaterialSystem,
                                                          name, group, 1, 0);
}

static void *g_pVomitMaterial = nullptr;  // 0x1004B2A0
static void *g_pDebugMaterial = nullptr;  // 0x1004B2E8 (debugambientcube)
static void *g_pChamMaterial = nullptr;   // 0x1004B2BC (debugambientcube)

// IMaterial vtable slot 29 (116/4): SetMaterialVarFlag(flag, value).
static void SetChamMaterialFlag(void *material, int flag, int value) {
    void **vtable = *reinterpret_cast<void ***>(material);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 30))
        return;
    typedef void(__thiscall * SetMaterialVarFlagFn)(void *, int, int);
    reinterpret_cast<SetMaterialVarFlagFn>(vtable[29])(material, flag, value);
}

// IVModelRender slot 1: ForcedMaterialOverride(material, type).
static void ForceMaterialOverride(void *material) {
    if (!g_pModelRender || IsBadReadPtr(g_pModelRender, sizeof(void *)))
        return;
    void **vtable = *reinterpret_cast<void ***>(g_pModelRender);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 2))
        return;
    typedef void(__thiscall * ForcedMaterialOverrideFn)(void *, void *, int);
    reinterpret_cast<ForcedMaterialOverrideFn>(vtable[1])(g_pModelRender,
                                                          material, 0);
}

// sub_10004800 / sub_100048C0: sets the cham material flags and color, then
// forces it. 0x8000 is the ignore-z flag (XQZ); the others are constant.
static void ConfigureChamMaterial(void *material, bool ignoreZ, int r, int g,
                                  int b) {
    if (!material || IsBadReadPtr(material, sizeof(void *)))
        return;

    SetChamMaterialFlag(material, 0x8000, ignoreZ ? 1 : 0);
    SetChamMaterialFlag(material, 1024, 1);
    SetChamMaterialFlag(material, 0x2000, 1);
    SetChamMaterialFlag(material, 0x4000, 1);
    SetChamMaterialFlag(material, 0x8000000, 1);

    void **vtable = *reinterpret_cast<void ***>(material);
    if (vtable && !IsBadReadPtr(vtable, sizeof(void *) * 29)) {
        // Slot 28 (112/4): ColorModulate(r, g, b).
        typedef void(__thiscall * ColorModulateFn)(void *, float, float,
                                                   float);
        reinterpret_cast<ColorModulateFn>(vtable[28])(
            material, static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f);
    }

    ForceMaterialOverride(material);
}

// One cham pass: the XQZ pass draws the ignore-z copy first, then the normal
// copy is configured for the caller's final draw.
static void ApplyChamPass(void *material, bool xqz, int r, int g, int b,
                          void *ctx, void *state, void *info) {
    if (xqz) {
        ConfigureChamMaterial(material, true, r, g, b);
        if (g_pfnDrawModelExecute_Orig)
            g_pfnDrawModelExecute_Orig(g_pModelRender, ctx, state, info);
    }
    ConfigureChamMaterial(material, false, r, g, b);
}

// IVModelInfoClient slot 3 (12/4): GetModelName(model_t*).
static const char *GetRenderModelName(void *model) {
    if (!model || !g_pModelInfoClient ||
        IsBadReadPtr(g_pModelInfoClient, sizeof(void *)))
        return nullptr;
    void **vtable = *reinterpret_cast<void ***>(g_pModelInfoClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 4))
        return nullptr;
    typedef const char *(__thiscall * GetModelNameFn)(void *, void *);
    const char *name =
        reinterpret_cast<GetModelNameFn>(vtable[3])(g_pModelInfoClient,
                                                    model);
    if (!name || IsBadReadPtr(name, 1))
        return nullptr;
    return name;
}

// 0x10004980: DrawModelExecute detour. Ported: screenspace Boomer-vomit
// material override (Remove Vomit) and Hand Chams. Player/weapon cham
// branches remain PENDING; the target also draws once at the end, which this
// port does unconditionally.
void __stdcall Hook_DrawModelExecute(void *ctx, void *state, void *info) {
    if (!g_pVomitMaterial) {
        g_pVomitMaterial = CreateRecoveredMaterial(
            "particle/screenspaceboomervomit", "Particle textures");
    }
    if (!g_pDebugMaterial) {
        g_pDebugMaterial = CreateRecoveredMaterial("debug/debugambientcube",
                                                   "Model textures");
    }
    if (!g_pChamMaterial) {
        g_pChamMaterial = CreateRecoveredMaterial("debug/debugambientcube",
                                                  "Model textures");
    }

    // 0x10004B1B: only when the state has a renderable (+28) and entity
    // index (+48) and both recovered materials exist.
    if (state && !IsBadReadPtr(state, 52) && g_pDebugMaterial &&
        g_pVomitMaterial) {
        void *renderable =
            *reinterpret_cast<void **>(static_cast<char *>(state) + 28);
        int entityIndex =
            *reinterpret_cast<int *>(static_cast<char *>(state) + 48);
        if (renderable && entityIndex) {
            void **matVtable = *reinterpret_cast<void ***>(g_pVomitMaterial);
            if (matVtable &&
                !IsBadReadPtr(matVtable, sizeof(void *) * 30)) {
                // 0x10004B59: vtable+116 (slot 29), flag 4, value =
                // (uint8_t)byte_1004B008 (Remove Vomit).
                typedef void(__thiscall * SetMaterialVarFlagFn)(void *, int,
                                                                int);
                reinterpret_cast<SetMaterialVarFlagFn>(matVtable[29])(
                    g_pVomitMaterial, 4, g_RemoveVomit ? 1 : 0);
            }
        }
    }

    // 0x10004B7C: Hand Chams (byte_1004ABE2), viewmodel arms only. XQZ
    // (byte_1004ABE1) draws the through-wall pass first, then the normal
    // page; the unconditional draw below supplies the normal pass.
    if (g_HandChams && state && !IsBadReadPtr(state, 52) &&
        g_pChamMaterial) {
        void *model =
            *reinterpret_cast<void **>(static_cast<char *>(state) + 28);
        const char *modelName = GetRenderModelName(model);
        if (modelName &&
            std::strstr(modelName, "models/weapons/arms/v_arms") != nullptr) {
            if (g_XQZChams) {
                ConfigureChamMaterial(g_pChamMaterial, true, g_HandChamR,
                                      g_HandChamG, g_HandChamB);
                if (g_pfnDrawModelExecute_Orig)
                    g_pfnDrawModelExecute_Orig(g_pModelRender, ctx, state,
                                               info);
            }
            ConfigureChamMaterial(g_pChamMaterial, false, g_HandChamR,
                                  g_HandChamG, g_HandChamB);
        }
    }

    // 0x10004C14 / 0x10004CAC / 0x10004D44: weapon chams, config-only
    // (Handwpnchams -> models/v_models, wpnchams ->
    // models/w_models/weapons, ChamsINFECTED -> models/infected). All use the
    // 0x1004B2BC material and the hand-cham colors.
    if (state && !IsBadReadPtr(state, 52) && g_pChamMaterial &&
        (g_HandWeaponChams || g_WeaponChams || g_InfectedChams)) {
        void *model =
            *reinterpret_cast<void **>(static_cast<char *>(state) + 28);
        const char *modelName = GetRenderModelName(model);
        if (modelName) {
            if (g_HandWeaponChams &&
                std::strstr(modelName, "models/v_models") != nullptr) {
                ApplyChamPass(g_pChamMaterial, g_XQZChams, g_HandChamR,
                              g_HandChamG, g_HandChamB, ctx, state, info);
            }
            if (g_WeaponChams &&
                std::strstr(modelName, "models/w_models/weapons") !=
                    nullptr) {
                ApplyChamPass(g_pChamMaterial, g_XQZChams, g_HandChamR,
                              g_HandChamG, g_HandChamB, ctx, state, info);
            }
            if (g_InfectedChams &&
                std::strstr(modelName, "models/infected") != nullptr) {
                ApplyChamPass(g_pChamMaterial, g_XQZChams, g_HandChamR,
                              g_HandChamG, g_HandChamB, ctx, state, info);
            }
        }
    }

    // 0x10004DD3: Player Chams (byte_1004ABE0). Matches category-4
    // entities, infected models, or survivor-team entities in a
    // chams-eligible animation state (sub_10001EC0).
    if (g_PlayerChams && state && !IsBadReadPtr(state, 52) &&
        g_pDebugMaterial && g_pVomitMaterial) {
        void *model =
            *reinterpret_cast<void **>(static_cast<char *>(state) + 28);
        int entityIndex =
            *reinterpret_cast<int *>(static_cast<char *>(state) + 48);
        int highest = GetHighestEntityIndex();

        // SAFETY: the target enters only when DrawModelState +48 is nonzero;
        // bound the index before calling into the engine's entity list.
        void *entity = nullptr;
        if (model && entityIndex > 0 && entityIndex <= highest)
            entity = GetClientEntityByIndex(entityIndex);

        if (entity) {
            int category = GetEntityCategory(entity);
            int team = GetEntityTeam(entity);

            const char *modelName = GetRenderModelName(model);

            bool match = (category == 4);
            if (!match && modelName &&
                std::strstr(modelName, "models/infected") != nullptr)
                match = true;
            bool eligible = false;
            if (!match && team == 2 && category != 0) {
                eligible = IsPlayerChamsEligible(entity);
                if (eligible)
                    match = true;
            }

            if (match) {
                if (g_XQZChams) {
                    ConfigureChamMaterial(g_pDebugMaterial, true,
                                          g_PlayerChamR, g_PlayerChamG,
                                          g_PlayerChamB);
                    if (g_pfnDrawModelExecute_Orig)
                        g_pfnDrawModelExecute_Orig(g_pModelRender, ctx, state,
                                                   info);
                }
                ConfigureChamMaterial(g_pDebugMaterial, false, g_PlayerChamR,
                                      g_PlayerChamG, g_PlayerChamB);
            }
        }
    }

    if (g_pfnDrawModelExecute_Orig)
        g_pfnDrawModelExecute_Orig(g_pModelRender, ctx, state, info);

    // 0x10004FA5: ForcedMaterialOverride(NULL, 0).
    ForceMaterialOverride(nullptr);
}

// Installs the two recovered VMT hooks. Called from the EndScene lazy-init
// block once that block is ported; not called yet.
void InstallVmtHooks() {
    if (g_pVGuiPanel) {
        g_pVmtHook_Panel = new VmtHook(g_pVGuiPanel);
        g_pfnPaintTraverse_Orig =
            reinterpret_cast<PaintTraverseFn>(g_pVmtHook_Panel->origVtable[41]);
        *g_pVmtHook_Panel->entry(41) =
            reinterpret_cast<void *>(&Hook_PaintTraverse);
    }

    if (g_pModelRender) {
        g_pVmtHook_ModelRender = new VmtHook(g_pModelRender);
        g_pfnDrawModelExecute_Orig = reinterpret_cast<DrawModelExecuteFn>(
            g_pVmtHook_ModelRender->origVtable[19]);
        *g_pVmtHook_ModelRender->entry(19) =
            reinterpret_cast<void *>(&Hook_DrawModelExecute);
    }

    if (g_pEngineClient) {
        g_pVmtHook_EngineClient = new VmtHook(g_pEngineClient);
        g_pfnEngineClientSlot20_Orig = reinterpret_cast<EngineSlot20Fn>(
            g_pVmtHook_EngineClient->origVtable[20]);
        *g_pVmtHook_EngineClient->entry(20) =
            reinterpret_cast<void *>(&Hook_EngineClient_Slot20);
    }
}

// 0x1000B830: GWL_WNDPROC subclass.
// Recovered structure:
//   1. forward to ImGui's Win32 handler; if it consumes the message -> 1
//   2. if the menu is closed or the game is paused -> call original wndproc
//   3. otherwise feed the mouse position/left button into ImGui and consume
// TODO(PENDING): engine IsPaused-style vtable check (slot 26) and the
// g_MenuState-gated mouse writes are recovered in Phase 3.
LRESULT __stdcall WndProc_Hook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;

    if (!(g_MenuState & 0xFF))
        return CallWindowProcA(g_pfnPrevWndProc, hWnd, msg, wParam, lParam);

    ImGuiIO &io = ImGui::GetIO();
    POINT point;
    if (GetCursorPos(&point)) {
        io.MousePos.x = static_cast<float>(point.x);
        io.MousePos.y = static_cast<float>(point.y);
    }
    io.MouseDown[0] = GetAsyncKeyState(1) != 0;
    return 0;
}

// --- Legit FOV circle (target Hook_EndScene 0x10009845..0x1000990E) --------
//
// Slot 26 of IVEngineClient is the in-game gate used by the target for the
// FOV circle, the radar and the Fake Lag indicator. GetScreenSize is slot 5.
static bool EngineSlot26Bool() {
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 27))
        return false;
    typedef bool(__thiscall * Slot26Fn)(void *);
    return reinterpret_cast<Slot26Fn>(vtable[26])(g_pEngineClient);
}

// Slot 27 of IVEngineClient: the second in-game/connection gate used by the
// target's radar block (Hook_EndScene 0x10009B2D).
static bool EngineSlot27Bool() {
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 28))
        return false;
    typedef bool(__thiscall * Slot27Fn)(void *);
    return reinterpret_cast<Slot27Fn>(vtable[27])(g_pEngineClient);
}

static bool EngineGetScreenSize(int *width, int *height) {
    if (!g_pEngineClient || IsBadReadPtr(g_pEngineClient, sizeof(void *)))
        return false;
    void **vtable = *reinterpret_cast<void ***>(g_pEngineClient);
    if (!vtable || IsBadReadPtr(vtable, sizeof(void *) * 6))
        return false;
    typedef void(__thiscall * GetScreenSizeFn)(void *, int *, int *);
    reinterpret_cast<GetScreenSizeFn>(vtable[5])(g_pEngineClient, width,
                                                 height);
    return true;
}

// Target sub_10028CC0: outlined text on the ImGui background draw list.
// The target draws the same string at (+/-1, +/-1) in black, then the main
// colour at the requested position.
static void DrawOutlinedText(ImDrawList *drawList, float x, float y,
                             ImU32 color, const char *text) {
    if (!drawList || !text)
        return;
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    drawList->AddText(ImVec2(x - 1.0f, y - 1.0f), black, text);
    drawList->AddText(ImVec2(x - 1.0f, y + 1.0f), black, text);
    drawList->AddText(ImVec2(x + 1.0f, y - 1.0f), black, text);
    drawList->AddText(ImVec2(x + 1.0f, y + 1.0f), black, text);
    drawList->AddText(ImVec2(x, y), color, text);
}

static void DrawEndSceneOverlay() {
    ImDrawList *background = ImGui::GetBackgroundDrawList();

    // Target Hook_EndScene 0x1000980F..0x10009840: permanent anti-resale
    // notice at (10,10) in white, then the SMAC warning at (10,25) in
    // yellow when SMAC mode is off. Both use the background draw list.
    // The XOR-decoded string was recovered from 0x10047690/0x10047610/
    // 0x10047620 + immediates with key (i - 71).
    DrawOutlinedText(background, 10.0f, 10.0f,
                     IM_COL32(255, 255, 255, 255),
                     "IF YOU PAID FOR THIS CHEAT YOU GOT SCAMMED, PLEASE "
                     "REFUND.");
    if (!g_SmacMode) {
        DrawOutlinedText(background, 10.0f, 25.0f,
                         IM_COL32(255, 255, 0, 255),
                         "SMAC MODE IS DISABLED!");
    }

    if (!g_LegitFovCircle || g_FovDesired <= 0)
        return;
    if (!EngineSlot26Bool())
        return;

    int width = 0;
    int height = 0;
    if (!EngineGetScreenSize(&width, &height) || width <= 0 || height <= 0)
        return;

    // Target: (float)FOV / (float)fov_desired * (float)(width/2) - 0.5f.
    // The inlined AddCircle constant 0x40C25C20 is 2*pi*29/30, i.e. the
    // target passed num_segments = 30; ImGui then builds 29 points and
    // strokes them closed with thickness 1.0.
    float radius = static_cast<float>(g_LegitAimFov) /
                       static_cast<float>(g_FovDesired) *
                       static_cast<float>(width / 2) -
                   0.5f;
    ImDrawList *drawList = background;
    if (!drawList)
        return;
    drawList->AddCircle(
        ImVec2(static_cast<float>(width / 2),
               static_cast<float>(height / 2)),
        radius, IM_COL32(255, 255, 255, 255), 30, 1.0f);
}

// --- Indicators window (target Hook_EndScene 0x1000990E..0x10009B20) ------
// Fixed 200x50 "Indicators" window (flags NoResize|NoCollapse). While Fake
// Lag is enabled with a positive value and the engine in-game gate passes,
// the target draws "Fake Lag (Choking)" and a 400x6 two-tone bar: a base
// rect covering 70% of (400 - 2*FramePadding.x) in ImGuiCol_Button, and an
// overlay in ImGuiCol_ButtonHovered whose width is that 70% multiplied by
// the previous frame's dword_1004A9BC. The pulse is updated afterwards as
// sin(g.Time * 13.0) * 0.5 + 0.5.
static void DrawIndicators() {
    if (!g_Indicators)
        return;

    ImGui::SetNextWindowSize(ImVec2(200.0f, 50.0f), ImGuiCond_Always);
    ImGui::Begin("Indicators", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const ImU32 baseColor = ImGui::GetColorU32(ImGuiCol_Button);
    const ImU32 pulseColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    static float pulse = 0.0f;  // dword_1004A9BC

    if (g_FakeLag && g_FakeLagValue > 0 && EngineSlot26Bool()) {
        ImGui::Text("Fake Lag (Choking)");
        const float width =
            400.0f - 2.0f * ImGui::GetStyle().FramePadding.x;
        const ImVec2 position = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, 6.0f));
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const float baseWidth = width * 0.7f;
        drawList->AddRectFilled(
            position, ImVec2(position.x + baseWidth, position.y + 6.0f),
            baseColor);
        drawList->AddRectFilled(
            position,
            ImVec2(position.x + baseWidth * pulse, position.y + 6.0f),
            pulseColor);
        pulse = sinf(static_cast<float>(ImGui::GetTime()) * 13.0f) * 0.5f +
                0.5f;
    }

    ImGui::End();
}

// --- Movable radar window (target Hook_EndScene 0x10009B20..0x10009C7C) ---
// With Radar on and both engine gates passing, the target creates a
// 292x306 ImGui window named "Radar" (NoResize|NoCollapse) whose WindowBg is
// temporarily overridden to (0.2,0.2,0.2,0). It copies the window Pos/Size
// into dword_1004A4B4/A4B8/A4BC/A4C0, which the PaintTraverse radar painter
// (sub_1003BFB0) uses to center the frame and dots. Dragging the ImGui window
// therefore moves the radar. The target's separate dword_1004AC28 drag-delta
// block is orphaned (its results are overwritten every frame), so it is not
// ported.
static void DrawRadarWindow() {
    if (!g_Radar)
        return;
    if (!EngineSlot27Bool() || !EngineSlot26Bool())
        return;

    const ImVec4 background(0.2f, 0.2f, 0.2f, 0.0f);  // 0x100474C0
    ImGui::PushStyleColor(ImGuiCol_WindowBg, background);
    ImGui::SetNextWindowSize(ImVec2(292.0f, 306.0f), ImGuiCond_Always);
    ImGui::Begin("Radar", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    g_RadarWindowPos[0] = pos.x;
    g_RadarWindowPos[1] = pos.y;
    g_RadarWindowSize[0] = size.x;
    g_RadarWindowSize[1] = size.y;
    ImGui::End();
    ImGui::PopStyleColor();
}

// 0x10009160: MinHook detour for IDirect3DDevice9::EndScene (slot 42).
// Ported so far: D3D9 state save/restore and the lazy ImGui/interface init.
// TODO(PENDING): popup, VMT hook installation, NewFrame/Menu_Render/Render/
// RenderDrawData, END-key menu toggle. Until the frame path is ported the
// detour produces no visuals.
HRESULT __stdcall Hook_EndScene(IDirect3DDevice9 *pDevice) {
    static bool imguiInitialized = false;

    g_pD3D9Device = pDevice;

    DWORD colorWriteEnable = 0;
    IDirect3DVertexDeclaration9 *vertexDeclaration = nullptr;
    IDirect3DVertexShader9 *vertexShader = nullptr;
    pDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWriteEnable);
    pDevice->GetVertexDeclaration(&vertexDeclaration);
    pDevice->GetVertexShader(&vertexShader);

    if (!imguiInitialized && g_hGameWindow) {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hGameWindow);
        ImGui_ImplDX9_Init(pDevice);
        ImGui_ImplDX9_CreateDeviceObjects();

        InitInterfaces();
        BuildNetpropTables();
        InstallVmtHooks();
        CreateOverlayFonts();

        // 0x10009754: one-shot watermark popup. Both strings are XOR-decoded
        // in the target from blobs at 0x10047510/0x100475F0/0x10047600/
        // 0x10047640/0x10047650 + the literal at 0x10009723, key (i - 71);
        // verified by decoding the static bytes. "wwww" typo is original.
        MessageBoxA(nullptr,
                    "This cheat was released for free on wwww.unknowncheats.com "
                    "by @space785, if you paid for it you got scammed.",
                    "L4D2", MB_OK);

        imguiInitialized = true;
    }

    // Frame path (target: inline in Hook_EndScene; menu invoked through
    // sub_10001050, which allocates the 0x30-byte menu state and calls
    // Menu_Render). Menu body is still PENDING, so nothing draws yet.
    if (imguiInitialized) {
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawEndSceneOverlay();
        DrawIndicators();
        DrawRadarWindow();
        if (g_MenuState & 1)
            Menu_Render();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

        // Target: VK_INSERT (45) toggles g_MenuState byte 0 after rendering.
        if ((GetAsyncKeyState(VK_INSERT) & 1) != 0)
            g_MenuState ^= 1;
    }

    pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, colorWriteEnable);
    pDevice->SetVertexDeclaration(vertexDeclaration);
    pDevice->SetVertexShader(vertexShader);

    if (g_pfnEndScene)
        return g_pfnEndScene(pDevice);
    return D3D_OK;
}
