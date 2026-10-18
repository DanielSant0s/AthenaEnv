# Native particles and 2D FX

`Particles.System` provides reusable emitters, a fixed particle pool and batched textured sprites. It is available globally with `GRAPHICS=1`, including builds with `NATIVE_COMPILER=0`. It can also be imported with `import { System } from "Particles"`.

Simulation, culling, camera projection, stable depth sorting and sprite packing run in native C on the EE. VU1 builds sprite packets through the TileMap microprogram; the GS performs texture sampling, blending and depth testing. There are no per-particle JS calls or `Image.draw()` calls.

## Start here

```js
const atlas = new Image("render/fx_atlas.png");
atlas.filter = LINEAR;
const fx = new Particles.System({ texture: atlas, capacity: 512 });
const sparks = fx.createEmitter({
    frame: [32, 0, 32, 32],
    lifetime: [0.3, 0.8], size: [3, 7], scaleEnd: 0.25,
    velocityMin: [-120, -180, 0], velocityMax: [120, 30, 0],
    acceleration: [0, 300, 0],
    color: [180, 90, 20, 128], colorEnd: [128, 20, 4, 0]
});

// A game event, such as a hit. Returns how many fit in the pool.
sparks.burst(32, 320, 224);

// Each frame: update once, clear/draw your background, then draw the FX.
fx.update(dtSeconds);
fx.draw();

// On scene teardown. Does not free the shared Image.
fx.dispose();
```

Run [bin/particles.js](../bin/particles.js) for a complete screen-space example. [bin/physics_collision.js](../bin/physics_collision.js) uses world-space dust and fragments on footsteps, landings and prop impacts.

## System options

| Option | Default | Meaning |
|---|---|---|
| `texture` | required | A loaded `Image`, shared by every emitter in the system |
| `capacity` | `512` | Fixed pool size, integer `1..16384` |
| `space` | `"screen"` | `"screen"` for pixels; `"world"` for camera-facing billboards in world units |
| `blend` | `"alpha"` | `"alpha"` or `"additive"` |
| `sort` | true for world, false for screen | Stable back-to-front depth sort |
| `depthTest` | true for world, false for screen | Test against existing scene depth using reversed-Z GEQUAL |
| `seed` | `42978` | Unsigned 32-bit seed for deterministic emission |

Screen-space positions are sprite **centers**, with positive Y down. Screen Z defaults to zero; greater Z is closer. World-space positions and sizes use the same units as `RenderObject`; positive Y follows your world. Call `Render.setView()` and `Camera.update()` before drawing world FX. `draw()` uses their current matrices and viewport, without a second JS camera/projection setup.

Particles do not write depth. Draw opaque world geometry first, particles next, and HUD last. Draw systems in the desired compositing order: sorting is within each system. Different textures or blend equations require separate systems. Use one atlas/system for dust and debris that need a shared transparency order.

## Emitter options

Create emitter definitions once and reuse them. A system supports up to 64 definitions; call `configure()` to change one instead of creating a new emitter on each impact.

| Option | Default | Meaning |
|---|---|---|
| `lifetime` | `1` | Positive seconds, or `[min,max]`, sampled at birth |
| `size` | `16` | Positive initial height, or `[min,max]`, sampled at birth |
| `scaleEnd` | `1` | Final size multiplier; linear over lifetime, zero allowed |
| `aspect` | `1` | Width/height ratio |
| `velocityMin`, `velocityMax` | `[0,0,0]` | Random initial velocity bounds, units/second |
| `spread` | `[0,0,0]` | Spawn box half extents around the emitter/event position |
| `acceleration` | `[0,0,0]` | Units/second squared, e.g. screen gravity `[0,300,0]` |
| `drag` | `0` | Exponential velocity damping coefficient per second |
| `color` | `[128,128,128,128]` | Initial GS modulation RGBA |
| `colorEnd` | `[128,128,128,0]` | Final modulation RGBA; linear over lifetime |
| `fadeIn`, `fadeOut` | `0` | Additional alpha fades as fractions of lifetime `0..1` |
| `frame` | whole texture | First atlas rectangle `[x,y,width,height]`, in texels |
| `frameCount` | `1` | Animation cells, `1..4096` |
| `frameColumns` | `1` | Cells per atlas row, `1..4096`; rows advance downward |
| `frameRate` | `0` | Frames per second; zero holds the first cell |
| `frameLoop` | `false` | Loop animation; otherwise hold its last frame |
| `rate` | `0` | Continuous births per second, `0..1000000`, enabled by `start()` |

