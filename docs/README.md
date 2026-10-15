# AthenaEnv Documentation

Enhanced JavaScript environment for PlayStation 2™. This is the full reference documentation, split by topic.

See the [project README](../README.md) for a quick overview and Hello World.

## Guides

* [Getting Started](GETTING_STARTED.md) — prerequisites, quick start, JavaScript language features (Float32), the error reporting system
* [Module System](MODULE_SYSTEM.md) — JavaScript modules and native (C) modules
* [Building AthenaEnv](BUILDING_ATHENA.md) — how to build and customize the ELF
* [Pure-C API](C_API.md) — the `athena/*.h` layer QuickJS bindings wrap
* [Native Compiler](NATIVE_COMPILER.md) — the AOT JS→MIPS compiler (`Native.compile`/`Native.struct`)
* [Native Modules](NATIVE_MODULES.md) — writing your own native (`.erl`) modules

## API Reference

* [Core](CORE_API.md) — `std`, `os`, `System`, `IOP`, `Archive`, `Mutex`, `Thread`, `Timer`
* [Math](MATH_API.md) — `Vector4`, `Matrix4`, `Color`
* [2D Graphics](GRAPHICS_2D_API.md) — `Image`, `ImageList`, `Draw`, `Screen`, `Font`
* [TileMap](TILEMAP.md) — high-throughput 2D tilemap renderer
* [3D Rendering](RENDER_3D_API.md) — `Render`, `RenderData`, `RenderObject`, `Batch`, `SceneNode`, `AsyncLoader`, `AnimCollection`, `Camera`, `Lights`, `Shadows`
* [Input](INPUT_API.md) — `Pads`, `Keyboard`, `Mouse`
* [Audio](AUDIO_API.md) — `Sound`
* [Video](VIDEO.md) — MPEG-1/2 playback
* [Network](NETWORK_API.md) — `Network`, `Request`, `Socket`, `WebSocket`
* [Physics](PHYSICS_API.md) — `Physics` (rigid bodies, collision, joints)

## Conventions

* *Italic* parameters in a function signature are optional.
* Code samples are plain JavaScript, runnable as-is inside an AthenaEnv script.
