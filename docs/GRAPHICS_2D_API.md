# 2D Graphics API

`Screen`, `Draw`, `Color`, `Image`, `ImageList`, `Font`.

For the tilemap renderer, see [TileMap](TILEMAP.md). For packed color values, see [`Color` in the Math API](MATH_API.md#color-module).

For native pooled particles, screen effects and world billboards, see [Particles and 2D FX](PARTICLES.md).

P.S.: *Italic* parameters refer to optional parameters.

## Image Module

**Functions:**
* `Image.copyVRAMBlock(src_buffer, src_x, src_y, dest_buffer, dest_x, dest_y)` - Copies a VRAM block from `src_buffer` to `dest_buffer`.

**Construction:**
```js
let image = new Image(*path*, *async_list*);
```
* `path` - Path to the file, e.g.: `"images/test.png"`.
* `async_list` - Gets an `ImageList` object, an asynchronous image loading list, if you want to load images in the background.
```js
let test = new Image("owl.png");
```

**Properties:**

* `width`, `height` - Image drawing size, default value is the original image size.
* `startx`, `starty` - Beginning of the area that will be drawn from the image, default value is 0.0.
* `endx`, `endy` - End of the area that will be drawn from the image, default value is the original image size.
* `angle` - Image rotation angle, default value is 0.0.
* `color` - Image tinting, default value is `Color.new(255, 255, 255, 128)`.
* `filter` - Choose between `LINEAR` or `NEAREST`, default value is `NEAREST`.
* `size` - Returns image real size occupied in memory.
* `bpp` - Bits per-pixel quantity.
* `pixels` - The image pixel ArrayBuffer. Reading or writing it brings a compressed EE copy back to raw pixels. See [Automatic EE RAM compaction](CORE_API.md#automatic-ee-ram-compaction): once a texture larger than 2 KB is resident in GS VRAM and left alone, its EE copy is LZ4-compressed in the background. Drawing it from VRAM does not decompress it.
* `palette` - If it's a palette image, it has a palette ArrayBuffer right here.
* `texWidth`, `texHeight` - The real texture area in memory.
* `renderable` - Set if the texture can be used as a rendering buffer.

**Methods:**

* `draw(x, y)` - Draw loaded image onscreen (call it every frame). Example: `image.draw(15.0, 100.0);`
* `optimize()` - If your image has 24 bits per-pixel (RGB), you can use this to make it 16 bits per-pixel, saving memory.
* `lock()` - Lock texture data in VRAM, useful when you deal with textures that are always needed or rendering buffers.
* `unlock()` - Unlock texture data in VRAM, so it can be replaced with more used textures.
* `locked()` - Returns true if the texture is locked in VRAM.
* `ready()` - Returns true if an asynchronous image was successfully loaded in memory.
* `free()` - Free asset content immediately. This is a way to quickly free stuff from memory, but you can also wait for the garbage collector, so it's not mandatory.

### ImageList

**Construction:**
```js
let async_list = new ImageList(); // This constructor creates a new thread and a queue to load images in background, avoid building multiple ImageList objects.
```
**Methods:**
* `process()` - This method starts the thread and loads added images on the queue.
```js
async_list.process();
```

## Draw module

* `Draw.point(x, y, color)` - Draws a pixel at the specified color and position on the screen.
* `Draw.rect(x, y, width, height, color)` - Draws a rectangle at the specified color, position and size on the screen.
* `Draw.line(x, y, x2, y2, color)` - Draws a line at the specified colors and position on the screen.
* `Draw.circle(x, y, radius, color, *filled*)` - Draws a circle at the specified color, position, radius and fill on the screen.
* `Draw.triangle(x, y, x2, y2, x3, y3, color, *color2*, *color3*)` - Draws a triangle at the specified points, positions and colors on the screen.
* `Draw.quad(x, y, x2, y2, x3, y3, x4, y4, color, *color2*, *color3*, *color4*)` - Draws a quad at the specified points, positions and colors on the screen.

## Screen module

* `Screen.display(loopFn)` – Runs `loopFn` every frame with automatic clear/flip (ideal for quick demos).
* `Screen.clearColor(color)` – Persists a clear color used by `Screen.display` and as the default for `Screen.clear()` with no args.
* `Screen.clear(color = Color.new(0,0,0,128))` – Clears the current draw buffer.
* `Screen.flip()` – Submits all queued draw packets and swaps the display buffers.
* `Screen.getMemoryStats(statId = Screen.VRAM_USED_TOTAL)` – Returns VRAM usage in bytes. Other counters: `VRAM_SIZE`, `VRAM_USED_STATIC`, `VRAM_USED_DYNAMIC`.
* `Screen.setVSync(enabled)` – Enables/disables VSync (locks FPS to the mode's refresh rate).
* `Screen.setFrameCounter(enabled)` – Enables internal FPS counter required by `Screen.getFPS()`.
* `Screen.setFrameOverlap(enabled)` / `Screen.getFrameOverlap()` – Frame overlap (default on, double-buffered modes only): `Screen.flip()` returns as soon as the frame is queued, and the CPU builds the next frame while the GS is still drawing the current one. Disable it to make `flip()` block until the frame is on screen.
* `Screen.getProfile()` – CPU (EE) time per frame in ms, averaged over the last 30 frames: `{ frame, script, render, anim, physics, gpuWait, vsync }`. The sections never overlap (`script` is everything not covered by another one, JS included). `gpuWait` is time spent blocked on the GPU: near zero means the CPU is the bottleneck, large means the GPU is.
* `Screen.waitVblankStart()` – Blocks until the next vertical blank.
* `Screen.getFPS(frameIntervalMs = 1000)` – Returns the measured FPS over the given window (requires frame counter enabled).
* `Screen.getMode()` – Returns the active video mode object: `{ mode, width, height, psm, interlace, field, double_buffering, zbuffering, psmz }`.
* `Screen.setMode(canvas)` – Applies a video mode previously fetched/edited via `Screen.getMode()`. Also accepts an optional `pass_count` field.
* `Screen.initBuffers()` – Allocates internal draw/display/depth buffers (required for off-screen rendering APIs).
* `Screen.resetBuffers()` – Restores the buffers created by `initBuffers()`.
* `Screen.getBuffer(bufferId)` – Returns the GS surface bound to `DRAW_BUFFER`, `DISPLAY_BUFFER`, or `DEPTH_BUFFER`.
* `Screen.setBuffer(bufferId, image[, mask = 0])` – Replaces a buffer with a renderable `Image`. Call `Screen.initBuffers()` before using this function.
* `Screen.channelShuffle(source, lut, inChannel, options)` – Remaps a channel through a LUT. `options.rawSource` / `rawDestination` use CT32 views of the physical bytes without allocating or changing the images; masked alpha writes can reuse the unused Z24 byte while preserving depth. A raw Z source bypasses Morton unswizzling.
* `Screen.switchContext()` – Toggles between the two GS drawing contexts and returns the active context id.
* `Screen.flush()` – Forces pending GIF packets to be flushed immediately.
* `Screen.getParam(param)` / `Screen.setParam(param, value)` – Low-level access to GS render state (alpha/depth/scissor/etc.). Pass either raw integers or helper objects (see below).
* `Screen.alphaEquation(a, b, c, d, fix)` – Utility that packs the GS alpha-blend formula into a 64-bit value suitable for `Screen.setParam(Screen.ALPHA_BLEND_EQUATION, ...)`.

### Render state parameters

* `Screen.ALPHA_BLEND_EQUATION` - Set alpha blending mode based on a fixed coefficient equation.
  * `Screen.SRC_RGB` - Source RGB
  * `Screen.DST_RGB` - Framebuffer RGB
  * `Screen.ZERO_RGB` - Zero RGB
  * `Screen.SRC_ALPHA` - Source alpha
  * `Screen.DST_ALPHA` - Framebuffer alpha
  * `Screen.ALPHA_FIX` - C = Fix argument

The GS's alpha blending formula is fixed but it contains four variables that can be reconfigured:
```
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

* `Screen.ALPHA_TEST_ENABLE`: bool - Enable or disable alpha test.
* `Screen.ALPHA_TEST_METHOD` - Set alpha test method.
  * `Screen.ALPHA_NEVER` - All pixels fail
  * `Screen.ALPHA_ALWAYS` - All pixels pass
  * `Screen.ALPHA_LESS` - pixel alpha < ALPHA_TEST_REF passes
  * `Screen.ALPHA_LEQUAL` - pixel alpha <= ALPHA_TEST_REF passes
  * `Screen.ALPHA_EQUAL` - pixel alpha == ALPHA_TEST_REF passes
  * `Screen.ALPHA_GEQUAL` - pixel alpha >= ALPHA_TEST_REF passes
  * `Screen.ALPHA_GREATER` - pixel alpha > ALPHA_TEST_REF passes
  * `Screen.ALPHA_NEQUAL` - pixel alpha != ALPHA_TEST_REF passes
* `Screen.ALPHA_TEST_REF`: int - Alpha test reference, normally 128 (0x80).
* `Screen.ALPHA_TEST_FAIL` - Set behavior when alpha test fails
  * `Screen.ALPHA_FAIL_NO_UPDATE` - Neither frame buffer nor depth buffer are updated.
  * `Screen.ALPHA_FAIL_FB_ONLY` - Only frame buffer is updated.
  * `Screen.ALPHA_FAIL_ZB_ONLY` - Only depth buffer is updated.
  * `Screen.ALPHA_FAIL_RGB_ONLY` - Only RGB in framebuffer is updated.
* `Screen.DST_ALPHA_TEST_ENABLE`: bool - Enable or disable destination alpha test.
* `Screen.DST_ALPHA_TEST_METHOD` - Destination alpha test method.
  * `Screen.DST_ALPHA_ZERO` - Destination alpha bit == 0 passes
  * `Screen.DST_ALPHA_ONE` - Destination alpha bit == 1 passes

  The alpha bit tested depends on the framebuffer format. If the format is CT32, bit 7 of alpha is tested. If the format is CT16/S, the sole alpha bit is tested. If the format is CT24, all pixels pass due to the lack of alpha.
* `Screen.DEPTH_TEST_ENABLE`: bool - Enable or disable depth test.
* `Screen.DEPTH_TEST_METHOD` - Depth test method.
  * `Screen.DEPTH_NEVER` - All pixels fail
  * `Screen.DEPTH_ALWAYS` - All pixels pass
  * `Screen.DEPTH_GEQUAL` - pixel Z >= depth buffer Z passes
  * `Screen.DEPTH_GREATER` - pixel Z > depth buffer Z passes
* `Screen.PIXEL_ALPHA_BLEND_ENABLE`: bool - If enabled, switches per-pixel alpha blending based on source alpha most significant bit value.
* `Screen.COLOR_CLAMP_MODE`: bool - If true, RGB components will be 0 if negative after alpha-blending, or 0xFF if 0x100 or above. Else, each color component will be ANDed with 0xFF, i.e. they overlap.
* `Screen.ALPHA_CORRECTION`: bool - If enabled, bit 7 of every alpha value written to a CT32 draw buffer is forced to 1 (current context only). Together with `DST_ALPHA_TEST_ENABLE` it turns the alpha bit into a one-bit stencil: each pixel is written at most once until the alpha is cleared again.
* `Screen.SCISSOR_BOUNDS` - Object model: `{x1: x_start, y1: y_start, x2: x_end, y2: y_end}`

## Font module

**Construction:**
```js
let font = new Font(path);  // It supports png, bmp, jpg, otf, ttf.
```
* `path` - Path to a font file, e.g.: `"images/atlas.png"`, `"fonts/font.png"`.
```js
let font = new Font("Segoe UI.ttf"); //Load trueType font
```

**Properties:**
* `color` - Font tinting, default value is `Color.new(255, 255, 255, 128)`.
* `scale` - Proportional scale, default: `1.0f`
* `outline_color` - Outline tinting, default value is `Color.new(0, 0, 0, 128)`.
* `outline` - Outline size, default: `0.0f`
* `dropshadow_color` - Dropshadow tinting, default value is `Color.new(0, 0, 0, 128)`.
* `dropshadow` - Shadow drop position, default: `0.0f`
* `align` - Font alignment, default value is `Font.ALIGN_NONE`. Available options:
  * `Font.ALIGN_NONE`
  * `Font.ALIGN_TOP`
  * `Font.ALIGN_BOTTOM`
  * `Font.ALIGN_LEFT`
  * `Font.ALIGN_RIGHT`
  * `Font.ALIGN_VCENTER`
  * `Font.ALIGN_HCENTER`
  * `Font.ALIGN_CENTER`

P.S.: outline and drop shadow do not coexist, so one of them must be `0.0f`.

**Methods:**
* `print(x, y, text)` - Draw text on screen (call it every frame). Example: `font.print(10.0, 10.0, "Hello world!");`
* `getTextSize(text)` - Returns text absolute size in pixels (width, height). Example: `const size = font.getTextSize("Hello world!");`
