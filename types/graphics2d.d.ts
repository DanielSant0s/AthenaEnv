/**
 * AthenaEnv — 2D graphics: `Screen`, `Draw`, `Color`, `Image`, `ImageList`,
 * `Font`, `TileMap`.
 *
 * Verified against src/js_api/ath_screen.c, ath_shape.c, ath_color.c,
 * ath_image.c, ath_imagelist.c, ath_font.c, ath_sprite.c. `Font`, `Image`
 * and `ImageList` are exposed globally as their class constructors
 * directly; `Screen`, `Draw`, `Color` are plain namespace objects; `TileMap`
 * is a single object bundling constructors, helpers and a data snapshot.
 */

/** Packed RGBA color value, as produced by `Color.new(...)`. */
type RGBAColor = number;

// ---------------------------------------------------------------------------
// Filter constants (bare globals — NOT Image.NEAREST / Image.LINEAR)
// ---------------------------------------------------------------------------

/** Nearest-neighbour texture filtering. Bare global — not `Image.NEAREST`. */
declare const NEAREST: number;
/** Bilinear texture filtering. Bare global — not `Image.LINEAR`. */
declare const LINEAR: number;

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

/**
 * Declared as an interface + const (rather than a namespace) because it
 * exposes a method literally named `new`, which TypeScript namespaces
 * cannot declare (`new` is a reserved word) — an interface can, using a
 * quoted property name.
 */
interface ColorModule {
    /** @param a Defaults to 128 (0x80) when omitted. */
    "new"(r: number, g: number, b: number, a?: number): RGBAColor;
    getR(color: RGBAColor): number;
    getG(color: RGBAColor): number;
    getB(color: RGBAColor): number;
    getA(color: RGBAColor): number;
    /** Colors are immutable-style values — returns a NEW packed color, does not mutate `color`. */
    setR(color: RGBAColor, r: number): RGBAColor;
    setG(color: RGBAColor, g: number): RGBAColor;
    setB(color: RGBAColor, b: number): RGBAColor;
    setA(color: RGBAColor, a: number): RGBAColor;
}

declare const Color: ColorModule;

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

declare namespace Draw {
    function point(x: number, y: number, color: RGBAColor): void;
    function line(x1: number, y1: number, x2: number, y2: number, color: RGBAColor): void;
    function rect(x: number, y: number, width: number, height: number, color: RGBAColor): void;
    /** @param filled Defaults to `true` when omitted. */
    function circle(x: number, y: number, radius: number, color: RGBAColor, filled?: boolean): void;
    /**
     * Flat-shaded triangle.
     * @remarks `color2`/`color3` must be supplied together — passing only one switches to the gouraud native call with the other read out-of-bounds.
     */
    function triangle(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, color1: RGBAColor): void;
    /** Gouraud-shaded triangle (per-vertex colors). */
    function triangle(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, color1: RGBAColor, color2: RGBAColor, color3: RGBAColor): void;
    /** Flat-shaded quad. */
    function quad(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, x4: number, y4: number, color1: RGBAColor): void;
    /** Gouraud-shaded quad (per-vertex colors). */
    function quad(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number, x4: number, y4: number, color1: RGBAColor, color2: RGBAColor, color3: RGBAColor, color4: RGBAColor): void;
}

// ---------------------------------------------------------------------------
// Image / ImageList
// ---------------------------------------------------------------------------

declare class ImageList {
    constructor();
    /** Starts the background loading thread and processes queued images. */
    process(): void;
}

declare class Image {
    /**
     * @param path Path to an image file (png/bmp/jpg). Omit to create a blank/renderable surface.
     * @param asyncList When given, the (still-loading) image is queued onto this list for background loading instead of loading synchronously.
     */
    constructor(path?: string, asyncList?: ImageList);

    /** Copies a VRAM block from `src` to `dst`. */
    static copyVRAMBlock(src: Image, srcX: number, srcY: number, dst: Image, dstX: number, dstY: number): void;

    /** Draw the image on screen (call every frame). */
    draw(x: number, y: number): void;
    /** True once an asynchronously-loaded image has finished loading. */
    ready(): boolean;
    /** Converts a 24bpp (RGB) image to 16bpp in place, saving memory. */
    optimize(): boolean;
    /** Free asset content immediately. Not mandatory — the garbage collector will do this eventually. */
    free(): void;
    /** Lock texture data in VRAM (useful for textures/render targets that are always needed). */
    lock(): boolean;
    /** Unlock texture data in VRAM so it can be evicted for more-used textures. */
    unlock(): boolean;
    locked(): boolean;

