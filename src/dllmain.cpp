// VR.dll bootstrap.
//
// Recovered from original/target.dll:
//   DllMain (0x1000BA00) -> CreateThread(CheatThread_Main)
//   CheatThread_Main (0x1000B960): kiero::init(D3D9) -> MinHook EndScene ->
//   EnumWindows -> WndProc subclass -> wait for VK_END.
//
// Kiero is included into this translation unit because the original DllMain
// directly touches kiero.cpp's file-static g_renderType / g_methodsTable
// (their addresses are 0x1004B180 / 0x1004B16C in the target).
#include "recovered.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "kiero.h"

#include <cstring>
#include <intrin.h>
// kiero.cpp pulls in its bundled MinHook.h when KIERO_USE_MINHOOK is set;
// including the second copy here would redefine MH_STATUS (both copies use
// #pragma once, which is path-based).

#include "../third_party/kiero-1.2.9/kiero.cpp"

// --- Globals (addresses from the original) ---------------------------------

IDirect3DDevice9 *g_pD3D9Device = nullptr;              // 0x1004B160
HRESULT(__stdcall *g_pfnEndScene)(IDirect3DDevice9 *) = nullptr;  // 0x1004A98C
HWND g_hGameWindow = nullptr;                           // 0x1004B144
WNDPROC g_pfnPrevWndProc = nullptr;                     // 0x1004A9B0
uint32_t g_MenuState = 0;                               // 0x1004B054

// --- DllMain ---------------------------------------------------------------

BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, 0, CheatThread_Main, hinstDLL, 0, nullptr);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        // Target disassembly: if (g_renderType) { MH_EnableHook(0,false);
        // free(g_methodsTable); g_methodsTable = 0; g_renderType = 0; }
        if (g_renderType != kiero::RenderType::None) {
            MH_DisableHook(MH_ALL_HOOKS);
            free(g_methodsTable);
            g_methodsTable = nullptr;
            g_renderType = kiero::RenderType::None;
        }
    }
    return TRUE;
}

// --- Window discovery ------------------------------------------------------

// 0x1000B920: first top-level window owned by this process.
BOOL __stdcall EnumWindows_FindGameWindow(HWND hWnd, LPARAM lParam) {
    (void)lParam;
    DWORD processId = 0;
    GetWindowThreadProcessId(hWnd, &processId);
    if (processId != GetCurrentProcessId())
        return TRUE;
    g_hGameWindow = hWnd;
    return FALSE;
}

// --- Worker thread ---------------------------------------------------------

// 0x1000B960. The target retries kiero::init until VK_END if setup failed;
// once the window is subclassed it exits on the next VK_END press.
DWORD __stdcall CheatThread_Main(LPVOID lpThreadParameter) {
    (void)lpThreadParameter;
    bool hooked = false;

    for (;;) {
        int status = (int)kiero::init(kiero::RenderType::D3D9);
        if (status == kiero::Status::Success) {
            if (g_renderType != kiero::RenderType::None) {
                void *target = reinterpret_cast<void *>(g_methodsTable[42]);
                MH_STATUS create = MH_CreateHook(
                    target, reinterpret_cast<LPVOID>(&Hook_EndScene),
                    reinterpret_cast<LPVOID *>(&g_pfnEndScene));
                if (create == MH_OK)
                    MH_EnableHook(target);
            }

            do {
                g_hGameWindow = nullptr;
                EnumWindows(EnumWindows_FindGameWindow, 0);
            } while (!g_hGameWindow);

            g_pfnPrevWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongA(g_hGameWindow, GWL_WNDPROC,
                               reinterpret_cast<LONG_PTR>(&WndProc_Hook)));
            hooked = true;
        }

        while (!GetAsyncKeyState(VK_END)) {
        }
        if (hooked)
            break;
    }
    return 1;
}
