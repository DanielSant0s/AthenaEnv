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
    <li>
      <a href="#about-athenaenv">About AthenaEnv</a>
      <ul>
        <li><a href="#function-types">Function types</a></li>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#coding">Coding</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#features">Features</a></li>
        <li><a href="#functions-classes-and-consts">Functions and classes</a></li>
      </ul>
    </li>
    <li><a href="#module-system">Module system</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#thanks">Thanks</a></li>
  </ol>
</details>

## About AthenaEnv

AthenaEnv is a complete JavaScript Runtime Environment for the PlayStation 2. It has dozens of built-in exclusive libraries made for the project, such as a MMI instruction accelerated 2D renderer, asynchronous texture manager system, an advanced 3D renderer powered by VU1 and VU0 and even a custom build HTTP/S client. It let's you to almost instantly write modern code on PS2 powered by performant libraries and a fine-tuned interpreter for it.

### Modules:
* System: Files, folders and system stuff.  
  • File operations  
  • Folder operations  
  • Mass device control  
  • Get machine info (CPU, GPU, memory, temperature)   
  • Native function control (call C functions)   
  • Load and use native dynamic libraries   

* Archive: A simple compressed file extractor.  
  • ZIP, GZ and TAR support   

* IOP: I/O driver and module manager.  
  • Module register  
  • I/O memory tracking  
  • Reverse init/end callbacks  
  • Smart reset routine  

* Image: Renderable surfaces.  
  • PNG, BMP and JPEG formats support  
  • Nearest and bilinear filters  
  • Off-screen rendering surfaces  
  • Surface cache system, with locks  
  • Asynchronous image list loading  

* Draw: Shape drawing.  
  • MMI accelerated  
  • Points  
  • Lines  
  • Rectangles  
  • Circles  
  • Triangles (flat, gouraud)  
  • Quads (flat, gouraud)  

* Render: High performance 3D renderer.  
  • VU1 vertex transformer  
  • VU0 matrix processor  
  • LookAt camera system  
  • Ambient lighting  
  • Diffuse lighting  
  • Specular lighting  
  • Polygon clipping  
  • (Back/Front)face culling  
  • Per-vertex colors  
  • Emboss bump mapping  
  • Skinning & node transforming  
  • Environment/Reflection maps  
  • Multiple material processing  
  • Frame stats API (draw calls/triangles) for live tuning  
  • Optional zero-copy vertex buffers via `Render.vertexList(..., { shareBuffers: true })`  
  • High-level helpers: batching (`Batch`), scene graph (`SceneNode`) and cooperative streaming (`AsyncLoader`).  

  Export model: AthenaEnv exports constructors on globalThis via ath_env (no imports from C modules):
  ```js
  const batch = new Batch({ autoSort: true });
  const root = new SceneNode();
  const loader = new AsyncLoader({ jobsPerStep: 2 });
  ```

* Screen: Rendering and video control.  
  • Screen control params (vsync, FPS)  
  • Render params (alpha, scissor, depth)  
  • Dual drawing context (optimized for off-screen drawing)  
  • Accelerated render loop  
  • Off-screen rendering  
  • Screenspace control  
  • Video modes  

* TileMap: High-throughput 2D tilemap renderer built on QuickJS + VU1.  
  • Descriptor/instance model for reusing textures/materials  
  • Sprite buffers exposed as `ArrayBuffer` (mutate via DataView/TypedArray)  
  • Material-aware batching and automatic texture uploads  
  • See `docs/TILEMAP.md` + "Functions, classes and consts" for API/usage  

* Font: Text rendering.  
  • MMI accelerated  
  • Render-time resizer  
  • FreeType and Image fonts  
  • Outline and dropshadow support  
  • Alignment support  

* Pads: DS2/3/4 input support.  
  • Gamepad type recognition  
  • Pressure sentivity  
  • Rumble support  
  • Connection states  
  • Callback events  

* Sound: Audio functions.  
  • ADPCM sound effects  
  • Automatic channel allocator  
  • Per-effect volume controller  
  • Pan and pitch control for effects  
  • WAV and OGG stream sound support  
  • Loop and position control for streams

* Video: MPEG Video playback.  
  • MPEG-1/2 playback support  
  • Playback control (play, pause, stop)


* Shadows: Real-time shadow projection system.  
  • Grid-based shadow projectors  
  • ODE ray casting integration  
  • Multiple blend modes (darken, alpha, add)  
  • Configurable shadow parameters  
  • VU1 accelerated rendering  
  • Dynamic shadow following  

* Network: Net basics and web requests.  
  • HTTP/HTTPS support  
  • TLS 1.2 support  
  • A/Sync requests (GET, POST, HEAD)  
  • Download functions  
  • Static/DHCP  

* Socket: Well, sockets.  
  • Classic sockets  
  • WebSockets  

* Timer: A simple time manager.  
  • Separated unique timers  
  • Resolution selectable  

* Keyboard: Basic USB keyboard support.
* Mouse: Basic USB mouse support.
New types are always being added and this list can grow a lot over time, so stay tuned.

### Built With

