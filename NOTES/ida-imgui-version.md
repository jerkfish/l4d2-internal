# ImGui version identification — result: v1.74 (master)

Date: 2026-10-25. Method: compile `tools/imgui_probe.cpp` against upstream
headers for 38 release tags with MSVC 14.38 x86, then compare `offsetof`
values with the raw offsets used by `Hook_EndScene` / `WndProc_Hook`.

## Evidence 1 — IO layout inside the context

`g_pImGuiContext` is an `ImGuiContext*` (offsets up to +7232; `ImGuiIO` is
only ~5.4 KB, the context is ~15 KB). For every version from 1.63 on,
`offsetof(ImGuiContext, IO) == 8`. Context-relative observed offsets:

| Observed | Field | Required `offsetof(ImGuiIO, ...)` |
|---:|---|---:|
| +0x08 | `IO.ConfigFlags` | 0 |
| +0x0C | `IO.BackendFlags` | 4 |
| +0x34 | `IO.KeyMap` | 44 |
| +0xBC | `IO.BackendPlatformName` | 180 |
| +0xC0 | `IO.BackendRendererName` | 184 |
| +0xE0 | `IO.ImeWindowHandle` | 216 |
| +0xE8/+0xEC | `IO.MousePos.x/y` | 224 |
| +0xF0 | `IO.MouseDown[0]` | 232 |

Probe results for the matching window:

| Version | KeyMap | BPN | BRN | Ime | MousePos | MouseDown | Verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| 1.72b | 44 | 176 | 180 | 212 | 220 | 228 | no |
| **1.73** | 44 | 180 | 184 | 216 | 224 | 232 | yes |
| **1.74** | 44 | 180 | 184 | 216 | 224 | 232 | yes |
| 1.75–1.79 | 44 | 180 | 184 | 216 | 224 | 232 | yes |
| 1.80+ | 44 | 180 | 184 | 216 | 220 | 228 | no |

## Evidence 2 — context fields (decisive)

`tools/imgui_probe_ctx.cpp` against v1.74:

| Observed in binary | v1.74 field offset | Meaning |
|---:|---:|---|
| ctx+5392 read as float | `offsetof(ImGuiContext, Style) == 5392` | `Style.Alpha` |
| ctx+5904 / +5920 as `__m128` | `Style.Colors[i]` (ImVec4) | style color reads |
| ctx+6576 dereferenced, `+124` set to 1 | `offsetof(ImGuiContext, CurrentWindow) == 6576`; `offsetof(ImGuiWindow, WriteAccessed) == 124` | `g.CurrentWindow->WriteAccessed = true` |
| ctx+7160 tested as bool | `offsetof(ImGuiContext, DrawData) == 7160` | `g.DrawData.Valid` |

1.73/1.75+ shift `CurrentWindow`/`DrawData`:
- 1.73: CurrentWindow 6588, DrawData 7168
- 1.75: CurrentWindow 6648, DrawData 7232
- 1.76+: even higher

Only **1.74** has `CurrentWindow == 6576` and `DrawData == 7160` together.

## Evidence 3 — backend source matches line-for-line

`third_party/imgui-1.74/backends/imgui_impl_win32.cpp` (`ImGui_ImplWin32_Init`):

- `QueryPerformanceFrequency` / `QueryPerformanceCounter` at lines 52–54
- `io.BackendPlatformName = "imgui_impl_win32"` line 62
- `io.ImeWindowHandle = hwnd` line 63
- `io.KeyMap[...] = VK_...` lines 66–85 (the exact 22 entries, same order)
- `io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors` / `HasSetMousePos`
  (the observed `|= 6`)

`imgui_impl_dx9.cpp` Init sets `io.BackendRendererName = "imgui_impl_dx9"` and
`io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset` (observed `|= 8`).

The observed `io.ConfigFlags = 32` at init corresponds to
`ImGuiConfigFlags_NoMouseCursorChange`, set by the cheat (not by the stock
backend).

## Conclusion

- ImGui **v1.74**, master branch, stock sources, MSVC x86.
- `imgui_impl_win32.cpp` and `imgui_impl_dx9.cpp` are also v1.74.
- These functions are library code: replace, do not recover.
- `Hook_EndScene` contains the *inlined* `ImGui_ImplWin32_Init` /
  `ImGui_ImplDX9_Init` bodies; when rebuilding, calling the vendored backend
  functions will produce equivalent (though possibly not byte-identical) code.
- `tools/imgui_probe.cpp`, `tools/imgui_probe_ctx.cpp`, and the candidate
  headers under `NOTES/ida/imgui/` are the reproducible evidence.

## Next validation

Compile v1.74 + backends with MSVC 14.38 x86 `/O2 /MD /GS /guard:cf` and
compare the 234-function ImGui range (size/order/string content) against the
binary before excluding it from recovery.