    /** Drawing size. Defaults to the original image size. */
    width: number;
    height: number;
    /** Start of the source area drawn from the image. Default 0. */
    startx: number;
    starty: number;
    /** End of the source area drawn from the image. Default the original image size. */
    endx: number;
    endy: number;
    /** Rotation angle. Default 0. */
    angle: number;
    /** Tinting. Default `Color.new(255, 255, 255, 128)`. */
    color: RGBAColor;
    /** `NEAREST` or `LINEAR` (bare globals). Default `NEAREST`. */
    filter: number;
    /** Real memory footprint, in bytes. @remarks Has a setter but writes are silently ignored — effectively read-only. */
    size: number;
    /** Bits per pixel. */
    bpp: number;
    /** The image's pixel data, as a zero-copy `ArrayBuffer` view. @remarks The setter only takes effect on images still marked "delayed" (freshly created, not yet uploaded) — otherwise silently ignored. */
    pixels: ArrayBuffer;
    /** Present only for palette (4bpp/8bpp indexed) images; `undefined` otherwise. @remarks The setter only takes effect while the image is "delayed" and already has a palette allocated. */
    palette?: ArrayBuffer;
    /** Real texture area allocated in memory. */
    texWidth: number;
    texHeight: number;
    /** Whether this texture can be used as a rendering target. */
    renderable: boolean;
}

// ---------------------------------------------------------------------------
// Font
// ---------------------------------------------------------------------------

/** Returned by {@link Font.render}; only constructible that way. */
declare class FontRender {
    /** Draw the pre-shaped text (call every frame). */
    print(x: number, y: number): void;
}

declare class Font {
    /** @param path Path to a font file (png/bmp/jpg atlas, or otf/ttf). Omit to load the built-in default font. */
    constructor(path?: string);

    static readonly ALIGN_NONE: number;
    static readonly ALIGN_TOP: number;
    static readonly ALIGN_BOTTOM: number;
    static readonly ALIGN_LEFT: number;
    static readonly ALIGN_RIGHT: number;
    static readonly ALIGN_VCENTER: number;
    static readonly ALIGN_HCENTER: number;
    static readonly ALIGN_CENTER: number;

    /** Draw text on screen (call every frame). */
    print(x: number, y: number, text: string): void;
    /**
     * Pre-shapes `text` into a reusable {@link FontRender}, useful for text
     * that doesn't change between frames.
     */
    render(text: string): FontRender;
    /** Text absolute size in pixels. */
    getTextSize(text: string): { width: number; height: number };

    /** Tinting. Default `Color.new(255, 255, 255, 128)`. */
    color: RGBAColor;
    /** Proportional scale. Default `1.0`. */
    scale: number;
    /** Outline tinting. Default `Color.new(0, 0, 0, 128)`. */
    outline_color: RGBAColor;
    /** Outline size. Default `0.0`. Mutually exclusive with {@link dropshadow} (one of the two must be `0.0`). */
    outline: number;
    /** Drop-shadow tinting. Default `Color.new(0, 0, 0, 128)`. */
    dropshadow_color: RGBAColor;
    /** Drop-shadow offset. Default `0.0`. Mutually exclusive with {@link outline}. */
    dropshadow: number;
    /** Bitwise combination of `Font.ALIGN_*`. Default `Font.ALIGN_NONE`. */
    align: number;
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------

interface AlphaBlendEquation {
    a: number;
    b: number;
    c: number;
    d: number;
    fix: number;
}

interface ScissorBounds {
    x1: number;
    y1: number;
    x2: number;
    y2: number;
}

interface ScreenMode {
    mode: number;
    width: number;
    height: number;
    psm: number;
    interlace: number;
    field: number;
    psmz: number;
    zbuffering: boolean;
    double_buffering: boolean;
    /** @remarks Only consumed by {@link Screen.setMode} — `Screen.getMode()` does not populate this field. */
    pass_count?: number;
}

declare namespace Screen {
    // -- VRAM stat ids --------------------------------------------------------
    const VRAM_SIZE: number;
    const VRAM_USED_TOTAL: number;
    const VRAM_USED_STATIC: number;
    const VRAM_USED_DYNAMIC: number;

