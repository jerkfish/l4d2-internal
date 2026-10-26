// Engine interface resolution. Recovered from InitInterfaces (0x10033900).
#include "interfaces.h"

void *g_pPlayerInfoManager = nullptr;
void *g_pClient = nullptr;
void *g_pEngineCvar = nullptr;
void *g_pEngineClient = nullptr;
void *g_pClientEntityList = nullptr;
void *g_pModelInfoClient = nullptr;
void *g_pModelRender = nullptr;
void *g_pMaterialSystem = nullptr;
void *g_pVGuiPanel = nullptr;
void *g_pVGuiSurface = nullptr;
void *g_pEngineTrace = nullptr;
void *g_pDebugOverlay = nullptr;
CGlobalVars *g_pGlobalVars = nullptr;

void InitInterfaces() {
    HMODULE engine = GetModuleHandleW(L"engine.dll");
    CreateInterfaceFn CreateInterfaceEngine =
        (CreateInterfaceFn)GetProcAddress(engine, "CreateInterface");

    HMODULE client = GetModuleHandleW(L"client.dll");
    CreateInterfaceFn CreateInterfaceClient =
        (CreateInterfaceFn)GetProcAddress(client, "CreateInterface");

    HMODULE vstdlib = GetModuleHandleW(L"vstdlib.dll");
    CreateInterfaceFn CreateInterfaceVstdlib =
        (CreateInterfaceFn)GetProcAddress(vstdlib, "CreateInterface");

    HMODULE vguimatsurface = GetModuleHandleW(L"vguimatsurface.dll");
    CreateInterfaceFn CreateInterfaceSurface =
        (CreateInterfaceFn)GetProcAddress(vguimatsurface, "CreateInterface");

    HMODULE vgui2 = GetModuleHandleW(L"vgui2.dll");
    CreateInterfaceFn CreateInterfacePanel =
        (CreateInterfaceFn)GetProcAddress(vgui2, "CreateInterface");

    HMODULE materialsystem = GetModuleHandleW(L"materialsystem.dll");
    CreateInterfaceFn CreateInterfaceMaterial =
        (CreateInterfaceFn)GetProcAddress(materialsystem, "CreateInterface");

    HMODULE server = GetModuleHandleW(L"server.dll");
    CreateInterfaceFn CreateInterfaceServer =
        (CreateInterfaceFn)GetProcAddress(server, "CreateInterface");

    g_pPlayerInfoManager = CreateInterfaceServer("PlayerInfoManager002", nullptr);
    g_pClient = CreateInterfaceClient("VClient016", nullptr);
    g_pEngineCvar = CreateInterfaceVstdlib("VEngineCvar007", nullptr);
    g_pEngineClient = CreateInterfaceEngine("VEngineClient013", nullptr);
    g_pClientEntityList = CreateInterfaceClient("VClientEntityList003", nullptr);
    g_pModelInfoClient = CreateInterfaceEngine("VModelInfoClient004", nullptr);
    g_pModelRender = CreateInterfaceEngine("VEngineModel016", nullptr);
    g_pMaterialSystem = CreateInterfaceMaterial("VMaterialSystem080", nullptr);
    g_pVGuiPanel = CreateInterfacePanel("VGUI_Panel009", nullptr);
    g_pVGuiSurface = CreateInterfaceSurface("VGUI_Surface031", nullptr);
    g_pEngineTrace = CreateInterfaceEngine("EngineTraceClient003", nullptr);
    g_pDebugOverlay = CreateInterfaceEngine("VDebugOverlay003", nullptr);

    // Both scan results are discarded in the target (version/validity checks).
    FindPattern("client.dll", "89 04 B5 ? ? ? ? E8");
    FindPattern("client.dll",
                "55 8B EC 83 EC 1C 53 56 8B F1 8B 0D ? ? ? ? 8B 01 8B 50 38 "
                "57 FF D2 84 C0 75 0D");

    // INFERRED: PlayerInfoManager002 vtable slot 1 = GetGlobalVars; the only
    // uses of the result read offset 28 (interval_per_tick).
    typedef CGlobalVars *(__thiscall * GetGlobalVarsFn)(void *);
    void **vtable = *reinterpret_cast<void ***>(g_pPlayerInfoManager);
    g_pGlobalVars =
        reinterpret_cast<GetGlobalVarsFn>(vtable[1])(g_pPlayerInfoManager);

    if (!g_pEngineCvar)
        ConsolePrint("pConsole failed (NULL)");
}
