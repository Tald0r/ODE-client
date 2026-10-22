---
type: Design
title: "60 fps rendering over the 62 ms logic tick"
description: "How the renderer interpolates between the game's 62 ms logic ticks to draw at the display's rate without changing the simulation."
tags: [rendering]
---

# 60 fps rendering over the 62 ms logic tick

**Date:** 2026-08-30
**Branch:** `perf/in-game-frame-rate`
**Status:** Implemented, verified in-game by the repo owner ("buttery smooth").

---

## The constraint this design lives under

The game simulation is hard-locked to **one logic tick per 62 ms (~16.1 Hz)**.
This is not a performance defect — it is the core game loop:

- `g_UpdateDelay` is forced to 62 in `CGameUpdate.cpp` (the gate constant is
  obfuscated as `100-20+40-38-19-1`, assignments elsewhere as `100 ^ 90` and
  `54 ^ 8`, all = 62).
- It is **anti-cheat-guarded**: if the value is ever not 62, the client resets
  it and fires a burst of `CGVerifyTime` packets at the server
  (`CGameUpdate.cpp`, the block above the tick gate).
- Movement speed, animation counts, and server synchronisation all assume it.
  Speeding the tick is a speedhack, not an optimisation.

Before this change the world was also only *drawn* on ticks, so the client
rendered at 16 fps. The fix decouples rendering from simulation: the logic
still ticks at 16.1 Hz, untouched, while the renderer presents **interpolated
frames between ticks** at the monitor refresh rate.

This is the same idea as RuneLite's "animation smoothing" for OSRS (0.6 s
ticks, interpolated rendering) adapted to a 2D sprite engine: positions and
the camera interpolate; the hand-drawn sprite frames themselves cannot (there
is no in-between bitmap the way there is an in-between skeletal pose), and do
not need to — sprite art flipping at 16 Hz over smoothly gliding positions is
how polished pixel-art games read as smooth.

## How it works

All of it landed in commit `fbfc274`.

### Interpolation state (`Client/MCreature.h` / `MCreature.cpp`)

- `MZone::UpdateAllCreature` calls `MCreature::SnapshotDrawState()` once per
  tick for every creature, **before** `Action()` moves it. The call sits above
  the Action calls because the player can legitimately `Action()` twice per
  tick (ghost / dragon-eyes double speed) and the snapshot must be the
  pre-move state.
- `GetPixelX()/GetPixelY()` interpolate between the snapshot and the exact
  position by `g_DrawAlphaNum` — the elapsed fraction of the current tick in
  0..256 fixed point — but **only while `g_bInterpolateDraw` is set**, which
  `CGameUpdate::UpdateDraw` scopes tightly around the world draw. Game logic
  always reads exact tick positions.
- Safety valves: a gap larger than 96 px means teleport/respawn and snaps
  instead of gliding; an object whose `m_DrawSnapFrame` is not the current
  tick (fresh spawn, corpse, anything unticked) falls back to its exact
  position.

### Camera (`Client/CGameUpdate.cpp` → `MTopView::DrawZone`)

`DrawZone` recomputes the camera from scratch every draw out of the player's
exact tile + sub-tile position, so between ticks it would still step. Rather
than modify `DrawZone`, `UpdateDraw` passes the player's interpolation gap
(negated) through the **existing `Draw(x, y)` screen-shake offsets**, which
feed directly into `m_PlusPoint`. Two properties fall out by construction:

- the player stays pinned to the same screen position (the player sprite is
  drawn at the interpolated position, and the camera is biased by the same
  gap);
- the zone-edge clamp zeroes the bias along with `m_PlusPoint`, and the
  tile-surface cache moves in whole sectors, so both tolerate the sub-tile
  offset without changes.

### Frame pacing

- The SDL renderer (`Client/DXLib/CDirectDraw.cpp`) is created with
  `SDL_RENDERER_PRESENTVSYNC` (fallback chain: accelerated+vsync →
  accelerated → default), so presents block until vblank and pace the loop.
- The draw gate in `CGameUpdate::Update` draws between ticks with a 15 ms
  fallback cap for renderers that ignore vsync (`SDL_RENDER_DRIVER=software`),
  so the message loop cannot spin-present.

### Draw-rate vs tick-rate guards

Anything that mutates state during the draw and assumed draw == tick needed a
guard on `g_bFrameChanged` (true only on iterations where a tick fired):

- `MTopView::DrawFade` — the `m_FadeValue += m_FadeInc` step.
- The advancement-quest ending crossfade — was `g_FrameCount & 0x01`
  (every other *draw*); now `g_bFrameChanged && (g_CurrentFrame & 0x01)` to
  keep the original every-other-tick pace.
- Already correctly guarded by the original authors: looping animation-object
  frame advances (`MAnimationObject::NextLoopFrame` / `NextFrame`) and the
  screen-shake step counter.
- Needed nothing: cutscene scripts, message-scroll timers, and the click
  marker are all wall-clock driven.

### Retired: the smooth-cursor surface ping-pong

`UseSmoothCursor` drove a store/restore of the background under the mouse plus
cursor-only flips, to move the cursor between 16 fps world draws. With real
frames going out every refresh, its saved rectangles would smear over
interpolated frames — `g_bSmoothCursor` is now forced false and the cursor is
simply drawn into every frame.

## What is and is not smoothed

| Thing | Rate | Why |
|---|---|---|
| Camera / screen panning | refresh rate | interpolated |
| Creature & player movement | refresh rate | interpolated |
| Mouse cursor | refresh rate | drawn per frame |
| Sprite animation frames | 16 Hz | discrete hand-drawn art; no in-between exists |
| Spell effects / projectiles | 16 Hz | `MEffect` is not `MCreature`; see follow-ups |
| UI blink/marquee | per draw | cosmetic, runs a little faster than before |
| Input sampling | per tick | unchanged; click latency same as always |

## Verification

No test binary can reach this code (see CLAUDE.md, *Testability*). Verified by
the repo owner running the client in-game: ~60 fps on the FPS counter, smooth
panning while walking, creatures gliding, movement speed and animation timing
unchanged.

## Follow-ups, if ever wanted

- **Effect/projectile interpolation** — extend the same snapshot+lerp pattern
  to `MEffect` (it keeps its own `m_PixelX/m_PixelY`); fireballs still step at
  16 Hz against the smooth world.
- **Frame-crossfade "smoothing"** was considered and rejected: alpha-blending
  adjacent sprite frames produces ghosting, especially on attack animations.
- The `NonBlockingIOException` storm (thousands of throw/unwind cycles per
  second in the socket poll path) is unrelated to frame rate now but still
  burns CPU every tick — separate issue, see
  `docs/reports/memory-leak-investigation-2026-08-30.md`, *Side findings*.
- If anything ever looks mistimed after merging (fades, cutscene pace, screen
  shake), the guards listed above are the complete set of places where
  draw-rate used to equal tick-rate.