Vectors/colors also accept `Float32Array`. Ranges must be ordered and values finite. Invalid options throw before changing a live emitter. Atlas cells must fit the texture. UVs include a half-texel inset for linear filtering.

As elsewhere in Athena, RGB `128` means neutral texture modulation, and alpha `128` means full opacity. Channels accept `0..255`. Specify both endpoint colors to keep a custom tint constant while fading alpha. `fadeIn`/`fadeOut` multiply the interpolated alpha, rather than replacing it.

Particle sprites are axis-aligned in screen space. World billboards face the camera; they do not rotate individually or simulate mesh fragments. Collision detection stays in the game's physics layer: call `burst()` from collision events as the physics example does.

## Methods

```js
const smoke = fx.createEmitter({ rate: 40, /* ... */ });
smoke.setPosition(x, y, z); // z defaults to 0
smoke.start();             // use configured rate
smoke.start(80);           // start/change continuous rate
smoke.stop();              // stop new births; live particles finish
smoke.burst(20);           // use emitter position
smoke.burst(20, x, y, z);  // one-shot position; does not move the emitter
smoke.configure({ drag: 2, colorEnd: [128, 128, 128, 0] });

fx.update(dtSeconds);     // finite, nonnegative; zero pauses simulation
const visible = fx.draw();
const active = fx.activeCount;
const capacity = fx.capacity;
const stats = fx.stats();
fx.clear();              // particles, counters and fractional emission carry
fx.clear(1234);          // also reset RNG for reproducible playback
fx.dispose();            // idempotent; other calls then throw
```

`configure()` patches settings; existing particles keep their sampled lifetime and initial size. Their other appearance/motion settings use the current emitter definition. `configure({rate})` sets the rate used on the next `start()`; use `start(newRate)` to change a running stream.

Call `update()` once per simulation tick, in seconds. The API does not clamp time or accumulate hidden simulation steps. Existing particles are integrated before continuous newborns are added; newborns begin at age zero. A newborn with `fadeIn > 0` is initially transparent. Use a fixed timestep if the game needs repeatable physics; identical calls, configuration and seed produce repeatable particles.

A full pool drops **new** births and returns the accepted count from `burst()`. Continuous emission retains fractional births, without queuing a backlog when full. Emitters share the pool; earlier emitters get first access to free slots during `update()`. `clear()` keeps emitter positions, definitions and running flags. Stop streams before clearing if you want them to stay empty.

`stats()` returns `capacity`, `active`, `visible`, `batches`, `emitted`, `dropped`, and `sort` (`"none"`, `"cached"`, `"radix"`). Visible/batches describe the most recent draw; emitted/dropped are saturating unsigned counters since creation/clear. Cached merge sort handles up to 256 visible sprites; larger lists use stable radix sorting. Batches contain up to 50 visible sprites; unused capacity is not submitted.

## Lifetime and renderer integration

The system retains its image, and emitter handles retain their system. Explicitly freeing that image makes `draw()` throw. Dispose systems before freeing shared textures or switching scenes. In C, the caller owns the image and core; destroy the renderer before either of them.

Pools, sorting scratch and two aligned sprite buffers are allocated at construction. Simulation and drawing allocate no particle objects. Native buffers wait for their DMA generation before reuse/destruction, allowing rendering overlap without JS-owned mutable-buffer restrictions. Drawing restores TEST, ZBUF, blend equation, alpha/fog flags, the TileMap camera and the VU1 buffer layout. If the prior layout was the 3D renderer's, its static lighting block is re-uploaded too. TileMap invalidates the cached VU view so a subsequent 3D draw reloads it.

The core C API is in [`particles.h`](../src/include/particles.h); PS2 rendering is in [`athena/particles.h`](../src/include/athena/particles.h), included by `athena/athena.h` in graphics builds.

