# Getting Started

## About AthenaEnv

AthenaEnv is a complete JavaScript Runtime Environment for the PlayStation 2. It has dozens of built-in exclusive libraries made for the project, such as an MMI instruction accelerated 2D renderer, asynchronous texture manager system, an advanced 3D renderer powered by VU1 and VU0, and even a custom-built HTTP/S client. It lets you write modern code on PS2 powered by performant libraries and a fine-tuned interpreter.

Built with [ps2dev](https://github.com/ps2dev/ps2dev) and [QuickJS](https://bellard.org/quickjs/).

## Prerequisites

You only need one way to code and one way to test — you can even write code on the PS2 itself — but here are some recommendations:

* **PC**: [Visual Studio Code](https://code.visualstudio.com) (with the JavaScript extension) and [PCSX2](https://pcsx2.net/download/development/dev-windows.html) (1.7.0 or above, HostFS enabled) or PS2Client for testing.
* **Android**: [QuickEdit](https://play.google.com/store/apps/details?id=com.rhmsoft.edit&hl=pt_BR&gl=US) and a PS2 with wLE for testing.

Knowing how to code in JavaScript is, of course, an essential prerequisite.

### Enabling HostFS on PCSX2 1.7.0

**(old) WxWidgets:**

![image](https://user-images.githubusercontent.com/47725160/145600021-b07dd873-137d-4364-91ec-7ace0b1936e2.png)

**Qt version:**

<img src="https://github.com/DanielSant0s/AthenaEnv/assets/47725160/e90471f6-7ada-4176-88e8-8a9d2c1e7eb4" width=50% height=50%>

Qt recommendation — enable console output:

<img src="https://github.com/DanielSant0s/AthenaEnv/assets/47725160/7ced1570-0013-4072-ad01-66b8a63dab6e" width=50% height=50%>

## Quick Start

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
See more examples at [AthenaEnv samples](https://github.com/DanielSant0s/AthenaEnv/tree/main/examples).

### How to run it

Athena is basically a JavaScript loader, so it loads `.js` files. It runs `main.js` by default, but you can run other file names by changing `athena.ini`'s default script, or by passing it as the first argument when launching the ELF file.

## Language Features

AthenaEnv uses a slightly modified version of the QuickJS interpreter, which means it brings almost-modern JavaScript features.

### Float32

The PS2's FPU only handles 32-bit floats: doubles are emulated in software and cost tens to hundreds of cycles per operation. So on AthenaEnv, **non-integer numbers are single floats (float32) by default**, the same choice as Lua builds with `LUA_32BITS`:

```js
let a = 0.5;          // float32
let b = 1 / 3;        // float32: 0.33333334
let c = 7;            // int32
let d = 2 ** 40;      // double: integers past int32 stay exact (up to 2^53)
let t = Date.now();   // double, exact
```

| Value | Stored as |
|---|---|
| integer that fits in 32 bits | int32 |
| integer beyond int32 (up to 2^53), e.g. `Date.now()`, hashes | double (exact) |
| outside the float32 range (`Number.MAX_VALUE`, `Number.MIN_VALUE`) | double |
| anything else (literals, `/`, `Math.*`, `parseFloat`, `JSON.parse`...) | float32 |

Arithmetic, comparisons and `Math.*` on ints and float32 run on the FPU; an operation that involves a real double is done in double. Numbers print with the shortest digits that read back as the same value (`String(0.1)` is `"0.1"`, `String(1/3)` is `"0.33333334"`).

Things to keep in mind:
* Precision is ~7 significant digits. Large non-integer values (seconds since epoch with a fraction, for example) lose their fractional part; keep times in integer milliseconds.
* The EE FPU rounds toward zero, so `1 + 0.1` is `1.0999999` and results can differ in the last digit from a PC.
* `Math.fround(x)` and the `f` suffix (`15.0f`) still work.

Building with `-DJS_NO_FLOAT32_NUMBERS` restores standard double semantics (much slower on the EE).

### Performance notes

* `let`/`const` cost the same as `var`: the compiler removes the temporal-dead-zone checks it can prove unnecessary.
* Property reads and writes (`obj.x`), methods and getters/setters inherited from a class or a native binding (`obj.method()`, `font.scale`), globals (`Math`, `Screen`) and `arr.length` go through inline caches. Objects that keep the same shape run fastest.
* Common sequences run as one instruction: `local.x`, `arr.length` and `arr[i]` when `arr` is a module-level variable (the `for (let i = 0; i < arr.length; i++) { const e = arr[i]; ... }` loop).
* Prefer methods that take numbers over temporary objects in hot code: `ro.setPosition(x, y, z)` instead of `ro.position = { x, y, z }`.
* `System.profileStart(hz)` / `System.profileStop(path)` record a statistical profile; `scripts/profile_report.py` turns it into a report (see [Core API](CORE_API.md)).

### Native compilation

For math-heavy code, AthenaEnv can Ahead-Of-Time compile JavaScript functions directly to native MIPS R5900 machine code via `Native.compile()`/`Native.struct()`. See [Native Compiler](NATIVE_COMPILER.md) for the full API and technical reference.

## Error Reporting System

Athena has a consistent error system with two levels.

**Runtime error:**
A runtime error occurs on the JavaScript side. It can be rich in debug data and easily fixable — it points at the error type, custom message, file, line, and even has a color code.

<img src="https://user-images.githubusercontent.com/47725160/230756861-94df60f8-8550-43a3-ac43-56d150e94145.png" width=50% height=50%>

**Core error:**
A core error happens when AthenaEnv is interrupted due to a critical exception (null pointer, invalid address, bus error, overflow, etc.). It is a lot harder to debug and requires low-level programming skills to figure out. From that screen you get the exception cause, processor register dump, the bad address (if address-related), the return function name (+offset), and the function that caused the error (+offset). It is highly recommended to contact the author for support with these.

<img src="https://github.com/user-attachments/assets/25da3350-cd53-4302-bf5e-3b7fdebcb4bb" width=50% height=50%>

## Next steps

* [Module System](MODULE_SYSTEM.md) — importing JavaScript and native modules
* [API Reference](README.md#api-reference) — the full function/class reference, by topic