    // -- render-state params (Screen.getParam/setParam) ------------------------
    const ALPHA_TEST_ENABLE: number;
    const ALPHA_TEST_METHOD: number;
    const ALPHA_TEST_REF: number;
    const ALPHA_TEST_FAIL: number;
    const DST_ALPHA_TEST_ENABLE: number;
    const DST_ALPHA_TEST_METHOD: number;
    const DEPTH_TEST_ENABLE: number;
    const DEPTH_TEST_METHOD: number;
    /** Literal-typed (rather than plain `number`) so {@link getParam}/{@link setParam} can discriminate their overloads. */
    const ALPHA_BLEND_EQUATION: 8;
    /** Literal-typed (rather than plain `number`) so {@link getParam}/{@link setParam} can discriminate their overloads. */
    const SCISSOR_BOUNDS: 9;
    const PIXEL_ALPHA_BLEND_ENABLE: number;
    const COLOR_CLAMP_MODE: number;

    // -- alpha test methods ----------------------------------------------------
    const ALPHA_NEVER: number;
    const ALPHA_ALWAYS: number;
    const ALPHA_LESS: number;
    const ALPHA_LEQUAL: number;
    const ALPHA_EQUAL: number;
    const ALPHA_GEQUAL: number;
    const ALPHA_GREATER: number;
    const ALPHA_NEQUAL: number;

    // -- alpha-fail behavior -----------------------------------------------------
    const ALPHA_FAIL_NO_UPDATE: number;
    const ALPHA_FAIL_FB_ONLY: number;
    const ALPHA_FAIL_ZB_ONLY: number;
    const ALPHA_FAIL_RGB_ONLY: number;

    // -- destination-alpha test ----------------------------------------------------
    const DST_ALPHA_ZERO: number;
    const DST_ALPHA_ONE: number;

    // -- depth test ------------------------------------------------------------
    const DEPTH_NEVER: number;
    const DEPTH_ALWAYS: number;
    const DEPTH_GEQUAL: number;
    const DEPTH_GREATER: number;

    // -- alpha blend equation operands -----------------------------------------
    const SRC_RGB: number;
    const DST_RGB: number;
    const ZERO_RGB: number;
    const SRC_ALPHA: number;
    const DST_ALPHA: number;
    const ALPHA_FIX: number;
    /** Pre-packed `{a:SRC_RGB, b:DST_RGB, c:SRC_ALPHA, d:DST_ALPHA, fix:0}` blend equation. */
    const BLEND_DEFAULT: bigint;
    const BLEND_ADD_NOALPHA: bigint;
    const BLEND_ADD: bigint;

    // -- video mode --------------------------------------------------------------
    const NTSC: number;
    const PAL: number;
    const DTV_480p: number;
    const DTV_576p: number;
    const DTV_720p: number;
    const DTV_1080i: number;
    const INTERLACED: number;
    const PROGRESSIVE: number;
    const FIELD: number;
    const FRAME: number;

    // -- pixel storage format (PSM) ------------------------------------------------
    const CT32: number;
    const CT24: number;
    const CT16: number;
    const CT16S: number;

    // -- z-buffer format -------------------------------------------------------
    const Z32: number;
    const Z24: number;
    const Z16: number;
    const Z16S: number;

    // -- buffer ids --------------------------------------------------------------
    const DRAW_BUFFER: number;
    const DISPLAY_BUFFER: number;
    const DEPTH_BUFFER: number;

    /** Runs `loopFn` every frame with automatic clear/flip (ideal for quick demos). */
    function display(loopFn: () => void): void;
    /** Persists a clear color used by {@link display} and as the default for {@link clear} with no args. */
    function clearColor(color: RGBAColor): void;
    /** @param color Defaults to `Color.new(0, 0, 0, 128)` when omitted. */
    function clear(color?: RGBAColor): void;
    /** Submits all queued draw packets and swaps the display buffers. */
    function flip(): void;
    /** @param statId Defaults to `Screen.VRAM_USED_TOTAL`. @returns bytes. */
    function getMemoryStats(statId?: number): number;
    function setVSync(enabled: boolean): void;
    /** Required before {@link getFPS} will return meaningful values. */
    function setFrameCounter(enabled: boolean): void;
    function waitVblankStart(): void;
    /** @param frameIntervalMs Measurement window. Defaults to 1000. */
    function getFPS(frameIntervalMs?: number): number;
    function getMode(): ScreenMode;
    function setMode(canvas: ScreenMode): void;
    /** Allocates internal draw/display/depth buffers (required for off-screen rendering APIs). */
    function initBuffers(): void;
    /** Restores the buffers created by {@link initBuffers}. */
    function resetBuffers(): void;
    function getBuffer(bufferId: number): Image;
    /** Call {@link initBuffers} before using this function. @param mask Defaults to 0. */
    function setBuffer(bufferId: number, image: Image, mask?: number): void;
    /** Toggles between the two GS drawing contexts. @returns the newly active context id. */
    function switchContext(): number;
    /** Forces pending GIF packets to be flushed immediately. */
    function flush(): void;