* [ps2dev](https://github.com/ps2dev/ps2dev)
* [QuickJS](https://bellard.org/quickjs/)

To learn how to build and customize: [Building AthenaEnv](docs/BUILDING_ATHENA.md)

## Coding

In this section you will have some information about how to code using AthenaEnv, from prerequisites to useful functions and information about the language.

### Prerequisites

Using AthenaEnv you only need one way to code and one way to test your code, that is, if you want, you can even create your code on PS2, but I'll leave some recommendations below.

* PC: [Visual Studio Code](https://code.visualstudio.com)(with JavaScript extension) and [PCSX2](https://pcsx2.net/download/development/dev-windows.html)(1.7.0 or above, enabled HostFS is required) or PS2Client for test.
* How to enable HostFS on PCSX2 1.7.0:  
  • (old) WxWidgets:  
![image](https://user-images.githubusercontent.com/47725160/145600021-b07dd873-137d-4364-91ec-7ace0b1936e2.png)  
  
  • Qt version:  
<img src="https://github.com/DanielSant0s/AthenaEnv/assets/47725160/e90471f6-7ada-4176-88e8-8a9d2c1e7eb4" width=50% height=50%>
  
Qt recommendation: Enable console output  
<img src="https://github.com/DanielSant0s/AthenaEnv/assets/47725160/7ced1570-0013-4072-ad01-66b8a63dab6e" width=50% height=50%>

  
* Android: [QuickEdit](https://play.google.com/store/apps/details?id=com.rhmsoft.edit&hl=pt_BR&gl=US) and a PS2 with wLE for test.

Oh, and I also have to mention that an essential prerequisite for using AthenaEnv is knowing how to code in JavaScript.

## Quick start with Athena

**Hello World:**  
```js
const font = new Font("default");

Screen.setParam(Screen.DEPTH_TEST_ENABLE, false); // Before doing 2D, disable depth!

os.setInterval(() => { // Basically creates an infinite loop, similar to while true(you can use it too).
  Screen.clear(); // Clear screen for the next frame.
  font.print(0, 0, "Hello World!"); // x, y, text
  Screen.flip(); // Updates the screen.
}, 0);
```
See more examples at [AthenaEnv samples](https://github.com/DanielSant0s/AthenaEnv/tree/main/bin).

### Features

AthenaEnv uses a slightly modified version of the QuickJS interpreter for JavaScript language, which means that it brings almost modern JavaScript features so far.

**Float32**

This project introduces a (old)new data type for JavaScript: single floats. Despite being less accurate than the classic doubles for number semantics, they are important for performance on the PS2, as the console only processes 32-bit floats on its FPU.

You can write single floats on AthenaEnv following the syntax below:
```js
let test_float = 15.0f; // The 'f' suffix makes QuickJS recognizes it as a single float.
``` 
or  
```js
let test_float = Math.fround(15.0); // Math.fround returns real single floats on Athena.
``` 

### Manual native compilation

The Native module provides an **Ahead-Of-Time (AOT) compiler** that translates JavaScript functions into native MIPS R5900 machine code for **40x+ performance** on math-heavy operations.

**Key features:**
* Compile JavaScript to native assembly
* Support for int, float, int64, arrays, strings, and custom structs
* Full control flow (if/else, loops, switch)
* Call Math functions (sqrt, abs, min, max, etc.)
* Struct methods with native performance
* 100+ optimized operations (strength reduction, constant folding, etc.)

#### Native.compile()

Compiles a JavaScript function to native code.

```js
const add = Native.compile({
    args: ['int', 'int'],
    returns: 'int'
}, (a, b) => a + b);

console.log(add(5, 3)); // 8 - runs at native speed!
```

**Supported type names:**
* **Integers**: `'int'`, `'uint'`, `'int64'`, `'uint64'`
* **Floats**: `'float'` (single precision)
* **Arrays**: `'Int32Array'`, `'Float32Array'`, `'Uint32Array'`
* **Strings**: `'string'`
* **Pointers**: `'ptr'`
* **Structs**: Use your struct name (e.g., `'Vec3'`)
* **Void**: `'void'` (return type only)

**Supported JavaScript features:**
* ✅ Arithmetic: `+`, `-`, `*`, `/`, `%`, `++`, `--`
* ✅ Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`
* ✅ Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
* ✅ Control flow: `if/else`, `for`, `while`, `do-while`, `switch`, `break`, `continue`
* ✅ **Math intrinsics** (compiled to native MIPS FPU instructions):
  * **Basic math:**
    * `Math.sqrt(x)` - Square root (SQRT.S instruction)
    * `Math.abs(x)` - Absolute value (ABS.S instruction)
    * `Math.min(a, b)` - Minimum (C.LT.S comparison)
    * `Math.max(a, b)` - Maximum (C.LT.S comparison)
  * **Integer operations:**
    * `Math.sign(x)` - Sign function (returns -1, 0, or 1)
    * `Math.imul(a, b)` - 32-bit integer multiplication
    * `Math.fround(x)` - Round to float32 (no-op on PS2, already float32)
  * **Advanced float operations:**
    * `Math.fma(a, b, c)` - Fused multiply-add: `a*b+c` (MADD.S instruction)
    * `Math.clamp(value, min, max)` - Clamp value to range
    * `Math.lerp(a, b, t)` - Linear interpolation: `a + (b-a)*t`
    * `Math.saturate(x)` - Clamp to [0, 1]
    * `Math.step(edge, x)` - Step function: `x >= edge ? 1 : 0`
    * `Math.smoothstep(e0, e1, x)` - Smooth interpolation (3rd order Hermite)
    * `Math.rsqrt(x)` - Reciprocal square root: `1/sqrt(x)`
* ✅ **String operations:**
  * String concatenation: `str1 + str2`
  * String comparison: `==`, `!=`
  * String passed as arguments and returned
* ✅ Typed array access: `array[i]`, `array[i] = value`
* ✅ Struct field access: `obj.x`, `obj.position[0]`
* ✅ Local variables and function parameters

**What's NOT supported:**
* ❌ Closures and capturing outer variables
* ❌ Calling JavaScript functions (only Math.* intrinsics and native functions)
* ❌ Dynamic typing (all types must be declared)
* ❌ Object/array creation inside native code (use passed arguments)
* ❌ Exceptions (try/catch)
* ❌ Regular expressions

**Calling external functions:**

You can call registered C functions using callbacks, but NOT JavaScript functions:

```js
// ❌ This WON'T work - can't call JS functions from native code
const helperJS = (x) => x * 2;
const bad = Native.compile({args: ['int'], returns: 'int'}, 
    (x) => helperJS(x)); // ERROR!

// ✅ This WORKS - Math functions are intrinsics
const good = Native.compile({args: ['float'], returns: 'float'},
    (x) => Math.sqrt(x * x + 1.0f));
```

**Control flow example:**
```js
const clamp = Native.compile({
    args: ['float', 'float', 'float'],
    returns: 'float'
}, (value, min, max) => {
    if (value < min) return min;
    if (value > max) return max;
    return value;
});

console.log(clamp(5.0f, 0.0f, 10.0f)); // 5.0
console.log(clamp(-1.0f, 0.0f, 10.0f)); // 0.0
console.log(clamp(15.0f, 0.0f, 10.0f)); // 10.0
```

**Loop example:**
```js
const dotProduct = Native.compile({
    args: ['Float32Array', 'Float32Array', 'int'],
    returns: 'float'
}, (a, b, length) => {
    let sum = 0.0f;
    for (let i = 0; i < length; i++) {
        sum += a[i] * b[i];
    }
    return sum;
});

const v1 = new Float32Array([1.0, 2.0, 3.0]);
const v2 = new Float32Array([4.0, 5.0, 6.0]);
console.log(dotProduct(v1, v2, 3)); // 32.0
```

**String example:**
```js
const greet = Native.compile({
    args: ['string', 'string'],
    returns: 'string'
}, (firstName, lastName) => {
    return firstName + " " + lastName;
});

console.log(greet("Athena", "Env")); // "Athena Env"
```

#### Native.struct()

Defines custom struct types with native performance and C-compatible memory layout.

**Construction:**
```js
const Vec3 = Native.struct('Vec3', {
    x: 'float',
    y: 'float',
    z: 'float'
}, {
    // Methods are compiled with 'self' as first argument
    length: Native.compile({
        args: ['self'],
        returns: 'float'
    }, (self) => {
        return Math.sqrt(self.x * self.x + 
                        self.y * self.y + 
                        self.z * self.z);
    }),
    
    scale: Native.compile({
        args: ['self', 'float'],
        returns: 'void'
    }, (self, factor) => {
        self.x *= factor;
        self.y *= factor;
        self.z *= factor;
    }),
    
    // Can also call other methods!
    normalize: Native.compile({
        args: ['self'],
        returns: 'void'
    }, (self) => {
        const len = Math.sqrt(self.x * self.x + 
                             self.y * self.y + 
                             self.z * self.z);
        if (len > 0.0f) {
            self.x /= len;
            self.y /= len;
            self.z /= len;
        }
    })
});
```

**Usage:**
```js
const v = new Vec3();
v.x = 3.0f;
v.y = 4.0f; 
v.z = 0.0f;
console.log(v.length()); // 5.0
v.scale(2.0f);
console.log(v.x); // 6.0
v.normalize();
console.log(v.x); // 0.6 (normalized)
```

**Array fields in structs:**
```js
const Transform = Native.struct('Transform', {
    position: {type: 'float', length: 3},  // float[3]
    rotation: {type: 'float', length: 4},  // float[4] (quaternion)
    scale: {type: 'float', length: 3}      // float[3]
});

const t = new Transform();
t.position[0] = 1.0f;
t.position[1] = 2.0f;
t.position[2] = 3.0f;
t.scale[0] = t.scale[1] = t.scale[2] = 1.0f;
```

#### Utility Functions

* `Native.free(func)` - Free compiled function immediately (optional, GC will handle it)
* `Native.getInfo(func)` - Get metadata: `{codeSize, argCount, returnType}`
* `Native.benchmark(func, iterations)` - Measure execution time in milliseconds
* `Native.disassemble(func)` - Get MIPS assembly listing (for debugging)

**Benchmarking example:**
```js
const func = Native.compile({args: ['int', 'int'], returns: 'int'},
    (a, b) => a * a + b * b);

const iterations = 1000000;
const timeMs = Native.benchmark(func, iterations);
const opsPerSec = (iterations / (timeMs / 1000)).toFixed(0);
console.log(`Performance: ${opsPerSec} ops/sec`);
```

**When to use Native.compile():**
* ✅ **Math-heavy loops** - Physics simulations, audio DSP, signal processing
* ✅ **Vector/matrix math** - 3D graphics, transformations
* ✅ **Array processing** - Image filters, particle updates
* ✅ **String operations** - Concatenation, comparison (basic operations)
* ✅ **Hot paths** - Any function called thousands of times per frame
* ❌ **Complex string parsing** - Regex, advanced manipulation
* ❌ **Complex logic** - If you need dynamic dispatch or closures
* ❌ **One-time calculations** - Compilation overhead not worth it

**Optimization tips:**
* Use `float` (with `f` suffix: `1.0f`) instead of double for 2x faster FPU ops
* Pass arrays instead of individual elements for batch processing
* Structs are passed by pointer (fast), primitives by value
* The compiler applies 5 optimization passes automatically

**Limitations:**
* Maximum 8 argument registers (use structs/arrays for more)
* Maximum expression depth: 8 levels
* No recursion support (stack frames are optimized away)
* No dynamic memory allocation inside native functions

**Technical details:** See `docs/NATIVE_COMPILER.md` for full IR opcodes, MIPS calling conventions, and implementation details.

**How to run it**

Athena is basically a JavaScript loader, so it loads .js files. It runs "main.js" by default, but you can run other file names by changing "athena.ini" default script or passing it as the first argument when launching the ELF file.

**Error reporting system**

Athena has a consistent error system, has two levels. 

**Runtime error:**  
A runtime error occurs on JavaScript side, it can be rich in debug data and easily fixable. It is capable of pointing the error type, custom message, files, lines and it even has
a color code.  
  
<img src="https://user-images.githubusercontent.com/47725160/230756861-94df60f8-8550-43a3-ac43-56d150e94145.png" width=50% height=50%>

**Core error:**  
A core error happens when AthenaEnv is interrupted due to a critical exception (null pointer, invalid address, bus error, overflow etc). It is a lot harder to debug and require low-level programming skills to figure out, from that screen you can get the exception cause, processor register dump, the bad address (if it is related with addresses), return function name(+offset) and the function that caused the error(+offset). It is highly recommended to contact the author to get proper support.  
  
<img src="https://github.com/user-attachments/assets/25da3350-cd53-4302-bf5e-3b7fdebcb4bb" width=50% height=50%>

## Functions, classes and consts

Below is the list of usable functions of AthenaEnv project currently, this list is constantly being updated.

P.S.: *Italic* parameters refer to optional parameters

### std module
The std module provides wrappers to the libc stdlib.h and stdio.h and a few other utilities.

**Available exports:**

* std.evalScript(str, options = undefined) - Evaluate the string str as a script (global eval). options is an optional object containing the following optional properties:
  • std.backtrace_barrier - Boolean (default = false). If true, error backtraces do not list the stack frames below the evalScript.
* std.loadScript(filename) - Evaluate the file filename as a script (global eval).
* let hasfile = std.exists(filename) - Returns a bool that determines whether the file exists or not.
* let fstr = std.loadFile(filename) - Load the file filename and return it as a string assuming UTF-8 encoding. Return null in case of I/O error.
* let file = std.open(filename, flags, errorObj = undefined) - Open a file (wrapper to the libc fopen()). Return the FILE object or null in case of I/O error. If errorObj is not undefined, set its errno property to the error code or to 0 if no error occured.
* std.fdopen(fd, flags, errorObj = undefined) - Open a file from a file handle (wrapper to the libc fdopen()). Return the FILE object or null in case of I/O error. If errorObj is not undefined, set its errno property to the error code or to 0 if no error occured.
* std.tmpfile(errorObj = undefined) - Open a temporary file. Return the FILE object or null in case of I/O error. If errorObj is not undefined, set its errno property to the error code or to 0 if no error occured.
* std.puts(str) - Equivalent to std.out.puts(str).
* std.printf(fmt, ...args) - Equivalent to std.out.printf(fmt, ...args).
* std.sprintf(fmt, ...args) - Equivalent to the libc sprintf().
* std.in, std.out, std.err - Wrappers to the libc file stdin, stdout, stderr.
* std.SEEK_SET, std.SEEK_CUR, std.SEEK_END - Constants for seek().

**Enumeration object containing the integer value of common errors (additional error codes may be defined):**

* std.EINVAL
* std.EIO
* std.EACCES
* std.EEXIST
* std.ENOSPC
* std.ENOSYS
* std.EBUSY
* std.ENOENT
* std.EPERM
* std.EPIPE
  
* std.strerror(errno) - Return a string that describes the error errno.
* std.gc() - Manually invoke the cycle removal algorithm. The cycle removal algorithm is automatically started when needed, so this function is useful in case of specific memory constraints or for testing.
* std.parseExtJSON(str) - Parse str using a superset of JSON.parse. The following extensions are accepted:  
  • Single line and multiline comments  
  • unquoted properties (ASCII-only Javascript identifiers)  
  • trailing comma in array and object definitions  
  • single quoted strings  
  • \f and \v are accepted as space characters  
  • leading plus in numbers  
  • octal (0o prefix) and hexadecimal (0x prefix) numbers  
* std.reload(str) - Reloads all the Javascript environment (stack, modules etc) with the specified script
  
**FILE prototype:**

**Construction:**  

* let file = std.open(filename, flags);  
  filename - Path to the file, E.g.: "samples/test.txt".  
  flags - File mode, E.g.: "w", "r", "wb", "rb", etc.
```js
let file = std.open("test.txt", "w");
``` 
  
* close() - Close the file. Return 0 if OK or -errno in case of I/O error.
* puts(str) - Outputs the string with the UTF-8 encoding.
* printf(fmt, ...args) - Formatted printf.
  • The same formats as the standard C library printf are supported. Integer format types (e.g. %d) truncate the Numbers or BigInts to 32 bits. Use the l modifier (e.g. %ld) to truncate to 64 bits.

* flush() - Flush the buffered file.
* seek(offset, whence) - Seek to a give file position (whence is std.SEEK_*). offset can be a number or a bigint. Return 0 if OK or -errno in case of I/O error.
* tell() - Return the current file position.
* tello() - Return the current file position as a bigint.
* eof() - Return true if end of file.
* fileno() - Return the associated OS handle.
* error() - Return true if there was an error.
* clearerr() - Clear the error indication.
* read(buffer, position, length) - Read length bytes from the file to the ArrayBuffer buffer at byte position position (wrapper to the libc fread).
* write(buffer, position, length) - Write length bytes to the file from the ArrayBuffer buffer at byte position position (wrapper to the libc fwrite).
* getline() - Return the next line from the file, assuming UTF-8 encoding, excluding the trailing line feed.
* readAsString(max_size = undefined) - Read max_size bytes from the file and return them as a string assuming UTF-8 encoding. If max_size is not present, the file is read up its end.
* getByte() - Return the next byte from the file. Return -1 if the end of file is reached.
* putByte(c) - Write one byte to the file.

### os module
The os module provides Operating System specific functions:

* low level file access
* signals
* timers
* asynchronous I/O
The OS functions usually return 0 if OK or an OS specific negative error code.

**Available exports:**

* let fd = os.open(filename, flags, mode = 0o666) - Open a file. Return a handle or < 0 if error.
Flags:  
  • os.O_RDONLY  
  • os.O_WRONLY  
  • os.O_RDWR  
  • os.O_APPEND  
  • os.O_CREAT  
  • os.O_EXCL  
  • os.O_TRUNC  
POSIX open flags.

* os.close(fd) - Close the file handle fd.
* os.seek(fd, offset, whence) - Seek in the file. Use std.SEEK_* for whence. offset is either a number or a bigint. If offset is a bigint, a bigint is returned too.
* os.read(fd, buffer, offset, length) - Read length bytes from the file handle fd to the ArrayBuffer buffer at byte position offset. Return the number of read bytes or < 0 if error.
* os.write(fd, buffer, offset, length) - Write length bytes to the file handle fd from the ArrayBuffer buffer at byte position offset. Return the number of written bytes or < 0 if error.
* os.remove(filename) - Remove a file. Return 0 if OK or -errno.
* os.rename(oldname, newname) - Rename a file. Return 0 if OK or -errno.
* os.realpath(path) - Return [str, err] where str is the canonicalized absolute pathname of path and err the error code.
* os.getcwd() - Return [str, err] where str is the current working directory and err the error code.
* os.chdir(path) - Change the current directory. Return 0 if OK or -errno.
* os.mkdir(path, mode = 0o777) - Create a directory at path. Return 0 if OK or -errno.
* os.stat(path), os.lstat(path) - Return [obj, err] where obj is an object containing the file status of path. err is the error code. The following fields are defined in obj: dev, ino, mode, nlink, uid, gid, rdev, size, blocks, atime, mtime, ctime. The times are specified in milliseconds since 1970. lstat() is the same as stat() excepts that it returns information about the link itself.  
Constants to interpret the mode property returned by stat(). They have the same value as in the C system header sys/stat.h.  
  • os.S_IFMT  
  • os.S_IFIFO  
  • os.S_IFCHR  
  • os.S_IFDIR  
  • os.S_IFBLK  
  • os.S_IFREG  
  • os.S_IFSOCK  
  • os.S_IFLNK  
  • os.S_ISGID  
  • os.S_ISUID  
  
* os.utimes(path, atime, mtime) - Change the access and modification times of the file path. The times are specified in milliseconds since 1970. Return 0 if OK or -errno.
* os.readdir(path) - Return [array, err] where array is an array of strings containing the filenames of the directory path. err is the error code.
* os.setReadHandler(fd, func) - Add a read handler to the file handle fd. func is called each time there is data pending for fd. A single read handler per file handle is supported. Use func = null to remove the handler.
* os.setWriteHandler(fd, func) - Add a write handler to the file handle fd. func is called each time data can be written to fd. A single write handler per file handle is supported. Use func = null to remove the handler.
* os.sleep(delay_ms) - Sleep during delay_ms milliseconds.
* os.setTimeout(func, delay) - Call the function func after delay ms. Return a handle to the timer.
* os.setInterval(func, interval) - Call the function func at specified intervals (in milliseconds). Return a handle to the timer.
* os.setImmediate(func)	- Executes a given function immediately.
* os.clearTimeout(handle) - Cancel a timer.
* os.clearInterval(handle) - Cancel a interval.
* os.clearImmediate(handle) - Cancel a immediate execution.
* os.platform - Return a string representing the platform: "ps2".

### Vector4 Module  

**Construction:**  

* let vec = new Vector4(x, y, z, w);  
```js
let test = new Vector4(0.0, 0.0, 0.0, 1.0); 
``` 

**Properties:**

* x, y, z, w - Vector components.

Methods:

* norm() - Get vector length/norm.
* dot(vec2) - Calculate vector dot product.
* cross(vec2) - Calculate vector cross product.
* distance(vec2) - Calculate vector distances.
* distance2(vec2) - Calculate vector squared distances.

**Operators:** 

* add: +
* sub: -
* mul: *
* div: /
* eq: ==


### Matrix4 Module  

**Construction:**  

* let mat = new Matrix4(m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16)  
or
* let mat = new Matrix4() - No arguments means identity matrix.
```js
let test = new Matrix4(); 
``` 

**Properties:**

* length - 16.

**Methods:**

* toArray() - Returns an array from the matrix.
* fromArray() - Puts the first 16 numbers of the array as the matrix elements.
* clone() - Create a new clone of the given matrix.
* copy(mat) - Copy the matrix contents to the given matrix.
* transpose() - Transpose matrix.
* invert() - Calculate matrix inverse.
* identity() - Put matrix identity.

**Operators:** 

* mul: *
* eq: ==

P.S.: Matrix4 module components can be accessed as an array, so... mat[n] from 0 to 15.
    
### Color module
* let col = Color.new(r, g, b, *a*) - Returns a color object from the specified RGB(A) parameters.
* let r = Color.getR(col) - Get red intensity of the color.
* let g = Color.getG(col) - Get green intensity of the color.
* let b = Color.getB(col) - Get blue intensity of the color.
* let a = Color.getA(col) - Get alpha intensity of the color.
* Color.setR(col, r) - Set red intensity of the color.
* Color.setG(col, g) - Set green intensity of the color.
* Color.setB(col, b) - Set blue intensity of the color.
* Color.setA(col, a) - Set alpha intensity of the color.

### Image Module  
**Functions:**  
* Image.copyVRAMBlock(src_buffer, src_x, src_y, dest_buffer, dest_x, dest_y) - Copies a VRAM block from src_buffer to dest_buffer.
  
**Construction:**  

* let image = new Image(*path*, *async_list*);  
  *path* - Path to the file, E.g.: "images/test.png".  
  *async_list* - Gets a ImageList object, which is a asynchronous image loading list, if you want to load images in the background.  
```js
let test = new Image("owl.png"); 
``` 

**Properties:**

* width, height - Image drawing size, default value is the original image size.
* startx, starty - Beginning of the area that will be drawn from the image, the default value is 0.0.
* endx, endy - End of the area that will be drawn from the image, the default value is the original image size.
* angle - Define image rotation angle, default value is 0.0.
* color - Define image tinting, default value is Color.new(255, 255, 255, 128).
* filter - Choose between **LINEAR** or **NEAREST**, default value is NEAREST.
* size - Returns image real size occupied in memory.
* bpp - Bits per-pixel quantity.
* pixels - The image pixel ArrayBuffer.
* palette - If is a palette image, it has a palette ArrayBuffer right here.
* texWidth, texHeight - The real texture area in memory.
* renderable - Set if the texture can be used as a rendering buffer.

**Methods:**

* draw(x, y) - Draw loaded image onscreen(call it every frame). Example: image.draw(15.0, 100.0);
* optimize() - If your image has 24 bits per-pixel (aka RGB), you can use this to make it 16 bits per-pixel, saving some memory!
* lock() - Lock texture data in VRAM, useful when you deal with textures that are always needed or rendering buffers.
* unlock() - Unlock texture data in VRAM, so it can be replaced with more used textures.
* locked() - Returns true if the texture is locked in VRAM.
* ready() - Returns true if an asynchronous image was successfully loaded in memory. 
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.
  
**ImageList**

**Construction:**

```js
let async_list = new ImageList(); // This constructor creates a new thread and a queue to load images in background, avoid building multiple ImageList objects.
```
**Methods:**

* process() - This method starts the thread and loads added images on the queue. 
```js
async_list.process();
```
  
  
### Draw module
* Draw.point(x, y, color) - Draws a pixel on the specified color and position on the screen.
* Draw.rect(x, y, width, height, color) - Draws a rectangle on the specified color, position and size on the screen.
* Draw.line(x, y, x2, y2, color) - Draws a line on the specified colors and position on the screen.
* Draw.circle(x, y, radius, color, *filled*) - Draws a circle on the specified color, position, radius and fill on the screen.
* Draw.triangle(x, y, x2, y2, x3, y3, color, *color2*, *color3*) - Draws a triangle on the specified points positions and colors on the screen.
* Draw.quad(x, y, x2, y2, x3, y3, x4, y4 color, *color2*, *color3*, *color4*) - Draws a quad on the specified points positions and colors on the screen.

### TileMap module
The TileMap module exposes the descriptor/buffer-based API described in [docs/TILEMAP.md](docs/TILEMAP.md). It is split into a namespace with constructors and helpers:

**Available exports:**

* `TileMap.Descriptor(config)` – Create an immutable descriptor (textures array or `Image` objects, plus `materials` list with `texture_index`, `blend_mode`, `end_offset`).
* `TileMap.Instance({ descriptor, spriteBuffer })` – Bind a sprite buffer (`ArrayBuffer`) to a descriptor. Methods:
  • `render(x, y[, zindex])` – Draw at the specified offset.  
  • `replaceSpriteBuffer(buffer)` – Swap the current buffer pointer.  
  • `getSpriteBuffer()` – Returns the attached `ArrayBuffer` for direct mutation.  
  • `updateSprites(dstOffset, srcBuffer[, spriteCount])` – Copy a range of sprites from another buffer.
* `TileMap.SpriteBuffer.create(count)` – Allocate a zeroed buffer sized for `count` sprites.
* `TileMap.SpriteBuffer.fromObjects(array)` – Convert an array of JS objects (`x`, `y`, `w`, `h`, `u1`, `v1`, …) into a packed buffer.
* `TileMap.layout` – `{ stride, offsets }` helper for DataView/TypedArray math (fields: `x`, `y`, `w`, `h`, `u1`, `v1`, `u2`, `v2`, `r`, `g`, `b`, `a`, `zindex`).
* `TileMap.init()` / `TileMap.begin()` / `TileMap.setCamera(x, y)` – Low-level hooks to initialize the renderer, start a frame, and move the shared camera offset.

### RenderBatch (RenderBatch Module)

Construction:

* let batch = new Batch(options);  
  options.autoSort - Boolean (default: true). Enables internal state-aware sort.

Methods:

* add(renderObject) - Adds a RenderObject to the batch. Returns current count.
* clear() - Removes all objects from the batch.
* render() - Renders all objects in the batch using optimal state transitions. Returns number of draws.

Properties:

* size (read-only) - Number of objects currently in the batch.

Example:
```js
const batch = new Batch({ autoSort: true });
batch.add(new RenderObject(new RenderData("dragon.obj")));
batch.render();
```

### SceneNode (RenderSceneNode Module)

Construction:

* let node = new SceneNode();

Methods:

* addChild(node) - Parents a child SceneNode.
* removeChild(node) - Unparents a specific child.
* attach(renderObject) - Attaches a RenderObject to this node.
* detach([renderObject]) - Detaches the specific object or all when omitted.
* update() - Recomputes world transforms (rotate → scale → translate) for this subtree.

Properties:

* position { x, y, z }
* rotation { x, y, z }
* scale { x, y, z }

Example:
```js
const root = new SceneNode();
const child = new SceneNode();
const obj = new RenderObject(new RenderData("monkey.obj"));
child.position = { x: 0, y: 2, z: 0 };
child.attach(obj);
root.addChild(child);
root.update();
```

### AsyncLoader (RenderAsyncLoader Module)

Cooperative streaming helper executed on the main thread in small steps.

Construction:

* let loader = new AsyncLoader({ jobsPerStep });  
  jobsPerStep - Number (default: 1). How many items to process per process() call when budget is omitted.

Methods:

* enqueue(path, callback[, texture]) - Queues a model load.  
  - path: String (e.g., "BoxTextured.gltf").  
  - callback: Function (path, renderData) invoked when the item is ready.  
  - texture: optional Image to bind during load (may be null/omitted).
* process([budget]) - Processes up to budget items from the queue (or jobsPerStep if omitted). Returns number processed.
* clear() - Clears pending items and releases their callbacks.
* destroy() - Clears and destroys the loader.
* size() - Returns number of items still pending.
* getJobsPerStep() - Returns current jobs-per-step.
* setJobsPerStep(n) - Sets jobs-per-step, clamped to at least 1. Returns the applied value.

Example:
```js
const loader = new AsyncLoader({ jobsPerStep: 2 });
loader.enqueue("BoxTextured.gltf", (path, data) => {
  const obj = new RenderObject(data);
  sceneBatch.add(obj);
});

// In your frame loop
loader.process();
```

### Render module

Always configure the screen z-buffer before rendering 3D:
```js
const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psmz = Screen.Z16S;
Screen.setMode(canvas);
```

* `Render.init()` – Initializes internal renderer state (GS/VU microprograms, materials cache). Call once during boot.
* `Render.begin()` – Starts a render pass and resets batched state. Call once per frame before issuing draw calls.
* `Render.setView(fov = 60, nearClip = 1.0, farClip = 2000.0, width = 0.0, height = 0.0)` – Configures the default projection matrix. Width/height override the auto-derived aspect ratio when non-zero.
* `Render.materialColor(r, g, b, alpha = 1.0)` – Convenience helper that returns a `{r,g,b,a}` color object for materials.
* `Render.material(ambient, diffuse, specular, emission, transmittance, shininess, refraction, transmission_filter, dissolve, texture_id, bump_texture_id, ref_texture_id, decal_texture_id)` – Builds a material descriptor used by RenderData/RenderObject. Texture ids accept `-1` to disable a layer.
* `Render.materialIndex(index, end)` – Tags the vertex/material arrays so the renderer knows which faces should use each material slice.
* `Render.vertexList(positions, normals, texcoords, colors, materials, material_indices)` – Creates the structure expected by `new RenderData(...)`. Each typed array must use 4-component packing (xyzw, n1n2n3w, stqw, rgba).

Constants:
* `Render.PL_NO_LIGHTS`, `Render.PL_DEFAULT`, `Render.PL_SPECULAR` – Select the shading pipeline for RenderData.
* `Render.CULL_FACE_NONE`, `Render.CULL_FACE_BACK`, `Render.CULL_FACE_FRONT` – Face-culling modes consumed by RenderData/RenderObject.

### RenderData module

**Construction:**

```js
let data = new RenderData(mesh, *texture*)
/* Load simple WaveFront OBJ files or vertex arrays.
MTL is supported on OBJs (including per-vertex colors and multi-texturing).
If you don't have a MTL file but you want to bind a texture on it,
just pass the image as a second argument if you want to use it. */
```
**Methods:**  

* getTexture(id) - Gets the nth texture object from the model.
* setTexture(id, texture) - Changes or sets the nth texture on models.
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.
  
**Properties:**

* positions - Float32Array with x, y, z, adc for each vertex.
* normals - Float32Array with n1, n2, n3, adc for each vertex.
* texcoords - Float32Array with s, t, q, w for each vertex.
* colors - Float32Array with r, g, b, a for each vertex.
* pipeline - Rendering pipeline. Avaliable pipelines below:
  • Render.PL_NO_LIGHTS - Lights disabled, colors still working.  
  • Render.PL_SPECULAR - Diffuse and specular lights and colors enabled.  
  • Render.PL_DEFAULT - Default for textured models. Lights and colors enabled.  

* materials - Render.material array.
* material_indices - Render.materialIndex array.
* size - Vertex quantity.
* bounds - Mesh bounding box.

* accurate_clipping - Toggle accurate clipping.
* face_culling - Face culling mode. Default: Render.CULL_FACE_BACK. Avaliable modes below:  
  • Render.CULL_FACE_NONE  
  • Render.CULL_FACE_FRONT  
  • Render.CULL_FACE_BACK  

* texture_mapping - Toggle texture mapping. (it can be used to disable texturing on a whole textured model)
* shade_model - Flat = 0, Gouraud = 1.  
  
**Properties(skinned):**
* bones: Array - Skeleton bones data.  
  • position: Vector4 - Bone local position.  
  • rotation: Vector4 - Bone local rotation.  
  • scale: Vector4 - Bone local scale.  
  • inverse_bind: Matrix4 - Bone inverse bind matrix.  

### AnimCollection module

**Construction:**

```js
let walk_anims = new AnimCollection("walk_anims.gltf");
```
Loads an array map containing the animations inside the file, so you can either load them by index or name, for instance: for a anim called "run_fast", just pass as:
```js
character.playAnim(walk_anims["run_fast"], true);

// now you want it to be in idle animation, which can be the first animation on our array, so:
character.playAnim(walk_anims[0], true);
```
  
### RenderObject module

**Construction:**

```js
let model = new RenderObject(render_data)
/* You need to build a RenderData first, RenderObject keeps with position, rotation   
and the individual object matrices */
```
**Methods:**  

* render() - Draws the object on screen.
* renderBounds() - Draws object bounding box.
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.  
  
**Methods(skinned):**

* playAnim(anim, loop) - Play animation on a skinned RenderObject.
* isPlayingAnim(anim) -  Returns true if anim is being played.  

**Properties:**  

* position - Object with x, y and z keys that stores the object position. Default is {x:0, y:0, z:0}.
* rotation - Object with x, y and z keys that stores the object rotation. Default is {x:0, y:0, z:0}.
* scale - Object with x, y and z keys that stores the object scale. Default is {x:0, y:0, z:0}.
* transform: Matrix4 - Object RTS transform matrix.  

**Properties(skinned):**
* bone_matrices: Matrix4[] - Array of Matrix4 containing the current bone state.
* bones: Array - Current bones current data.  
  • position: Vector4 - Current bone local position.  
  • rotation: Vector4 - Current bone local rotation.  
  • scale: Vector4 - Current bone local scale.  
  • transform: Matrix4 - Current bone RTS matrix.  
  
**Camera**   
* Camera.position(x, y, z)  
* Camera.rotation(x, y, z)  
* Camera.target(x, y, z)  
* Camera.orbit(yaw, pitch)  
* Camera.turn(yaw, pitch)  
* Camera.pan(x, y)  
* Camera.dolly(distance)  
* Camera.zoom(distance)  
  
* Camera.update() - Update camera state (must be called every frame).  
  
**Lights**  
You have 4 lights to use in 3D scenes, use set to configure them.

* Lights.set(id, attribute, x, y, z)  
  • Avaliable light attributes: Lights.DIRECTION, Lights.AMBIENT, Lights.DIFFUSE    
  
### Screen module
* `Screen.display(loopFn)` – Runs `loopFn` every frame with automatic clear/flip (ideal for quick demos).
* `Screen.clearColor(color)` – Persists a clear color used by `Screen.display` and as the default for `Screen.clear()` with no args.
* `Screen.clear(color = Color.new(0,0,0,0))` – Clears the current draw buffer.
* `Screen.flip()` – Submits all queued draw packets and swaps the display buffers.
* `Screen.getMemoryStats(statId = Screen.VRAM_USED_TOTAL)` – Returns VRAM usage in bytes. Other counters: `VRAM_SIZE`, `VRAM_USED_STATIC`, `VRAM_USED_DYNAMIC`.
* `Screen.setVSync(enabled)` – Enables/disables VSync (locks FPS to the mode’s refresh rate).
* `Screen.setFrameCounter(enabled)` – Enables internal FPS counter required by `Screen.getFPS()`.
* `Screen.waitVblankStart()` – Blocks until the next vertical blank.
* `Screen.getFPS(frameIntervalMs = 1000)` – Returns the measured FPS over the given window (requires frame counter enabled).
* `Screen.getMode()` – Returns the active video mode object: `{ mode, width, height, psm, interlace, field, double_buffering, zbuffering, psmz, pass_count }`.
* `Screen.setMode(canvas)` – Applies a video mode previously fetched/edited via `Screen.getMode()`.
* `Screen.initBuffers()` – Allocates internal draw/display/depth buffers (required for off-screen rendering APIs).
* `Screen.resetBuffers()` – Restores the buffers created by `initBuffers()`.
* `Screen.getBuffer(bufferId)` – Returns the GS surface bound to `DRAW_BUFFER`, `DISPLAY_BUFFER`, or `DEPTH_BUFFER`.
* `Screen.setBuffer(bufferId, image[, mask = 0])` – Replaces a buffer with a renderable `Image`. Call `Screen.initBuffers()` before using this function.
* `Screen.switchContext()` – Toggles between the two GS drawing contexts and returns the active context id.
* `Screen.flush()` – Forces pending GIF packets to be flushed immediately.
* `Screen.getParam(param)` / `Screen.setParam(param, value)` – Low-level access to GS render state (alpha/depth/scissor/etc.). Pass either raw integers or helper objects (see below).
* `Screen.alphaEquation(a, b, c, d, fix)` – Utility that packs the GS alpha-blend formula into a 64-bit value suitable for `Screen.setParam(Screen.ALPHA_BLEND_EQUATION, ...)`.

**Parameters below:**  
  
* Screen.ALPHA_BLEND_EQUATION - Set alpha blending mode based on a fixed coefficient equation.  
  • Screen.SRC_RGB - Source RGB  
  • Screen.DST_RGB - Framebuffer RGB  
  • Screen.ZERO_RGB - Zero RGB  
  • Screen.SRC_ALPHA - Source alpha  
  • Screen.DST_ALPHA - Framebuffer alpha  
  • Screen.ALPHA_FIX - C = Fix argument  
  
```The GS's alpha blending formula is fixed but it contains four variables that can be reconfigured:
Output = (((A - B) * C) >> 7) + D
A, B, and D are colors and C is an alpha value. Their specific values come from the ALPHA register:
      A                B                C                   D
  0   Source RGB       Source RGB       Source alpha        Source RGB
  1   Framebuffer RGB  Framebuffer RGB  Framebuffer alpha   Framebuffer RGB
  2   Zero RGB         Zero RGB         Fix argument        Zero RGB
```
So pass it as an object like:
```js
const NORMAL_BLEND = {
  a: Screen.SRC_RGB, 
  b: Screen.DST_RGB, 
  c: Screen.SRC_ALPHA, 
  d: Screen.DST_RGB, 
  fix: 0
};

Screen.setParam(Screen.ALPHA_BLEND_EQUATION, NORMAL_BLEND);
```
* Screen.ALPHA_TEST_ENABLE: bool - Enable or disable alpha test.
* Screen.ALPHA_TEST_METHOD - Set alpha test method.  
  • Screen.ALPHA_NEVER - All pixels fail  
  • Screen.ALPHA_ALWAYS - All pixels pass  
  • Screen.ALPHA_LESS - pixel alpha < ALPHA_TEST_REF passes  
  • Screen.ALPHA_LEQUAL - pixel alpha <= ALPHA_TEST_REF passes  
  • Screen.ALPHA_EQUAL - pixel alpha == ALPHA_TEST_REF passes  
  • Screen.ALPHA_GEQUAL - pixel alpha >= ALPHA_TEST_REF passes  
  • Screen.ALPHA_GREATER - pixel alpha > ALPHA_TEST_REF passes  
  • Screen.ALPHA_NEQUAL - pixel alpha != ALPHA_TEST_REF passes  
* Screen.ALPHA_TEST_REF: int - Alpha test reference, normally it is 128 (0x80).
* Screen.ALPHA_TEST_FAIL - Set behavior when alpha test fails  
  • Screen.ALPHA_FAIL_NO_UPDATE - Neither frame buffer nor depth buffer are updated.  
  • Screen.ALPHA_FAIL_FB_ONLY - Only frame buffer is updated.  
  • Screen.ALPHA_FAIL_ZB_ONLY - Only depth buffer is updated.  
  • Screen.ALPHA_FAIL_RGB_ONLY - Only RGB in framebuffer is updated.  
* Screen.DST_ALPHA_TEST_ENABLE: bool - Enable or disable destination alpha test.
* Screen.DST_ALPHA_TEST_METHOD - Destination alpha test method.  
  • Screen.DST_ALPHA_ZERO - Destination alpha bit == 0 passes  
  • Screen.DST_ALPHA_ONE - Destination alpha bit == 1 passes  
The alpha bit tested depends on the framebuffer format. If the format is CT32, bit 7 of alpha is tested. If the format is CT16/S, the sole alpha bit is tested. If the format is CT24, all pixels pass due to the lack of alpha.  
* Screen.DEPTH_TEST_ENABLE: bool - Enable or disable depth test.  
* Screen.DEPTH_TEST_METHOD - Depth test method.  
  • Screen.DEPTH_NEVER - All pixels fail  
  • Screen.DEPTH_ALWAYS - All pixels pass  
  • Screen.DEPTH_GEQUAL - pixel Z >= depth buffer Z passes  
  • Screen.DEPTH_GREATER - pixel Z > depth buffer Z passes  
* Screen.PIXEL_ALPHA_BLEND_ENABLE: bool - If enabled, switch per-pixel alpha blending based on source alpha most significant bit value.
* Screen.COLOR_CLAMP_MODE: bool - If true, RGB components will be 0 if negative after alpha-blending or 0xFF if 0x100 or above. else, each color component will be ANDed with 0xFF, ie, they overlap.
* Screen.SCISSOR_BOUNDS - Object model: {x1: x_start, y1: y_start, x2: x_end, y2: y_end}

### Font module

**Construction:**  
```js
let font = new Font(path);  // It supports png, bmp, jpg, otf, ttf.
```
  path - Path to a font file, E.g.: "images/atlas.png", "fonts/font.png".  
```js
let font = new Font("Segoe UI.ttf"); //Load trueType font 
``` 

**Properties:**
* color - Define font tinting, default value is Color.new(255, 255, 255, 128).
* scale - Proportional scale, default: 1.0f
* outline_color - Define outline tinting, default value is Color.new(0, 0, 0, 128).
* outline - Outline size, default: 0.0f
* dropshadow_color - Define dropshadow tinting, default value is Color.new(0, 0, 0, 128).
* dropshadow - Shadow drop position, default: 0.0f
* align - Font alignment, default value is FontAlign.NONE. Avaliable options below:  
  • Font.ALIGN_NONE  
  • Font.ALIGN_TOP  
  • Font.ALIGN_BOTTOM  
  • Font.ALIGN_LEFT  
  • Font.ALIGN_RIGHT  
  • Font.ALIGN_VCENTER  
  • Font.ALIGN_HCENTER  
  • Font.ALIGN_CENTER  
  

P.S.: outline and drop shadow do not coexist, so one of them must be 0.0f.

**Methods:**
* print(x, y, text) - Draw text on screen(call it every frame). Example: font.print(10.0, 10.0, "Hello world!);
* getTextSize(text) - Returns text absolute size in pixels (width, height). Example: const size = font.getTextSize("Hello world!");

### Pads module

* Buttons list:  
  • Pads.SELECT  
  • Pads.START  
  • Pads.UP  
  • Pads.RIGHT  
  • Pads.DOWN  
  • Pads.LEFT  
  • Pads.TRIANGLE  
  • Pads.CIRCLE  
  • Pads.CROSS  
  • Pads.SQUARE  
  • Pads.L1  
  • Pads.R1  
  • Pads.L2  
  • Pads.R2  
  • Pads.L3  
  • Pads.R3  

* let pad = Pads.get(*port*) - Returns a pad object:  
**Properties:**  
  • pad.btns - Button state on the current check.  
  • pad.old_btns = Button state on the last check.  
  • pad.lx - Left analog horizontal position (left = -127, default = 0, right = 128).  
  • pad.ly - Left analog vertical position (up = -127, default = 0, down = 128).  
  • pad.rx - Right analog horizontal position (left = -127, default = 0, right = 128).  
  • pad.ry - Right analog vertical position (up = -127, default = 0, down = 128).  
    
  ![analog_graph](https://user-images.githubusercontent.com/47725160/154816009-99d7e5da-badf-409b-9a3b-3618fd372f09.png)  
  
**Methods:**  
  • update() - Updates all pads pressed and stick positions data.  
  • pressed(button) - Checks if a button is being pressed (continuously).  
  • justPressed(button) - Checks if a button was pressed only once.  
  • setEventHandler() - Sets the pad object to listen events defined by Pads.newEvent, so it doesn't need to be updated.  
  
* let event_id = Pads.newEvent(button, kind, function) - Creates an asynchronous pad event, returns the event id. Remember to set the pad object event handler first!
* Pad event kinds:  
  • Pads.PRESSED  
  • Pads.JUST_PRESSED  
  • Pads.NON_PRESSED  
* Pads.deleteEvent(event_id) - Deletes the event created by Pads.newEvent.
* let type = Pads.getType(*port*) - Gets gamepad type in the specified port.
* Pad Types:  
  • Pads.DIGITAL  
  • Pads.ANALOG  
  • Pads.DUALSHOCK  

* let press = Pads.getPressure(*port*, button) - Get button pressure level.
* Pads.rumble(port, big, small) - Rumble your gamepad.
* let count = Pads.getConnectedCount() – Returns the number of gamepads currently connected.
* let ports = Pads.getConnected() – Returns an array containing the ports of all connected gamepads. Example: [0], [1], or [0, 1].
* let active = Pads.isActive(port) – Checks whether the specified port is active and ready to receive input.
  
### Keyboard module
* Keyboard.init() - Initialize keyboard routines.
* let c = Keyboard.get() - Get keyboard current char.
* Keyboard.setRepeatRate(msec) - Set keyboard repeat rate.
* Keyboard.setBlockingMode(mode) - Sets keyboard to block(or not) the thread waiting for the next key to be pressed.
* Keyboard.deinit() - Destroy keyboard routines.

### Mouse module
* Mouse.init() - Initialize mouse routines.
* let mouse = Mouse.get() - Returns mouse actual properties on the object format below:  
  • mouse.x  
  • mouse.y  
  • mouse.wheel  
  • mouse.buttons  
* Mouse.setBoundary(minx, maxx, miny, maxy) - Set mouse x and y bounds.
* let mode = Mouse.getMode() - Get mouse mode(absolute or relative).
* Mouse.setMode(mode) - Set mouse mode.
* let accel = Mouse.getAccel() - Get mouse acceleration.
* Mouse.setAccel(val) - Set mouse acceleration.
* Mouse.setPosition(x, y) - Set mouse pointer position.
  
### System module
* let ret = System.nativeCall(address, arguments, *return_type*)  
  • address - Memory address of the native function.  
  • arguments - Array of arguments for the function, details below:  
    • Argument model: {type: System.T_X, value: X}, types below:
      • System.T_LONG  
      • System.T_ULONG  
      • System.T_INT  
      • System.T_UINT  
      • System.T_SHORT  
      • System.T_USHORT  
      • System.T_CHAR  
      • System.T_UCHAR  
      • System.T_PTR  
      • System.T_BOOL  
      • System.T_FLOAT  
      • System.T_STRING  
      • System.JS_BUFFER - ArrayBuffer  
  • return_type - Use one of the types above, or simply omit for void.  
* let reloc_id = System.loadReloc(path) - Loads a relocatable code (library or module)
* System.unloadReloc(reloc_id) - Unloads it from memory. P.S.: DANGER!!! Assures that you aren't using code from it.
* let symbol_addr = System.findRelocObject("symbol_name") - Find object inside Athena binary or a relocatable code/data
* let symbol_addr = System.findRelocLocalObject(reloc_id, "symbol_name") - Find object inside a relocatable code/data
* let info = System.getBDMInfo("mass0") - Gets info about the BDM device. E.g.: {name: "usb", index:0}
* let result = System.mount(mountpoint, device, *mode*)
* let result = System.umount(path)
* System.devices() - Array with avaliable devices: {name, desc}
* let listdir = System.listDir(*path*)
  • listdir[index].name - return file name on indicated index(string)  
  • listdir[index].size - return file size on indicated index(integer)  
  • listdir[index].directory - return if indicated index is a file or a directory(bool)  
* System.removeDirectory(path)
* System.copyFile(source, dest)
* System.moveFile(source, dest)
* System.rename(source, dest)
* System.sleep(sec)
* System.exitToBrowser()
* System.setDarkMode(value)
* let temps = System.getTemperature() // It only works with SCPH-500XX and later models.
* let info = System.getMCInfo(slot)  
  • info.type  
  • info.freemem  
  • info.format  
  
* let ee_info = System.getCPUInfo()  
  • ee_info.implementation  
  • ee_info.revision  
  • ee_info.FPUimplementation  
  • ee_info.FPUrevision  
  • ee_info.ICacheSize  
  • ee_info.DCacheSize  
  • ee_info.RAMSize  
  • ee_info.MachineSize  
  
* let gs_info = System.getGPUInfo()  
  • gs_info.id  
  • gs_info.revision  
  
* let ram_usage = System.getMemoryStats()  
  • ram_usage.core - Kernel + Native code size in RAM  
  • ram_usage.nativeStack - Kernel + Native stack size  
  • ram_usage.allocs - Dynamic allocated memory tracking  
  • ram_usage.used - All above, but combined  
  
Asynchronous functions:  
* System.threadCopyFile(source, dest)
* let progress = System.getFileProgress()  
  • progress.current  
  • progress.final  
  
### Mutex module

**Construction:**
```js
const mutex = new Mutex();
```
**Methods:**
* lock() - Acquire the lock.
* unlock() - Release the lock.

### Thread module
**Functions:**
* Thread.list() - List all system threads (OS level, not JavaScript itself). List[Object[id, name, status, stack_size]]
* Thread.kill(id) - Force kill a thread from an internal ID.

**Construction:**
```js
const thread = new Thread(() => console.log("Hello from a thread!"), "Thread: Hello World!"); // Thread name is an optional parameter with 64 characters maximum size, useful to be tracked from Thread.list()
```
**Methods:**
* start() - Set the thread to an active state
* stop() - Finish thread execution, changing it to the state before calling start().

**Properties:**
* id - Thread internal ID
* name - Thread name

### Timer module

* let timer = Timer.new()
* Timer.getTime(timer)
* Timer.setTime(src, value)
* Timer.destroy(timer)
* Timer.pause(timer)
* Timer.resume(timer)
* Timer.reset(timer)
* Timer.isPlaying(timer)

### Sound module

* Sound.setVolume(volume) - Set master volume.
* Sound.findChannel() - Returns the first free channel found to be used on sound effect playback.
* const bgm = Sound.Stream(path) - Loads a audio stream file(WAV, OGG)  
**Methods:**  
  • play() - Play(or resume) audio stream.  
  • free() - Free audio stream from memory.  
  • pause() - Pause audio stream.  
  • playing() - Check if the audio stream is being played.  
  • rewind() - Restart audio to it's beginning (should call play() again if it's not the current track).  
**Properties:**  
  • position - Current track playtime in msec, you can get or change.  
  • length - Current track duration in msec, read-only property.  
  • loop - If the track is played in a loop, you can get or change.  

* const shoot_sfx = Sound.Sfx(path) - Loads a sound effect(ADPCM)  
**Methods:**  
  • play(*channel*) - Play sound effect. P.S.: If channel isn't specified, it will automatically use a free channel(and return the channel index, otherwhise it returns undefined).  
  • free() - Free sound effect from memory.  
  • playing(channel) - Check if the sound effect is being played on the specified channel.  
**Properties:**  
  • volume - Current sound effect volume, you can get or change from 0 to 100.  
  • pan - Sound effect spatial setting, you can get or change from -100(left) to 100(right), 0 is the center.  
  • pitch - Sound effect pitch, you can get or change from -100 to 100, 0 is the default value.

### Video module

**Construction:**

* let video = new Video(path)
  path - Path to the MPEG file, E.g.: "videos/intro.mpg".

**Properties:**

* width (read-only) - Video width in pixels.
* height (read-only) - Video height in pixels.
* fps (read-only) - Video frames per second.
* ready (read-only) - Returns true if the video is loaded and ready for playback.
* ended (read-only) - Returns true if playback has finished.
* playing (read-only) - Returns true if video is currently playing.
* loop - Boolean to enable/disable Looping.
* frame (read-only) - Returns the current frame as an Image object (useful for use as texture).
* currentFrame (read-only) - Returns the current frame index.

**Methods:**

* play() - Start or resume playback.
* pause() - Pause playback.
* stop() - Stop playback and reset to beginning.
* update() - Process video decoding (call it every frame). Returns true if a new frame was decoded.
* draw(x, y, *w*, *h*) - Draw the current video frame to screen.
* free() - Release video resources.


### Shadows module

The Shadows module provides real-time shadow projection capabilities using a grid-based approach with optional ODE ray casting for accurate shadow placement on 3D geometry.

**Constants:**

* Shadows.SHADOW_BLEND_DARKEN - Darken blend mode (default)
* Shadows.SHADOW_BLEND_ALPHA - Alpha blend mode  
* Shadows.SHADOW_BLEND_ADD - Additive blend mode

**Construction:**

```js
let projector = new Shadows.Projector(shadowTexture);
```
Creates a new shadow projector using the specified texture for the shadow appearance.

**Methods:**

* setSize(width, height) - Set the shadow projection area size in world units.
* setGrid(gridX, gridZ) - Set the grid resolution for shadow tessellation (minimum 2x2).
* setLightDir(x, y, z) - Set the directional light direction (automatically normalized).
* setBias(bias) - Set shadow bias to prevent z-fighting (default: 0.01).
* setLightOffset(offset) - Set offset along light direction to shift shadow center.
* setSlopeLimit(maxSlopeCos) - Set maximum slope angle for shadow projection (optional).
* setColor(r, g, b, a) - Set shadow color and alpha (0.0-1.0 range).
* setBlend(mode) - Set shadow blend mode (SHADOW_BLEND_* constants).
* setUVRect(u0, v0, u1, v1) - Set texture UV rectangle for shadow appearance.
* enableRaycast(space, rayLength, enable) - Enable/disable ODE ray casting for accurate shadow placement.
* render() - Render the shadow projector (call every frame).

**Properties:**

* position - Object with x, y, z keys for shadow projector world position.
* rotation - Object with x, y, z keys for shadow projector rotation (quaternion).
* scale - Object with x, y, z keys for shadow projector scale.

**Usage Example:**

```js
// Create shadow render target
const shadowRT = new Image();
shadowRT.filter = LINEAR;
shadowRT.renderable = true;
shadowRT.bpp = 32;
shadowRT.texWidth = 256;
shadowRT.texHeight = 256;
shadowRT.width = 256;
shadowRT.height = 256;
shadowRT.lock();

// Create shadow projector
const projector = new Shadows.Projector(shadowRT);
projector.setSize(2.0, 2.0);
projector.setGrid(12, 12);
projector.setLightDir(0.0, 1.0, 1.0);
projector.setBias(-0.02);
projector.setColor(0.0, 0.0, 0.0, 0.65);
projector.setBlend(Shadows.SHADOW_BLEND_DARKEN);
projector.enableRaycast(space, 4, 12.0);
projector.setLightOffset(1.0);

// In render loop
projector.position = { x: object.x, y: 0.0, z: object.z };
projector.render();
```

**Performance Notes:**

* Higher grid resolution provides smoother shadows but uses more vertices
* Ray casting provides accurate shadows but has performance cost
* Shadow projectors use VU1 pipeline for optimal performance
* Consider using lower resolution for distant or less important shadows  
  
### Archive module

* let zip = Archive.open(fname)
* let list = Archive.list(zip)
* Archive.extractAll(zip)
* Archive.close(zip)
* Archive.untar(fname)

### IOP module

* const module = IOP.newModule(name, data, *arg_len*, *args*) - data can be a string when it is a file or an ArrayBuffer when loading from memory.
* IOP.loadModule(module, arg) - Loads an IOP module created by newModule.
* IOP.reset()
* let stats = IOP.getMemoryStats()  
  • stats.free  
  • stats.used  

### Network module

* Network.init(*ip*, *netmask*, *gateway*, *dns*)  
```js
Network.init("192.168.0.10", "255.255.255.0", "192.168.0.1", "192.168.0.1"); //Static mode  
Network.init(); //DHCP Mode, dynamic.  
```

* let conf = Network.getConfig()  
  Returns conf.ip, conf.netmask, conf.gateway, conf.dns.
  
* Network.deinit()  
  Shutdown network module.  
  
  
### Request module

**Construction:**  

* let r = new Request()  
```js
let r = new Request();
```

**Properties:**

* keepalive - bool
* useragent - string
* userpwd - string
* headers - string array

**Methods:**

* get(url)
* head(url)
* post(url, data)
* download(url, fname)  
  
**Asynchronous methods:**  
* asyncGet(url)
* asyncDownload(url, fname)
* ready(*timeout*, *conn_timeout*)
* getAsyncData()
* getAsyncSize()


### Socket module

**Properties:**

* Socket.AF_INET
* Socket.SOCK_STREAM
* Socket.SOCK_DGRAM
* Socket.SOCK_RAW

**Construction:**  

* let s = new Socket(domain, type)  
```js
let s = new Socket(Socket.AF_INET, Socket.SOCK_STREAM);
```

**Methods:**

* connect(host, port)
* bind(host, port)
* listen()
* send(data) - Send data with Buffer
* recv(size) - Receive data to a buffer
* close()


### WebSocket module

**Construction:**  

* let s = new WebSocket(url)  
```js
let s = new WebSocket("wss://example.com");
```

**Methods:**

* send(data) - Send data with Buffer
* recv() - Receive data to a buffer

## External/Library based modules

### ODE Module

The `ODE` module provides JavaScript bindings for the **Open Dynamics Engine (ODE)**.
It allows creating and manipulating worlds, rigid bodies, geometries, spaces, and physical joints.

---

### Table of Contents

1. [Class `World`](#class-world)
2. [Class `Body`](#class-body)
3. [Class `Geom`](#class-geom)
4. [Class `Space`](#class-space)
5. [Class `JointGroup`](#class-jointgroup)
6. [Class `Joint`](#class-joint)

   * [Ball Joint](#ball-joint)
   * [Hinge Joint](#hinge-joint)
   * [Slider Joint](#slider-joint)
   * [Hinge2 Joint](#hinge2-joint)
   * [Universal Joint](#universal-joint)
   * [Fixed Joint](#fixed-joint)
   * [AMotor Joint](#amotor-joint)
7. [Global Functions (`ODE` namespace)](#global-functions-ode-namespace)

   * [Geometry Creation](#geometry-creation)
   * [Joint Creation](#joint-creation)
   * [Structure Creation](#structure-creation)

---

### World

Represents the physical simulation world. It defines global properties such as gravity and integration parameters.

### Methods

* `setGravity(x, y, z)` — Set the world gravity vector.
* `getGravity()` — Returns the gravity vector `[x, y, z]`.
* `setCFM(value)` — Set the Constraint Force Mixing parameter.
* `setERP(value)` — Set the Error Reduction Parameter.
* `step(dt)` — Advances the simulation by `dt` seconds.
* `quickStep(dt)` — Advances the simulation using the quick step integrator.
* `setQuickStepIterations(iter)` — Set the number of iterations for the quick step solver.
* `stepWithContacts(dt, space, jointGroup)` — Advances the simulation including collision detection.
* `destroyWorld()` — Frees all resources associated with the world.

---

### Body

Represents a rigid body that can move, receive forces, and participate in collisions.

### Methods

* `setPosition(x, y, z)`
* `getPosition()` — Returns `[x, y, z]`.
* `setRotation(matrix)` — Set the rotation using a 3x3 matrix or quaternion.
* `getRotation()` — Returns the current rotation matrix.
* `setLinearVel(x, y, z)` — Set linear velocity.
* `getLinearVel()` — Returns `[vx, vy, vz]`.
* `setAngularVel(x, y, z)` — Set angular velocity.
* `getAngularVel()` — Returns `[wx, wy, wz]`.
* `setMass(mass)`
* `setMassBox(density, lx, ly, lz)` — Set mass from a box shape.
* `setMassSphere(density, radius)` — Set mass from a sphere shape.
* `addForce(x, y, z)` — Apply a linear force.
* `addTorque(x, y, z)` — Apply a torque.
* `enable()` — Enable simulation for the body.
* `disable()` — Disable simulation for the body.
* `enabled()` — Returns `true` if enabled, otherwise `false`.
* `free()` — Free the body resources.

---

### Geom

Represents a collision geometry. A geometry can optionally be attached to a `Body`.

### Methods

* `setPosition(x, y, z)`
* `setRotation(matrix)`
* `getPosition()`
* `getRotation()`
* `setBody(body)` — Attach this geometry to a body.
* `getBody()` — Returns the attached body.
* `free()`

---

### Space

A space groups geometries for collision detection.

### Methods

* `collide(callback)` — Runs collision detection between all geometries in the space. Calls `callback(geom1, geom2)` for each pair.
* `free()`

---

### JointGroup

Represents a group of temporary joints, often used for collision contacts.

### Methods

* `empty()` — Removes all joints from the group.
* `free()` — Frees the group resources.

---

### Joint

Represents a constraint between two bodies.

### General Methods

* `free()` — Frees the joint.
* `attach(body1, body2)` — Attaches the joint to two bodies.

---

### Ball Joint

* `setBallAnchor(x, y, z)`
* `getBallAnchor()`

---

### Hinge Joint

* `setHingeAnchor(x, y, z)`
* `setHingeAxis(x, y, z)`
* `addHingeTorque(t)`
* `getHingeAnchor()`
* `getHingeAxis()`
* `getHingeAngle()`
* `getHingeAngleRate()`

---

### Slider Joint

* `setSliderAxis(x, y, z)`
* `addSliderForce(f)`
* `getSliderAxis()`
* `getSliderPosition()`
* `getSliderPositionRate()`

---

### Hinge2 Joint

* `setHinge2Anchor(x, y, z)`
* `setHinge2Axis1(x, y, z)`
* `setHinge2Axis2(x, y, z)`
* `AddHinge2Torques(t1, t2)`
* `getHinge2Anchor()`
* `getHinge2Axis1()`
* `getHinge2Axis2()`
* `getHinge2Angle1()`
* `getHinge2Angle1Rate()`
* `getHinge2Angle2Rate()`

---

### Universal Joint

* `setUniversalAnchor(x, y, z)`
* `setUniversalAxis1(x, y, z)`
* `setUniversalAxis2(x, y, z)`
* `setUniversalTorques(t1, t2)`
* `getUniversalAnchor()`
* `getUniversalAxis1()`
* `getUniversalAxis2()`
* `getUniversalAngle1()`
* `getUniversalAngle2()`
* `getUniversalAngle1Rate1()`
* `getUniversalAngle2Rate2()`

---

### Fixed Joint

* `setFixed()`

---

### AMotor Joint

* `setAMotorNumAxes(n)`
* `setAMotorAxis(index, rel, x, y, z)`
* `setAMotorAngle(axis, angle)`
* `setAMotorMode(mode)`
* `setAMotorTorques(x, y, z)`
* `getAMotorNumAxes()`
* `getAMotorAxis(index)`
* `getAMotorAxisRel(index)`
* `getAMotorAngle(index)`
* `getAMotorAngleRate(index)`
* `getAMotorMode()`

---

## Global Functions (`ODE` namespace)

* `cleanup()` — Finalize ODE and release global resources.
* `geomCollide(geom1, geom2)` — Check for collision between two geometries.

---

### Geometry Creation

* `GeomRenderObject(renderObj, space)`
* `GeomBox(space, lx, ly, lz)`
* `GeomSphere(space, radius)`
* `GeomPlane(space, a, b, c, d)` — Defines a plane `Ax + By + Cz = D`.
* `GeomTransform(space, geom)`
* `GeomRay(space, length)` — Creates a ray geometry for raycasting. See [Ray Methods](#ray-methods) below.

---

### Ray Methods

Ray geometries support specialized methods for raycasting and collision detection:

* `raySetLength(length)` — Set the ray length.
* `rayGetLength()` — Returns the current ray length.
* `raySet(px, py, pz, dx, dy, dz)` — Set ray position `(px, py, pz)` and direction `(dx, dy, dz)`.
* `rayGet()` — Returns an object with `start` and `direction` arrays: `{start: [x,y,z], direction: [dx,dy,dz]}`.
* `raySetParams(firstContact, backfaceCull)` — Configure ray behavior:
  * `firstContact` — If `true`, returns only the first contact found.
  * `backfaceCull` — If `true`, ignores back-facing surfaces.
* `rayGetParams()` — Returns current parameters: `{firstContact: boolean, backfaceCull: boolean}`.
* `raySetClosestHit(closestHit)` — If `true`, returns only the closest hit.
* `rayGetClosestHit()` — Returns whether closest hit mode is enabled.

**Ray Contact Information:**

When a ray collides with another geometry, contact data follows special conventions:

* **position** — Point where the ray intersects the surface.
* **normal** — Surface normal at the contact point (oriented for reflection if ray is first argument).
* **depth** — Distance from ray start to contact point.

---

### Joint Creation

* `JointBall(world, group)`
* `JointHinge(world, group)`
* `JointSlider(world, group)`
* `JointHinge2(world, group)`
* `JointUniversal(world, group)`
* `JointFixed(world, group)`
* `JointNull(world, group)`
* `JointAMotor(world, group)`

---

### Structure Creation

* `World()`
* `Space()`
* `Body(world)`
* `JointGroup()`

---

## Module System
AthenaEnv can import JavaScript or native compiled modules.
### JavaScript Modules
Module writing is pretty straightforward when you are used to JavaScript. Here's an example:
```js
export function fib(n)
{
    if (n <= 0)
        return 0;
    else if (n == 1)
        return 1;
    else
        return fib(n - 1) + fib(n - 2);
}

```
To import it in runtime, just do:
```js
import fib from 'module_code.js'
```
### Native Modules
For native modules, you have to follow a few instructions to write them: [Building native modules](docs/NATIVE_MODULES.md)  
  
Native module importing is pretty simple:
```js
import * as CustomModule from 'sample_module.erl' //the "as" thing is actually important here
```

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
