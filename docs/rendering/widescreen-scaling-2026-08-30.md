---
type: Design
title: "Widescreen fullscreen and HD upscaling"
description: "The game keeps rendering at 800x600 or 1024x768 and the presented frame is scaled to the display, with input mapped back."
tags: [rendering, input]
---

# Widescreen fullscreen and HD upscaling

**Date:** 2026-08-30
**Branch:** `feat/widescreen-scaling`
**Status:** Implemented, verified in-game by the repo owner (fullscreen scaling,
click accuracy, continuous attacks, alt-tab, and clean exit all confirmed).

---

## What this delivers

The game keeps rendering exactly as before — an 800×600 (or 1024×768 with the
`3`/`4` flag) frame composed on the CPU — and the presentation layer scales it
to modern screens:

- **Fullscreen** (`2` as the last command-line character): borderless window at
  desktop resolution, game scaled to fill the screen height, centered, with
  pillarbox bars on 16:9. On a 1080p monitor that is a 1.8× upscale.
- **Windowed** (`1`): the window comes up at the largest integer multiple of
  the game size that fits the desktop work area (e.g. 1600×1200 on a 1440p
  monitor).
- No gameplay, UI, or server-visible change: the client does not render more
  world than before. True widescreen field-of-view was considered and
  deliberately rejected — the server controls the sight radius, and the UI is
  anchored to the 800×600 frame.

DLSS was investigated for the "make it HD" ask and is not applicable at any
version: DLSS consumes motion vectors, depth, and jittered 3D rendering from a
D3D11+/Vulkan pipeline. This client never renders in the GPU sense — it blits
one finished CPU-composed image. The right tool for sprite art is a sharp
scaler, below. (ReShade-style post-process shader injection over the SDL
swapchain does work, for anyone who wants CRT/xBR filters on top.)

## The scaler (`spritectl_present_surface`)

`Client/SpriteLib/SpriteLibBackendSDL.cpp` computes an aspect-preserving
letterbox destination every present and picks a filter per scale:

| Scale | Method |
|---|---|
| exact integer (1×, 2×, ...) | nearest — pixel-perfect |
| fractional upscale (e.g. 1.8×) | **sharp-bilinear**: nearest to the next integer multiple into a render target, then linear down to the final size — crisp pixels without nearest's shimmer or plain linear's blur |
| downscale | linear |

The two-pass path runs only on accelerated renderers; a software renderer
(`SDL_RENDER_DRIVER=software`) falls back to single-pass linear because both
passes would run on the CPU at output resolution. (That env var was left set
in the debugger from the memory-leak investigation and made the first
fullscreen test crawl — it is not needed and should stay unset.)

## Mouse mapping

With the frame letterboxed, window-client pixels no longer equal game pixels.
`spritectl_window_to_game_coords` (declared in `SpriteLibBackend.h`) maps
window coordinates back to game coordinates, clamped to the frame so clicks in
the bars land on the nearest edge. Every input entry point routes through it —
the complete list, found by exhaustive search (the WndProc handles no mouse
messages at all):

1. `UpdateMouse()` — `Client/GameMain.cpp`, the canonical writer of `g_x/g_y`.
2. Three `GetCursorPos`→`MouseControl(M_MOVING, ...)` paths in
   `Client/CGameUpdate.cpp` (`ProcessInput`, `UpdateDraw`, and the retired
   smooth-cursor branch), via the `WindowToGamePoint` helper.
3. `Client/DXLib/DXLibBackendSDL.cpp` — one transform after both the SDL event
   cases and the unconditional `SDL_GetMouseState` fallback (the fallback
   overwrites the events every update, so the single late transform covers
   everything).

`dxlib_input_set_mouse_pos` (mouse warp) would need the forward transform; its
only caller is commented out, so it was left alone.

## Fullscreen usability fixes

Found the hard way (a wedged fullscreen debug session forced a PC reboot):

- **No more `WS_EX_TOPMOST`.** A topmost popup keeps covering the screen even
  when the process hangs or the debugger breaks — alt-tab appears dead and the
  machine looks frozen. A plain screen-sized popup covers the desktop just as
  well while focused and yields normally otherwise.
- **Alt+F4 always works.** Upstream swallowed `SC_CLOSE` in fullscreen;
  together with topmost that removed every escape hatch.
- **The fullscreen flag now works in Debug builds.** Upstream forced
  `g_bFullScreen = false` under `#ifdef _DEBUG`, which is live in every Debug
  build (see the `_DEBUG` trap in CLAUDE.md).

## Picking back in exact tick space (60 fps regression fix)

Testing on this branch surfaced a regression from the interpolated-rendering
work (`docs/rendering/frame-rate-interpolation-2026-08-30.md`): holding right-click no
longer chained spell attacks — each cast required waiting out the skill delay.

Mechanism: the draw path publishes picking state as a side effect — the camera
(`m_PlusPoint`/`m_FirstZonePixel`) and every creature's clickable
`m_ScreenRect` — and with interpolation those are written from *drawn*
(sub-tick) positions, ~60×/sec. The per-tick input path re-picks the target
under the cursor every tick while the button is held, mixing exact tick
positions (sector pass) with those interpolated leftovers (rect pass, camera).
One missed pick reaches `ProcessInputRButtonDown` with `pObject == NULL`,
which calls `UnSetRepeatAction()` — the held-button repeat silently drops.

Fix: drawing stays interpolated, picking reads exact tick space again:

- `CGameUpdate.cpp` publishes the camera bias of the last draw
  (`g_DrawCamGapX/Y`).
- `MObject::SetScreenRect`/`AddScreenRect` shift incoming rects by minus the
  object's own interpolation gap (virtual `GetDrawGapX/Y`, overridden by
  `MCreature`) and minus the camera gap. For the player both cancel to zero —
  matching the pinned on-screen position.
- `ScreenToPixel`, `GetSelectedSector`, `GetSelectedObject`, and
  `GetSelectedObjectSprite` add the camera gap back. This also fixes spell
  re-aiming while moving (`MPlayer::CheckRepeatAction` aims through
  `ScreenToPixel`).

While the player stands still every correction is zero. Verified in-game:
continuous attacks work again, both stationary and while moving.

## Deliberately deferred

- **Live window resizing.** The SDL renderer wraps a foreign Win32 window
  (`SDL_CreateWindowFrom`) and does not reliably track swapchain resizes, so
  the window is pinned at its chosen size (`WM_GETMINMAXINFO`).
- **True widescreen FOV** — see above; a server-and-UI project, not a
  rendering one.
- **Shader-based pixel-art scalers (xBR/CRT)** — the present path now has a
  natural insertion point if ever wanted; ReShade works today as an external
  option.
- The UI→client message pump (`_DispatchMessage` in
  `VS_UI_ui_result_receiver.cpp`) pops exactly one message per tick — a
  pre-existing 16-messages-per-second bottleneck noted during this
  investigation, unrelated to any current symptom.