    function getParam(param: 8): AlphaBlendEquation;
    function getParam(param: 9): ScissorBounds;
    function getParam(param: number): number;
    function setParam(param: 8, value: AlphaBlendEquation): void;
    function setParam(param: 9, value: ScissorBounds | number): void;
    function setParam(param: number, value: number | boolean): void;

    /** Packs the GS alpha-blend formula `Output = (((A-B)*C) >> 7) + D` into a value suitable for `Screen.setParam(Screen.ALPHA_BLEND_EQUATION, ...)`. */
    function alphaEquation(a: number, b: number, c: number, d: number, fix: number): bigint;
}

// ---------------------------------------------------------------------------
// TileMap
// ---------------------------------------------------------------------------

declare namespace TileMap {
    interface Material {
        texture_index: number;
        /** Packed GS alpha-blend value, e.g. from {@link Screen.alphaEquation}. */
        blend_mode: number | bigint;
        /** Last sprite index handled by this material. */
        end_offset: number;
    }

    interface DescriptorOptions {
        textures?: Array<string | Image>;
        /** Either a list of material descriptors, or a raw packed `ArrayBuffer`/TypedArray of native records. */
        materials?: Material[] | ArrayBuffer | ArrayBufferView;
    }

    /** Immutable render description: texture list + materials. Create once per tileset/level. */
    class Descriptor {
        constructor(options: DescriptorOptions);
        readonly materialCount: number;
    }

    interface InstanceOptions {
        descriptor: Descriptor;
        /** @remarks The native binding also technically accepts a TypedArray view here, but every buffer produced by {@link SpriteBuffer} is a plain `ArrayBuffer`, and that's the idiomatic/documented usage (e.g. `new DataView(instance.getSpriteBuffer())`) — typed as `ArrayBuffer` to match. */
        spriteBuffer?: ArrayBuffer;
    }

    /** A drawable referencing a descriptor plus a sprite buffer. Multiple instances can share the same descriptor. */
    class Instance {
        constructor(options: InstanceOptions);
        /** @param x,y,z Each independently default to 0. */
        render(x?: number, y?: number, z?: number): void;
        /** Swaps the current buffer pointer for an entirely new buffer (must match {@link TileMap.layout}). */
        replaceSpriteBuffer(buffer: ArrayBuffer): void;
        /** Returns the attached buffer for direct mutation, or `undefined` if none attached. */
        getSpriteBuffer(): ArrayBuffer | undefined;
        /**
         * Copies a range of sprites from another buffer.
         * @param spriteCount Defaults to `srcBuffer`'s full sprite count; silently clamped down to what `srcBuffer` actually holds.
         */
        updateSprites(dstOffset: number, srcBuffer: ArrayBuffer, spriteCount?: number): void;
    }

    interface SpriteObject {
        x?: number; y?: number; zindex?: number; w?: number; h?: number;
        u1?: number; v1?: number; u2?: number; v2?: number;
        r?: number; g?: number; b?: number; a?: number;
    }

    namespace SpriteBuffer {
        /** Allocates a zeroed buffer sized for `count` sprites. */
        function create(count: number): ArrayBuffer;
        /** Converts an array of JS objects into a packed native buffer. Omitted fields default to 0. */
        function fromObjects(objects: SpriteObject[]): ArrayBuffer;
    }

    /** `stride`/per-field byte offsets, for editing sprite buffers directly via `DataView`/TypedArray. A static snapshot, not live getters. */
    const layout: {
        stride: number;
        offsets: {
            x: number; y: number; w: number; h: number;
            u1: number; v1: number; u2: number; v2: number;
            r: number; g: number; b: number; a: number;
            zindex: number;
        };
    };

    /** Initializes the tilemap renderer. Call once at startup. */
    function init(): void;
    /** Starts a tilemap render batch. Call once per frame before drawing instances. */
    function begin(): void;
    /** Moves the shared tilemap camera offset. */
    function setCamera(x: number, y: number): void;
}
