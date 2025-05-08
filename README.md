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

AthenaEnv is a complete JavaScript Runtime Environment for the PlayStation 2. It has dozens of built-in exclusive libraries made for the project, such as a MMI instruction accelerated 2D renderer, asynchronous texture manager system and a advanced 3D renderer powered by VU1 and VU0. It let's you to almost instantly write modern code on PS2 powered by performant libraries and a fine-tuned interpreter for it.

### Modules:
* System: Files, folders and system stuff.
* Archive: A simple compressed file extractor and manager.
* IOP: The PlayStation 2 has an I/O processor to deal with drivers and modules. Take control of it!
* Image: Image drawing.
* ImageList: Load and manage multiple images while your code is running, multithreaded loading!
* Draw: Shape drawing, triangles, circles etc.
* Render: Advanced 3D support powered by a VU1 renderer and VU0 matrix processor.
* Screen: The entire screen of your project (2D and 3D), being able to change the resolution, enable or disable parameters.
* Font: Functions that control the texts that appear on the screen, loading texts, drawing and unloading from memory.
* Pads: Above being able to draw and everything else, A human interface is important. Supports rumble and pressure sensitivity.
* Keyboard: Basic USB keyboard support.
* Mouse: Basic USB mouse support.
* Timer: Control the time precisely in your code, it contains several timing functions.
* Sound: Sound functions, supporting WAV, OGG and ADPCM.
* Network: Net basics and web requests :D.
* Socket: Well, sockets.

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
![image](https://github.com/DanielSant0s/AthenaEnv/assets/47725160/e90471f6-7ada-4176-88e8-8a9d2c1e7eb4)  
  
Qt recommendation: Enable console output  
![console0](https://github.com/DanielSant0s/AthenaEnv/assets/47725160/7ced1570-0013-4072-ad01-66b8a63dab6e)

  
* Android: [QuickEdit](https://play.google.com/store/apps/details?id=com.rhmsoft.edit&hl=pt_BR&gl=US) and a PS2 with wLE for test.

Oh, and I also have to mention that an essential prerequisite for using AthenaEnv is knowing how to code in JavaScript.

## Quick start with Athena

Hello World:  
```js
const font = new Font("default");

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

**How to run it**

Athena is basically a JavaScript loader, so it loads .js files. It runs "main.js" by default, but you can run other file names by passing it as the first argument when launching the ELF file.

If you try to just download it on releases tab and run it, that's what you will see:
![_50bda816_20230409025946](https://user-images.githubusercontent.com/47725160/230757268-5968d7e0-79df-4e98-9c02-4ec5252e056f.png)

That's the default dashboard, coded in default main.js file. It searchs JavaScript files with the first line containing the following structure:
```js
// {"name": "App name", "author": "Who did it?", "version": "04012023", "icon": "app_icon.png", "file": "my_app.js"}
// Now you can freely code below:
```
Once it was found, it will appear on the dashboard app list.

**Error reporting system**

Athena has a consistent error system, which is capable of pointing the error type, custom message, files, lines and it even has
a color code.

EvalError:  
![_50bda816_20230409024828](https://user-images.githubusercontent.com/47725160/230756846-e7e5ef7d-4ca6-4e10-822b-bd8ab94e302f.png)

SyntaxError:  
![_50bda816_20230409024849](https://user-images.githubusercontent.com/47725160/230756861-94df60f8-8550-43a3-ac43-56d150e94145.png)

TypeError:  
![_50bda816_20230409024911](https://user-images.githubusercontent.com/47725160/230756863-9b5d2b27-ef7c-449b-8663-7b466243c425.png)

ReferenceError:  
![_50bda816_20230409024944](https://user-images.githubusercontent.com/47725160/230756870-1deac594-7b3b-4804-a5cd-bfe366f5be27.png)

RangeError:  
![_50bda816_20230409025004](https://user-images.githubusercontent.com/47725160/230756874-5b03f2ee-1f23-4629-a775-5567c580b1da.png)

InternalError:  
![_50bda816_20230409025029](https://user-images.githubusercontent.com/47725160/230756880-d3f9449f-a379-4eec-8342-721987d3c7a9.png)

URIError:  
![_50bda816_20230409025053](https://user-images.githubusercontent.com/47725160/230756884-0e7dc7c8-91b3-4a4d-9d0f-ee120b4cc18a.png)

AggregateError:  
![_50bda816_20230409025131](https://user-images.githubusercontent.com/47725160/230756885-11749f0c-ef5b-4f17-ad78-59181230e75a.png)

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

Construction:  

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
* os.platform - Return a string representing the platform: "linux", "darwin", "win32", "ps2" or "js".
    
### Color module
* let col = Color.new(r, g, b, *a*) - Returns a color object from the specified RGB(A) parameters.
* let r = Color.getR(col) - Get red intensity of the color.
* let g = Color.getG(col) - Get green intensity of the color.
* let b = Color.getB(col) - Get blue intensity of the color.
* let a = Color.getA(col) - Get alpha intensity of the color.
* Color.setR(col, r) - Set red intensity of the color.
* Color.setG(col, g) - Set green intensity of the color.
* Color.setB(col, g) - Set blue intensity of the color.
* Color.setA(col, a) - Set alpha intensity of the color.

### Image Module  

Construction:  

* let image = new Image(path, *mode*, *async_list*);  
  path - Path to the file, E.g.: "images/test.png".  
  *mode* - Choose between storing the image between **RAM** or **VRAM**, default value is RAM.  
  *async_list* - Gets a ImageList object, which is a asynchronous image loading list, if you want to load images in the background.  
```js
let test = new Image("owl.png", VRAM); 
``` 

Properties:

* width, height - Image drawing size, default value is the original image size.
* startx, starty - Beginning of the area that will be drawn from the image, the default value is 0.0.
* endx, endy - End of the area that will be drawn from the image, the default value is the original image size.
* angle - Define image rotation angle, default value is 0.0.
* color - Define image tinting, default value is Color.new(255, 255, 255, 128).
* filter - Choose between **LINEAR** or **NEAREST**, default value is NEAREST.
* size - Returns image real size occupied in memory.
* bpp - Returns image bits per-pixel qantity.
* delayed - If true, your texture was loaded in RAM, else, VRAM.
* pixels - The image pixel ArrayBuffer.
* palette - If is a palette image, it has a palette ArrayBuffer right here.

Methods:

* draw(x, y) - Draw loaded image onscreen(call it every frame). Example: image.draw(15.0, 100.0);
* optimize() - If your image has 24 bits per-pixel (aka RGB), you can use this to make it 16 bits per-pixel, saving some memory!
* ready() - Returns true if an asynchronous image was successfully loaded in memory. 
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.
  
**ImageList**

Construction:

```js
let async_list = new ImageList(); // This constructor creates a new thread and a queue to load images in background, avoid building multiple ImageList objects.
```
Methods:

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

### Render module

• Remember to enable zbuffering on screen mode, put the line of code below  
• Default NTSC mode(3D enabled): 
```js
const canvas = Screen.getMode();
canvas.zbuffering = true;
canvas.psmz = Z16S;

Screen.setMode(canvas);
```

* Render.setView(*fov*, *near_clip*, *far_clip*) - Initializes rendering routines. FOV, NearClip and FarClip aren't mandatory.  
  • fov - Field of view, default: 60  
  • near_clip - Near clip, default: 0.1  
  • far_clip - Far clip, default: 2000.0  
* Render.materialColor(red, green, blue, *alpha*) - alpha isn't mandatory.
* Render.materialIndex(index, end)
* Render.material(ambient, diffuse, specular, emission, transmittance, shininess, refraction, transmission_filter, disolve, texture_id) - Returns a material object.  
  • ambient - Render.materialColor  
  • diffuse - Render.materialColor  
  • specular - Render.materialColor  
  • emission - Render.materialColor  
  • transmittance - Render.materialColor  
  • shininess - Float32  
  • refraction - Float32  
  • transmission_filter - Render.materialColor  
  • disolve - Float32  
  • texture_id - Texture index, -1 for an untextured mesh.  
  
* Render.vertexList(positions, normals, texcoords, colors, materials, material_indices) - Returns an object used to build a RenderData. P.S.: All vertex arrays are Vector4 (x, y, z, w for i+0, i+1, i+2, i+3, in steps of 4).  
  • positions - Float32Array that stores all vertex positions (x, y, z, w/adc).  
  • normals - Float32Array that stores all vertex normals (n1, n2, n3, w/adc).  
  • texcoords - Float32Array that stores all vertex texture coordinates (s, t, q, w/adc).  
  • colors - Float32Array that stores all vertex colors (r, g, b, a).  
  • materials - A Render.material array.  
  • material_indices - A Render.materialIndex array.  
  
### RenderData module

Construction:

```js
let data = new RenderData(mesh, *texture*)
/* Load simple WaveFront OBJ files or vertex arrays.
MTL is supported on OBJs (including per-vertex colors and multi-texturing).
If you don't have a MTL file but you want to bind a texture on it,
just pass the image as a second argument if you want to use it. */
```
Methods:  

* getTexture(id) - Gets the nth texture object from the model.
* setTexture(id, texture) - Changes or sets the nth texture on models.
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.
  
Properties:

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
* backface_culling - Toggle backface culling. (Not implemented yet)
* texture_mapping - Toggle texture mapping. (it can be used to disable texturing on a whole textured model)
* shade_model - Flat = 0, Gouraud = 1.

### RenderObject module

Construction:

```js
let model = new RenderObject(render_data)
/* You need to build a RenderData first, RenderObject keeps with position, rotation   
and the individual object matrices */
```
Methods:  

* render() - Draws the object on screen.
* renderBounds() - Draws object bounding box.
* free() - Free asset content immediately. P.S.: This is a way to quick free stuff from memory, but you can also wait for the garbage collector so it's not mandatory.
  
Properties:

* position - Object with x, y and z keys that stores the object position. Default is {x:0, y:0, z:0}.
* rotation - Object with x, y and z keys that stores the object rotation. Default is {x:0, y:0, z:0}.
  
  
**Camera**   
* Camera.type(type) - Change camera function types.  
  • Camera.DEFAULT - Raw camera coordinates  
    • Camera.position(x, y, z)  
    • Camera.rotation(x, y, z)  
  • Camera.LOOKAT - "Look at" style coordinates, most common for games  
    • Camera.target(x, y, z)  
    • Camera.orbit(yaw, pitch)  
    • Camera.turn(yaw, pitch)  
    • Camera.pan(x, y)  
    • Camera.dolly(distance)  
    • Camera.zoom(distance)  
  
* Camera.update() - Update camera state (must be called every frame).  
  


**Lights**  
You have 4 lights to use in 3D scenes, use set to configure them.

* Lights.set(id, attribute, x, y, z)  
  • Avaliable light attributes: Lights.DIRECTION, Lights.AMBIENT, Lights.DIFFUSE    
  
### Screen module
* Screen.display(func) - Makes the specified function behave like a main loop, when you don't need to clear or flip the screen because it's done automatically.  
* Screen.clearColor(*color*) - Sets a constant clear color for Screen.display function.
* Screen.clear(*color*) - Clears screen with the specified color. If you don't specify any argument, it will use black as default.  
* Screen.flip() - Run the render queue and jump to the next frame, i.e.: Updates your screen.  
* let freevram = Screen.getFreeVRAM() - Returns the total of free Video Memory.  
* Screen.setVSync(bool) - Toggles VSync, which makes the framerate stable in 15, 30, 60(depending on the mode) on screen.  
* Screen.setFrameCounter(bool) - Toggles frame counting and FPS collecting.  
* Screen.waitVblankStart() - Waits for a vertical sync.  
* let fps = Screen.getFPS(frame_interval) - Get Frames per second measure within the specified frame_interval in msec. Dependant on Screen.setFrameCounter(true) to work.
* const canvas = Screen.getMode() - Get actual video mode parameters. Returns an object.
  • canvas.mode - Available modes: Screen.NTSC, Screen.DTV_480p, Screen.PAL, Screen.DTV_576p, Screen.DTV_720p, Screen.DTV_1080i.  
  • canvas.width - Screen width. Default: 640.  
  • canvas.height - Screen height. Default: 448 on NTSC consoles, 512 on PAL consoles.  
  • canvas.psm - Color mode. Available colormodes: Screen.CT16, Screen.CT16S, Screen.CT24, Screen.CT32.  
  • canvas.interlace - Available interlaces: Screen.INTERLACED, Screen.PROGRESSIVE.  
  • canvas.field - Available fields: Screen.FIELD, Screen.FRAME.  
  • canvas.double_buffering - Enable or disable double buffering(bool).  
  • canvas.zbuffering - Enable or disable Z buffering (3D buffering)(bool).  
  • canvas.psmz - ZBuffering color mode. Available zbuffer colormodes: Screen.Z16, Screen.Z16S, Screen.Z24, Screen.Z32.  
* Screen.setMode(canvas) - Set the current video mode, get an video mode object as an argument.  

### Font module

Construction:  

```js
let font = new Font(path);  // It supports png, bmp, jpg, otf, ttf.
```
  path - Path to a font file, E.g.: "images/atlas.png", "fonts/font.png".  
```js
let font = new Font("Segoe UI.ttf"); //Load trueType font 
``` 

Properties:
* color - Define font tinting, default value is Color.new(255, 255, 255, 128).
* scale - Proportional scale, default: 1.0f
* outline_color - Define outline tinting, default value is Color.new(0, 0, 0, 128).
* outline - Outline size, default: 0.0f
* dropshadow_color - Define dropshadow tinting, default value is Color.new(0, 0, 0, 128).
* dropshadow - Shadow drop position, default: 0.0f

P.S.: outline and drop shadow do not coexist, so one of them must be 0.0f.

Methods:
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
Properties:  
  • pad.btns - Button state on the current check.  
  • pad.old_btns = Button state on the last check.  
  • pad.lx - Left analog horizontal position (left = -127, default = 0, right = 128).  
  • pad.ly - Left analog vertical position (up = -127, default = 0, down = 128).  
  • pad.rx - Right analog horizontal position (left = -127, default = 0, right = 128).  
  • pad.ry - Right analog vertical position (up = -127, default = 0, down = 128).  
    
  ![analog_graph](https://user-images.githubusercontent.com/47725160/154816009-99d7e5da-badf-409b-9a3b-3618fd372f09.png)  
  
Methods:  
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
  • loop - If the sample is played in a loop, you can get or change.  
  • pitch - Sound effect pitch, you can get or change from -100 to 100, 0 is the default value.  
  
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

Construction:  

* let r = new Request()  
```js
let r = new Request();
```

Properties:

* keepalive - bool
* useragent - string
* userpwd - string
* headers - string array

Methods:

* get(url)
* head(url)
* post(url, data)
* download(url, fname)  
  
Asynchronous methods:  
* asyncGet(url)
* asyncDownload(url, fname)
* ready(*timeout*, *conn_timeout*)
* getAsyncData()
* getAsyncSize()


### Socket module

Construction:  

* let s = new Socket(domain, type)  
```js
let s = new Socket(AF_INET, SOCK_STREAM);
```

Methods:

* connect(host, port)
* bind(host, port)
* listen()
* send(data) - Send data with Buffer
* recv(size) - Receive data to a buffer
* close()


### WebSocket module

Construction:  

* let s = new WebSocket(url)  
```js
let s = new WebSocket("wss://example.com");
```

Methods:

* send(data) - Send data with Buffer
* recv() - Receive data to a buffer

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





