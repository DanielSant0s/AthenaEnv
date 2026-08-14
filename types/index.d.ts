/**
 * AthenaEnv — ambient type declarations.
 *
 * These declarations describe the global JavaScript environment exposed by
 * the AthenaEnv PS2 runtime (a modified QuickJS). Every symbol referenced
 * here is installed directly on `globalThis` by the native bootstrap
 * (`athena_js_globals_prelude()` in `src/js_api/ath_env.c`) before any user
 * script runs — there is no `import` required, and none of these files
 * declare a top-level `import`/`export` themselves (which is what makes
 * them *global* ambient declarations rather than module declarations).
 *
 * Reference this file (or the whole `types/` directory) from your editor —
 * VS Code and other TS-language-service-backed editors will pick these up
 * automatically via the root `jsconfig.json`.
 *
 * Layout:
 * - core.d.ts       — std, os, System, IOP, Archive, Mutex, Thread, Timer
 * - math.d.ts        — Vector2, Vector3, Vector4, Matrix4
 * - input.d.ts        — Pads, Keyboard, Mouse
 * - graphics2d.d.ts    — Screen, Draw, Color, Image, ImageList, Font, TileMap
 * - render3d.d.ts      — Render, RenderData, RenderObject, Batch, SceneNode,
 *                        AsyncLoader, AnimCollection, Camera, Lights, Shadows
 * - audio.d.ts         — Sound, Video
 * - network.d.ts        — Network, Request, Socket, WebSocket
 * - physics.d.ts        — ODE
 * - native.d.ts         — Native (AOT compiler)
 *
 * Every declaration here was cross-checked against the native C bindings in
 * `src/js_api/*.c`.
 *
 * @see https://github.com/DanielSant0s/AthenaEnv
 */

/// <reference path="./core.d.ts" />
/// <reference path="./math.d.ts" />
/// <reference path="./input.d.ts" />
/// <reference path="./graphics2d.d.ts" />
/// <reference path="./render3d.d.ts" />
/// <reference path="./audio.d.ts" />
/// <reference path="./network.d.ts" />
/// <reference path="./physics.d.ts" />
/// <reference path="./native.d.ts" />
