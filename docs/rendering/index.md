# Designs

* [60 fps rendering over the 62 ms logic tick](frame-rate-interpolation-2026-08-30.md) - How the renderer interpolates between the game's 62 ms logic ticks to draw at the display's rate without changing the simulation.
* [Native GPU sprite composition](native-gpu-renderer.md) - SpriteGpu composes sprites, glyphs, fills and scaled copies on SDL's accelerated device, with the CPU rasterizer kept as the fallback.
* [Widescreen fullscreen and HD upscaling](widescreen-scaling-2026-08-30.md) - The game keeps rendering at 800x600 or 1024x768 and the presented frame is scaled to the display, with input mapped back.
