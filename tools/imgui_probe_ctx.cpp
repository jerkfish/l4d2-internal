// Prints ImGuiContext / ImGuiWindow field offsets for version fingerprinting.
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "imgui.h"
#include "imgui_internal.h"

#define FIELD_PRINTER(T, F)                                                       \
    template <typename X, typename = void>                                        \
    struct Off_##F {                                                              \
        static void p(const char* n) { std::printf(" %s NA", n); }                 \
    };                                                                            \
    template <typename X>                                                         \
    struct Off_##F<X, std::void_t<decltype(((X*)0)->F)>> {                        \
        static void p(const char* n) {                                            \
            std::printf(" %s %zu", n, (size_t)offsetof(X, F));                     \
        }                                                                         \
    };

FIELD_PRINTER(ImGuiContext, Initialized)
FIELD_PRINTER(ImGuiContext, IO)
FIELD_PRINTER(ImGuiContext, Style)
FIELD_PRINTER(ImGuiContext, Font)
FIELD_PRINTER(ImGuiContext, FontSize)
FIELD_PRINTER(ImGuiContext, Time)
FIELD_PRINTER(ImGuiContext, FrameCount)
FIELD_PRINTER(ImGuiContext, FrameCountEnded)
FIELD_PRINTER(ImGuiContext, FrameCountRendered)
FIELD_PRINTER(ImGuiContext, WithinFrameScope)
FIELD_PRINTER(ImGuiContext, Windows)
FIELD_PRINTER(ImGuiContext, CurrentWindowStack)
FIELD_PRINTER(ImGuiContext, CurrentWindow)
FIELD_PRINTER(ImGuiContext, NavWindow)
FIELD_PRINTER(ImGuiContext, DrawData)
FIELD_PRINTER(ImGuiContext, BackgroundDrawList)
FIELD_PRINTER(ImGuiContext, ForegroundDrawList)
FIELD_PRINTER(ImGuiContext, Viewports)
FIELD_PRINTER(ImGuiContext, PlatformImePos)
FIELD_PRINTER(ImGuiContext, WindowsFocusOrder)
FIELD_PRINTER(ImGuiWindow, Name)
FIELD_PRINTER(ImGuiWindow, ID)
FIELD_PRINTER(ImGuiWindow, Flags)
FIELD_PRINTER(ImGuiWindow, Pos)
FIELD_PRINTER(ImGuiWindow, Size)
FIELD_PRINTER(ImGuiWindow, Active)
FIELD_PRINTER(ImGuiWindow, WasActive)
FIELD_PRINTER(ImGuiWindow, WriteAccessed)
FIELD_PRINTER(ImGuiWindow, Hidden)
FIELD_PRINTER(ImGuiWindow, Collapsed)

int main() {
    std::printf("VER %s\n", IMGUI_VERSION);
    std::printf("CTX");
    Off_IO<ImGuiContext>::p("IO");
    Off_Style<ImGuiContext>::p("Style");
    Off_Font<ImGuiContext>::p("Font");
    Off_FontSize<ImGuiContext>::p("FontSize");
    Off_Time<ImGuiContext>::p("Time");
    Off_FrameCount<ImGuiContext>::p("FrameCount");
    Off_FrameCountEnded<ImGuiContext>::p("FrameCountEnded");
    Off_FrameCountRendered<ImGuiContext>::p("FrameCountRendered");
    Off_WithinFrameScope<ImGuiContext>::p("WithinFrameScope");
    Off_Windows<ImGuiContext>::p("Windows");
    Off_CurrentWindowStack<ImGuiContext>::p("CurrentWindowStack");
    Off_CurrentWindow<ImGuiContext>::p("CurrentWindow");
    Off_NavWindow<ImGuiContext>::p("NavWindow");
    Off_DrawData<ImGuiContext>::p("DrawData");
    Off_BackgroundDrawList<ImGuiContext>::p("BackgroundDrawList");
    Off_ForegroundDrawList<ImGuiContext>::p("ForegroundDrawList");
    Off_Viewports<ImGuiContext>::p("Viewports");
    Off_PlatformImePos<ImGuiContext>::p("PlatformImePos");
    std::printf("\nWIN");
    Off_Name<ImGuiWindow>::p("Name");
    Off_ID<ImGuiWindow>::p("ID");
    Off_Flags<ImGuiWindow>::p("Flags");
    Off_Pos<ImGuiWindow>::p("Pos");
    Off_Size<ImGuiWindow>::p("Size");
    Off_Active<ImGuiWindow>::p("Active");
    Off_WasActive<ImGuiWindow>::p("WasActive");
    Off_WriteAccessed<ImGuiWindow>::p("WriteAccessed");
    Off_Hidden<ImGuiWindow>::p("Hidden");
    Off_Collapsed<ImGuiWindow>::p("Collapsed");
    std::printf("\n");
    return 0;
}
