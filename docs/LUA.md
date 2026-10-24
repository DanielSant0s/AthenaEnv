# Lua runtime

AthenaEnv can be built with **Lua 5.4** as its scripting engine instead of QuickJS. The Lua target has the same modules as the JavaScript one: 2D/3D rendering, physics, tilemaps, particles, audio, input, network, video and archives. It runs the same AthenaOS shell and ships a Lua version of every sample in `examples/`.

Both runtimes sit on top of the same C facade (`src/athena/`, see [Pure-C API](C_API.md)). Only the binding layer differs: `src/js_api/` for QuickJS and `src/lua_api/` for Lua.

## Building

```sh
make SCRIPT_ENGINE=lua        # bin/athena_lua.elf (+ athena_lua_pkd.elf)
make                          # the JavaScript build, unchanged (SCRIPT_ENGINE=js)
```

The Lua build produces:

| File | Contents |
|---|---|
| `athena_lua.elf` | Runtime, graphics, render, physics, tilemap, particles, audio, input |
| `athena_network_lua.erl` | `Network`, `Request`, `Socket`, `WebSocket` |
| `athena_archive_lua.erl` | `Archive` |
| `athena_mpeg_lua.erl` | `Video` |

The relocatable modules work as in the JS build, and `STATIC_NETWORK=1`, `STATIC_ARCHIVE=1` and `STATIC_MPEG_VIDEO=1` link them in instead. Keyboard, mouse and camera are always built in for Lua. The [native compiler](NATIVE_COMPILER.md) is JavaScript-only, since it compiles QuickJS bytecode.

The Lua VM is vendored in `src/lua/` and built with `LUA_32BITS` (32-bit integers, single-precision floats), which matches the EE's single-precision FPU.

The boot script is `main.lua`. Set `default_script` in `athena.ini` to run another one.

## A first script

```lua
local font = Font("default")
local pad = Pads.get()

while true do
  pad:update()
  if pad:justPressed(Pads.TRIANGLE) then System.reload("main.lua") end

  Screen.clear(Color.new(40, 40, 40))
  font:print(20, 20, "Hello from Lua!")
  Screen.flip()
end
```

Every global module from the JS runtime exists under the same name: `Screen`, `Image`, `Font`, `Draw`, `Render`, `Physics` and the rest. Calls follow Lua style:

* **Methods use `:`**: `pad:update()`, `image:draw(x, y)`, `ro:render()`.
* **Constructors are calls, without `new`**: `Image("a.png")`, `RenderObject(data)`, `Request()`. `Color.new` and `Lights.new` keep their names.
* **Properties are plain fields**: `image.width = 64`, `font.scale = 0.6`, `video.loop = true`.

## Differences from the JavaScript runtime

| JavaScript | Lua |
|---|---|
| `new Image(path)` | `Image(path)` |
| `ArrayBuffer`, `Float32Array`, `DataView` | `Buffer(size)` with `getU8/I8/U16/I16/U32/I32/F32` and matching setters, `fill`, `copy`, `#buf` |
| `System.JS_BUFFER` (in `nativeCall`) | `System.T_BUFFER` |
| `matrix[0]`..`matrix[15]` | `matrix[1]`..`matrix[16]` |
| `v1.add(v2)` | `v1 + v2` (also `-`, `*`, `/`, unary `-`, `==`) |
| `const p = body.getPosition()` → `{x,y,z}` | `local x, y, z = body:getPosition()` (multiple returns) |
| `Screen.alphaEquation(...)` → object | an `{a, b, c, d, fix}` table |
| `std.loadFile` / `std.open(...).puts` | `io.readfile(path)` / `io.writefile(path, data)` / `io.exists(path)` |
| `os.setTimeout`, `os.setInterval` | same names on `os` |
| `async function` / `await` | `async(fn)`, `await(x)`, `sleep(ms)` (coroutines, see below) |
| `JSON.parse` / `JSON.stringify` | same, plus `JSON.null` and `JSON.array(t)` |
| `std.reload(script)` | `System.reload(script)` |
| `Camera` USB module | `EyeToy` |
| `Thread` (EE thread) | `Thread(fn)`, cooperative (see below) |
| `import ... from 'athena_network.erl'` | nothing: first use of `Network`/`Request`/... loads the module |
| `import { x } from 'lib.js'` | `local lib = require "lib"` (`?.lua`, `?/init.lua` from the boot folder) |

