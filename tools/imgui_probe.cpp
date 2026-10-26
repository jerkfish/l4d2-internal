// Prints layout fingerprints for the ImGui headers in the include path.
// Compiled with the target toolchain (MSVC x86) and used to match the
// offsets observed in original/target.dll. Not part of the final build.
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "imgui.h"
#include "imgui_internal.h"

template <typename T, typename = void>
struct KeyMapOff {
    static void print() { std::printf(" IO.KeyMap NA"); }
};
template <typename T>
struct KeyMapOff<T, std::void_t<decltype(((T*)0)->KeyMap)>> {
    static void print() { std::printf(" IO.KeyMap %zu", (size_t)offsetof(T, KeyMap)); }
};

template <typename T, typename = void>
struct ImeOff {
    static void print() { std::printf(" IO.ImeWindowHandle NA"); }
};
template <typename T>
struct ImeOff<T, std::void_t<decltype(((T*)0)->ImeWindowHandle)>> {
    static void print() {
        std::printf(" IO.ImeWindowHandle %zu", (size_t)offsetof(T, ImeWindowHandle));
    }
};

template <typename T, typename = void>
struct BpnOff {
    static void print() { std::printf(" IO.BackendPlatformName NA"); }
};
template <typename T>
struct BpnOff<T, std::void_t<decltype(((T*)0)->BackendPlatformName)>> {
    static void print() {
        std::printf(" IO.BackendPlatformName %zu", (size_t)offsetof(T, BackendPlatformName));
    }
};

template <typename T, typename = void>
struct BrnOff {
    static void print() { std::printf(" IO.BackendRendererName NA"); }
};
template <typename T>
struct BrnOff<T, std::void_t<decltype(((T*)0)->BackendRendererName)>> {
    static void print() {
        std::printf(" IO.BackendRendererName %zu", (size_t)offsetof(T, BackendRendererName));
    }
};

template <typename T, typename = void>
struct WindowsOff {
    static void print() { std::printf(" CTX.Windows NA"); }
};
template <typename T>
struct WindowsOff<T, std::void_t<decltype(((T*)0)->Windows)>> {
    static void print() { std::printf(" CTX.Windows %zu", (size_t)offsetof(T, Windows)); }
};

template <typename T, typename = void>
struct CurWinOff {
    static void print() { std::printf(" CTX.CurrentWindow NA"); }
};
template <typename T>
struct CurWinOff<T, std::void_t<decltype(((T*)0)->CurrentWindow)>> {
    static void print() {
        std::printf(" CTX.CurrentWindow %zu", (size_t)offsetof(T, CurrentWindow));
    }
};

template <typename T, typename = void>
struct CtxFlagsOff {
    static void print() { std::printf(" CTX.ContextFlags NA"); }
};
template <typename T>
struct CtxFlagsOff<T, std::void_t<decltype(((T*)0)->ContextFlags)>> {
    static void print() {
        std::printf(" CTX.ContextFlags %zu", (size_t)offsetof(T, ContextFlags));
    }
};

int main() {
    std::printf("VER %s SIZEOF_IO %zu SIZEOF_CTX %zu IO_OFF %zu",
                IMGUI_VERSION, sizeof(ImGuiIO), sizeof(ImGuiContext),
                (size_t)offsetof(ImGuiContext, IO));
    std::printf(" IO.ConfigFlags %zu IO.BackendFlags %zu IO.DisplaySize %zu",
                (size_t)offsetof(ImGuiIO, ConfigFlags),
                (size_t)offsetof(ImGuiIO, BackendFlags),
                (size_t)offsetof(ImGuiIO, DisplaySize));
    std::printf(" IO.Fonts %zu IO.MousePos %zu IO.MouseDown %zu",
                (size_t)offsetof(ImGuiIO, Fonts),
                (size_t)offsetof(ImGuiIO, MousePos),
                (size_t)offsetof(ImGuiIO, MouseDown));
    KeyMapOff<ImGuiIO>::print();
    BpnOff<ImGuiIO>::print();
    BrnOff<ImGuiIO>::print();
    ImeOff<ImGuiIO>::print();
    WindowsOff<ImGuiContext>::print();
    CurWinOff<ImGuiContext>::print();
    CtxFlagsOff<ImGuiContext>::print();
    std::printf("\n");
    return 0;
}
