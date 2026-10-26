cmake_minimum_required(VERSION 3.20)
project(VR LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target fingerprint: MSVC 14.38 x86, release /MD, /O2 /GS /guard:cf, /GL+LTCG.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    # NOTE: /guard:cf is link-only. The target's user code contains no
    # __guard_check_icall calls; only the prebuilt CRT objects are
    # CFG-instrumented (they populate GuardFlags 0x100 and the check fptr).
    add_compile_options(/O2 /GS /GL /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS)
    # No /GUARD:CF: LTCG would instrument indirect calls, which the target does
    # not contain. The CRT objects still reference the guard pointer, which is
    # why the target's load config has GuardFlags 0x100.
    # /MAP: address map for the crash-diagnostic builds (does not change
    # codegen; the map is used to resolve VR.dll fault offsets from logs).
    add_link_options(/LTCG /DYNAMICBASE /NXCOMPAT /DEBUG /MAP)
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui-1.74)
set(MINHOOK_DIR ${CMAKE_SOURCE_DIR}/third_party/minhook)
set(KIERO_DIR ${CMAKE_SOURCE_DIR}/third_party/kiero-1.2.9)

add_library(VR SHARED
    src/dllmain.cpp
    src/hooks.cpp
    src/interfaces.cpp
    src/utils.cpp
    src/vmt.cpp
    src/menu.cpp
    src/netprop.cpp
    src/md5.cpp
    src/nospread_scan.cpp
    src/nospread.cpp
    src/cheat.cpp
    src/overlay.cpp
    src/esp.cpp
    src/radar.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp

    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde32.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)

target_include_directories(VR PRIVATE
    include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/hde
    ${KIERO_DIR}
)

target_link_libraries(VR PRIVATE d3d9)

set_target_properties(VR PROPERTIES
    OUTPUT_NAME VR
    PREFIX ""
)