**Index base.** Lua tables, lists returned by the API, `AnimCollection` and `Matrix4` are 1-based. Values that name hardware or memory stay 0-based: texture ids, material indices, byte offsets in a `Buffer`, sprite records in a `TileMap.SpriteBuffer`, pad ports and IOP ids.

**Numbers.** Integers are 32-bit and wrap on overflow. `//` is integer division and `&`, `|`, `~`, `<<`, `>>` are native bitwise operators. Floats are single precision.

**Locals.** Lua allows at most 200 local variables per function, and the main chunk of a script counts as one function. Large scripts should group helpers in tables or `do ... end` blocks (`starfox.lua` does this).

## Main loop, timers and coroutines

A script either owns its loop (`while true do ... Screen.flip() end`) or registers a render function:

```lua
local pad = Pads.get()
pad:setEventHandler()          -- the runtime updates this pad every frame

Screen.display(function()
  Screen.clear(Color.new(0, 0, 0))
  -- draw...
end)                           -- the runtime clears, calls this and flips
```

Timers and coroutine tasks run in both styles. `Screen.flip()` pumps them, and so does the runtime loop after the main chunk returns. That loop keeps running while timers or tasks are pending, like the JS event loop.

```lua
os.setInterval(function() print("tick") end, 1000)

async(function()
  local req = Request()
  req:asyncGet("https://example.com")
  await(req)                   -- yields until req:ready(); transfer errors are raised here
  print(#req:getAsyncData())
  sleep(500)                   -- await(500)
end)
```

`await` accepts a number of milliseconds, a function (the task waits until it returns a truthy value) or any object with a `:ready()` method, such as a task returned by `async` or a `Request`.

A Lua state is not thread-safe. So `Thread(fn, name)` creates a cooperative task that advances when it yields (`sleep`, `await` or `Thread.yield()`). Blocking native work still runs on EE threads in C: file copies, `ImageList`, `AsyncLoader`, network transfers and audio. `Thread.list()` and `Thread.kill(id)` act on the real EE task table.

## Errors and reloading

A script error, or an error thrown from a callback, timer or task, shows the standard AthenaEnv error screen with a Lua traceback. `System.reload(script)` restarts the VM with another script. The AthenaOS shell uses it to launch apps and to return to the shell.

## AthenaOS apps

The shell (`main.lua`) lists every `.lua` file whose first line is a JSON header, the same format as the JS apps:

```lua
-- {"name": "My app", "author": "me", "version": "1", "file": "my_app.lua", "category": "games", "exit": "TRIANGLE"}
local app = require "athenaos.app"
local ui = app.AppUI{ title = "My app", category = "games" }
```

`athenaos/app.lua` provides `AppUI`, `Terminal`, `COLORS`, `rgba` and `exitToShell` with the same layout and behavior as `athenaos/app.js`. Settings are shared with the JS shell through `settings.json`.

## Editor support

`.luarc.json` at the repository root points [lua-language-server](https://luals.github.io/) at `types/lua/`. That gives completion and type checking for the whole API in VS Code (sumneko.lua), Neovim and other LSP editors.

## Performance

Measured in PCSX2 after the QuickJS performance work (float32 numbers, TDZ check elimination, inline caches, superinstructions, allocator), on the same scenes with the same scripted input for both runtimes. `starfox` spawns random content and its frame rate depends on it, so both runs use the same random number sequence. `physics_collision` runs with its dynamic shadows on (the default; L3 cycles exact shadows, fast overlaps and off); with them off it holds 60 FPS:

| Scene | JavaScript | Lua |
|---|---|---|
| `physics_collision` (lights, post-FX and shadows on) | 48 FPS | 50 FPS |
| `physics_collision` (lights on, post-FX off, shadows on) | 59 FPS | 59 FPS |
| `physics_collision` (lights, post-FX and fast-overlap shadows on) | 54 FPS | 56 FPS |
| `starfox` gameplay, vsync on | 57 FPS | 58 FPS |
| `starfox` gameplay, vsync off (equivalent pure scripts) | 196 FPS | 198 FPS |
| `starfox` gameplay, vsync off (`starfox.js` with `Native.compile`) | 216 FPS | - |

Object creation (`{x, y, z}` literals) and number-to-string conversion are faster in JS. Plain interpreter loops (integer and float arithmetic, small function calls) are still 1.2–2x faster in Lua, whose register-based bytecode needs fewer instructions per operation.
