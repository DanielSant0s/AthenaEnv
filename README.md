<br />
<p align="center">
  <a href="https://github.com/DanielSant0s/AthenaEnv/">
    <img src="https://github.com/DanielSant0s/AthenaEnv/assets/47725160/f507ad9b-f9a1-4000-a454-ff824bc9d70b" alt="Logo" width="100%" height="auto">
  </a>

  <p align="center">
    Enhanced JavaScript environment for PlayStation 2™
    <br />
  </p>
</p>

<details open="open">
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#about-athenaenv">About AthenaEnv</a></li>
    <li><a href="#quick-start">Quick Start</a></li>
    <li><a href="#documentation">Documentation</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#thanks">Thanks</a></li>
  </ol>
</details>

## About AthenaEnv

AthenaEnv is a complete JavaScript Runtime Environment for the PlayStation 2. It has dozens of built-in exclusive libraries made for the project, such as an MMI instruction accelerated 2D renderer, asynchronous texture manager system, an advanced 3D renderer powered by VU1 and VU0, and even a custom-built HTTP/S client. It lets you write modern code on PS2 powered by performant libraries and a fine-tuned interpreter.

### Modules

| Module | Highlights |
|---|---|
| **System** | File/folder ops, mass device control, machine info (CPU/GPU/memory/temperature), automatic LZ4 compaction of idle textures and mesh buffers, native function calls, dynamic libraries |
| **Render** | VU1/VU0 3D pipeline, lighting, clipping, culling, skinning, materials, batching (`Batch`), scene graph (`SceneNode`), streaming (`AsyncLoader`) |
| **Screen** | Vsync/FPS control, alpha/scissor/depth render params, off-screen rendering, video modes |
| **Image** | PNG/BMP/JPEG, nearest/bilinear filters, off-screen surfaces, async loading |
| **Draw** | MMI accelerated points, lines, rects, circles, triangles and quads (flat/gouraud) |
| **TileMap** | High-throughput VU1 2D tilemap renderer, `ArrayBuffer`-backed sprite buffers |
| **Font** | MMI accelerated, FreeType and Image fonts, outline/dropshadow, alignment |
| **Pads** | DS2/3/4 support, type recognition, pressure sensitivity, rumble, connection events |
| **Sound / Video** | ADPCM effects, WAV/OGG streams, MPEG-1/2 playback |
| **Physics** | VU0/MMI hardware-accelerated rigid-body dynamics, BVH triangle meshes, collision shapes, raycasting, and joints |
| **Shadows** | Grid-based real-time shadow projection with physics ray casting |
| **Network / Socket** | HTTP/HTTPS, TLS 1.2, async requests, classic sockets, WebSockets |
| **IOP / Archive** | I/O module manager, ZIP/GZ/TAR extraction |
| **Timer / Keyboard / Mouse** | Independent timers, USB keyboard and mouse |

New types are always being added and this list can grow a lot over time, so stay tuned.

### Built With

* [ps2dev](https://github.com/ps2dev/ps2dev)
* [QuickJS](https://bellard.org/quickjs/)

To learn how to build and customize: [Building AthenaEnv](docs/BUILDING_ATHENA.md)

## Quick Start

Using AthenaEnv you only need one way to code and one way to test your code — see [Getting Started](docs/GETTING_STARTED.md) for prerequisites and editor/emulator setup.

**Hello World:**
```js
const font = new Font("default");

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false); // Before doing 2D, disable depth!

os.setInterval(() => { // Basically creates an infinite loop, similar to while true (you can use it too).
  Screen.clear(); // Clear screen for the next frame.
  font.print(0, 0, "Hello World!"); // x, y, text
  Screen.flip(); // Updates the screen.
}, 0);
```
See more examples at [AthenaEnv samples](https://github.com/DanielSant0s/AthenaEnv/tree/main/bin).

## Documentation

The full API reference and guides live in [`docs/`](docs/README.md):

* [Getting Started](docs/GETTING_STARTED.md) — prerequisites, quick start, language features, error reporting
* [Module System](docs/MODULE_SYSTEM.md) — JavaScript and native modules
* [Lua Runtime](docs/LUA.md) — the alternative Lua 5.4 scripting engine (`make SCRIPT_ENGINE=lua`)
* [Core API](docs/CORE_API.md) — `std`, `os`, `System`, `IOP`, `Archive`, `Mutex`, `Thread`, `Timer`, automatic LZ4 EE RAM compaction
* [Math API](docs/MATH_API.md) — `Vector4`, `Matrix4`, `Color`
* [2D Graphics API](docs/GRAPHICS_2D_API.md) — `Image`, `ImageList`, `Draw`, `Screen`, `Font`
* [TileMap](docs/TILEMAP.md)
* [Particles and 2D FX](docs/PARTICLES.md) — native pooled emitters and VU1/GS sprites
* [3D Rendering API](docs/RENDER_3D_API.md) — `Render`, `RenderData`, `RenderObject`, `Batch`, `SceneNode`, `AsyncLoader`, `AnimCollection`, `Camera`, `Lights`, `Shadows`
* [Input API](docs/INPUT_API.md) — `Pads`, `Keyboard`, `Mouse`
* [Audio API](docs/AUDIO_API.md) — `Sound`
* [Video](docs/VIDEO.md) — MPEG-1/2 playback
* [Network API](docs/NETWORK_API.md) — `Network`, `Request`, `Socket`, `WebSocket`
* [Physics API](docs/PHYSICS_API.md) — `Physics` (rigid bodies, collision shapes, BVH trimeshes, raycasting, joints)
* [Native Compiler](docs/NATIVE_COMPILER.md) — AOT JS→MIPS compilation (`Native.compile`/`Native.struct`)
* [Native Modules](docs/NATIVE_MODULES.md) — writing your own native (`.erl`) modules
* [Pure-C API](docs/C_API.md) — the `athena/*.h` layer QuickJS bindings wrap

Editors/IDEs can also pick up the TSDoc-annotated `.d.ts` declarations in [`types/`](types/) for autocomplete and inline docs.

## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AwesomeFeature`)
3. Commit your Changes (`git commit -m 'Add some AwesomeFeature'`)
4. Push to the Branch (`git push origin feature/AwesomeFeature`)
5. Open a Pull Request

## License

Distributed under MIT. See [LICENSE](LICENSE) for more information.

<!-- CONTACT -->
## Contact

Daniel Santos - [@danadsees](https://twitter.com/danadsees) - danielsantos346@gmail.com

Project Link: [https://github.com/DanielSant0s/AthenaEnv](https://github.com/DanielSant0s/AthenaEnv)

## Thanks

Here are some direct and indirect thanks to the people who made the project viable!

* guiprav - for 3dcb-duktape, which was the inspiration for bringing Enceladus and JavaScript together
* HowlingWolf&Chelsea - Tests, tips and many other things
* Whole PS2DEV team
