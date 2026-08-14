/**
 * AthenaEnv — 3D renderer: `Render`, `RenderData`, `RenderObject`, `Batch`,
 * `SceneNode`, `AsyncLoader`, `AnimCollection`, `Camera`, `Lights`, `Shadows`.
 *
 * Verified against src/js_api/ath_render.c, ath_3dcamera.c, ath_lights.c,
 * ath_anim_3d.c, ath_shadows.c.
 */

interface XYZ { x: number; y: number; z: number; }
interface RGB { r: number; g: number; b: number; }
interface RGBA { r: number; g: number; b: number; a: number; }

// ---------------------------------------------------------------------------
// Render namespace
// ---------------------------------------------------------------------------

interface RenderMaterial {
    ambient: RGB;
    diffuse: RGB;
    specular: RGB;
    emission: RGB;
    transmittance: RGB;
    shininess: number;
    refraction: number;
    transmission_filter: RGB;
    /** @remarks The real property key is `disolve`. */
    disolve: number;
    texture_id: number;
    bump_texture_id: number;
    ref_texture_id: number;
    decal_texture_id: number;
}

interface RenderMaterialIndex {
    index: number;
    end: number;
}

/** Shape produced by {@link Render.vertexList}, consumed by `new RenderData(...)`. */
interface RenderVertexList {
    positions?: Float32Array | ArrayBuffer;
    normals?: Float32Array | ArrayBuffer;
    texcoords?: Float32Array | ArrayBuffer;
    colors?: Float32Array | ArrayBuffer;
    materials?: RenderMaterial[];
    material_indices?: RenderMaterialIndex[];
    /** Present only when requested via {@link Render.vertexList}'s options argument. */
    shareBuffers?: boolean;
}

interface RenderStats {
    drawCalls: number;
    triangles: number;
    culled: number;
}

declare namespace Render {
    const PL_NO_LIGHTS: number;
    const PL_DEFAULT: number;
    const PL_SPECULAR: number;

    const CULL_FACE_NONE: number;
    const CULL_FACE_BACK: number;
    const CULL_FACE_FRONT: number;

    /** Initializes internal renderer state (GS/VU microprograms, materials cache). Call once during boot. */
    function init(): void;
    /** Starts a render pass and resets batched state. Call once per frame before issuing draw calls. */
    function begin(): void;
    /**
     * Configures the default projection matrix. `width`/`height` override the
     * auto-derived aspect ratio when non-zero. `nearClip`/`farClip` and
     * `width`/`height` are each read as a pair — you cannot supply one
     * without the other in its pair.
     */
    function setView(fov?: number, nearClip?: number, farClip?: number, width?: number, height?: number): void;

    /** Per-frame draw call / triangle counters. */
    function stats(): RenderStats;
    /** Resets the counters returned by {@link stats}. */
    function resetStats(): void;

    /**
     * Packages typed arrays (4-component packing: xyzw / n1n2n3w / stqw / rgba)
     * into the shape expected by `new RenderData(...)`.
     * @param shareBuffers When true, the resulting RenderData keeps a live
     * reference to these buffers instead of copying them.
     */
    function vertexList(
        positions: Float32Array,
        normals: Float32Array,
        texcoords: Float32Array,
        colors: Float32Array,
        materials: RenderMaterial[],
        material_indices: RenderMaterialIndex[],
        shareBuffers?: boolean | { shareBuffers?: boolean }
    ): RenderVertexList;

    /** Convenience helper that returns an `{r,g,b,a}` color object for materials. */
    function materialColor(r: number, g: number, b: number, alpha?: number): RGBA;

    /**
     * Builds a material descriptor used by RenderData/RenderObject. Texture
     * ids accept `-1` to disable a layer.
     * @remarks All 13 arguments are effectively required at the native
     * binding level (the C function does not pad missing trailing
     * arguments) even though the property is misleadingly spelled
     * `disolve` in the resulting object (not `dissolve`).
     */
    function material(
        ambient: RGB, diffuse: RGB, specular: RGB, emission: RGB, transmittance: RGB,
        shininess: number, refraction: number, transmission_filter: RGB, disolve: number,
        texture_id: number, bump_texture_id: number, ref_texture_id: number, decal_texture_id: number
    ): RenderMaterial;

    /** Tags the vertex/material arrays so the renderer knows which faces should use each material slice. */
    function materialIndex(index: number, end: number): RenderMaterialIndex;
}

// ---------------------------------------------------------------------------
// RenderData
// ---------------------------------------------------------------------------

/** Per-vertex attribute buffers, as returned/accepted by {@link RenderData.vertices}. Each field is a raw `ArrayBuffer`, not a typed array. */
interface RenderDataVertices {
    /** `x, y, z, adc` per vertex. */
    positions?: ArrayBuffer;
    /** `n1, n2, n3, adc` per vertex. */
    normals?: ArrayBuffer;
    /** `s, t, q, w` per vertex. */
    texcoords?: ArrayBuffer;
    /** `r, g, b, a` per vertex. */
    colors?: ArrayBuffer;
}

interface RenderDataBone {
    name: string;
    parent_id: number;
    /** Local position. */
    position: Vector4;
    /** Local rotation. */
    rotation: Vector4;
    /** Local scale. */
    scale: Vector4;
    inverse_bind: Matrix4;
}

interface RenderDataMaterialPatch {
    ambient?: RGB; diffuse?: RGB; specular?: RGB; emission?: RGB;
    transmittance?: RGB; transmission_filter?: RGB;
    /** Clamped 0..255. */
    shininess?: number;
    /** Clamped 0.1..4.0. */
    refraction?: number;
    /** Clamped 0..1. Real key is `disolve`, not `dissolve`. */
    disolve?: number;
    /** Clamped -10..10. */
    bump_scale?: number;
}

declare class RenderData {
    /** Loads a WaveFront OBJ file (with MTL support, including per-vertex colors and multi-texturing). */
    constructor(mesh: string, texture?: Image);
    /**
     * Builds a mesh directly from typed-array vertex data, e.g. the result
     * of {@link Render.vertexList}.
     * @param tristrip Interpret the vertex stream as a triangle strip.
     */
    constructor(vertexData: RenderVertexList, texture?: Image, tristrip?: boolean);

    /** Gets the nth texture object from the model. */
    getTexture(id: number): Image | undefined;
    /** Changes or sets the nth texture on the model. */
    setTexture(id: number, texture: Image): void;
    /** Appends a texture, returning its new index. */
    pushTexture(texture: Image): number;
    /**
     * Free asset content immediately. Not mandatory — the garbage collector
     * will do this eventually.
     */
    free(): void;
    /** Alias for {@link free}. */
    dispose(): void;
    /**
     * Clones this RenderData.
     * @param deep When false (default), shares geometry buffers and textures
     * with the original (only materials/material_indices are independent).
     * When true, produces a fully independent deep copy.
     */
    clone(deep?: boolean): RenderData;
    /** Merges partial fields into `materials[index]`. */
    updateMaterial(index: number, props: RenderDataMaterialPatch): number;
    /**
     * Locks in the current vertex buffers for zero-copy rendering.
     * @returns false if any buffer isn't uniquely owned (e.g. after a
     * shallow {@link clone} or when constructed with `shareBuffers`).
     */
    freeze(): boolean;

    /**
     * @remarks There is no separate `positions`/`normals`/`texcoords`/
     * `colors` property — all four live together under `vertices`, and each
     * is a raw `ArrayBuffer`.
     */
    vertices: RenderDataVertices;
    materials: RenderMaterial[];
    material_indices: RenderMaterialIndex[];

    /** Rendering pipeline — one of `Render.PL_*`. */
    pipeline: number;

    /** Vertex quantity. Has a setter registered but writes are ignored (effectively read-only). */
    size: number;
    /** Mesh bounding box: 8 corner points. */
    bounds: XYZ[];

    accurate_clipping: boolean;
    /** Default: `Render.CULL_FACE_BACK`. */
    face_culling: number;
    texture_mapping: boolean;
    /** Flat = 0, Gouraud = 1. */
    shade_model: number;

    /** Textures referenced by this mesh. */
    textures?: Image[];
    /** Present only on skinned meshes. */
    bones?: RenderDataBone[];
}

// ---------------------------------------------------------------------------
// RenderObject
// ---------------------------------------------------------------------------

interface RenderObjectBone {
    transform: Matrix4;
    position: Vector4;
    rotation: Vector4;
    scale: Vector4;
}

interface BoneTransform {
    matrix: Matrix4;
    position: XYZ;
    rotation: { x: number; y: number; z: number; w: number };
    index: number;
}

declare class RenderObject {
    constructor(renderData: RenderData);

    /** Draws the object on screen. */
    render(): void;
    /**
     * Draws the object's bounding box.
     * @remarks `color` is required — calling `renderBounds()` with no
     * arguments is an out-of-bounds native read, not a clean default.
     */
    renderBounds(color: number): void;
    /** Free asset content immediately. Not mandatory — the garbage collector will do this eventually. */
    free(): void;
    /** Alias for {@link free}. */
    dispose(): void;
    /** Looks up a bone by name. */
    getBoneTransform(name: string): BoneTransform | undefined;
    /** Attaches an ODE collision geometry to this object. Pass `null` to detach. @remarks Requires the ODE build. */
    setCollision(geom: ODEGeom | null): void;
    /** Attaches an ODE physics body to this object. Pass `null` to detach. @remarks Requires the ODE build. */
    setPhysics(body: ODEBody | null): void;

    /** Play an animation on a skinned RenderObject. @remarks Only present on instances built from skinned RenderData — calling it on a non-skinned RenderObject throws (`not a function`). */
    playAnim(anim: number, loop: boolean): void;
    /** @remarks Only present on instances built from skinned RenderData. `anim` may be omitted. */
    isPlayingAnim(anim?: number): boolean;

    /** Default `{x:0, y:0, z:0}`. */
    position: XYZ;
    /** Default `{x:0, y:0, z:0}`. */
    rotation: XYZ;
    scale: XYZ;
    /** Object RTS transform matrix. */
    transform: Matrix4;
    /** Whether this object is culled against the view frustum. */
    frustumCull: boolean;

    /** Present only on instances built from skinned RenderData. */
    bone_matrices?: Matrix4[];
    /** Present only on instances built from skinned RenderData. */
    bones?: RenderObjectBone[];
}

// ---------------------------------------------------------------------------
// Batch
// ---------------------------------------------------------------------------

declare class Batch {
    constructor(options?: { autoSort?: boolean });

    /** Adds a RenderObject to the batch. @returns the batch's new size. */
    add(renderObject: RenderObject): number;
    /** Removes all objects from the batch. */
    clear(): void;
    /** Renders all objects using optimal state transitions. @returns number of draws. */
    render(): number;
    /** Destroys the native batch. */
    free(): void;

    /** Number of objects currently in the batch. */
    readonly size: number;
}

// ---------------------------------------------------------------------------
// SceneNode
// ---------------------------------------------------------------------------

declare class SceneNode {
    constructor();

    /** Parents a child SceneNode. @returns the new child count. */
    addChild(node: SceneNode): number;
    /** Unparents a specific child. @returns the new child count. */
    removeChild(node: SceneNode): number;
    /** Attaches a RenderObject to this node. @returns the new attachment count. */
    attach(renderObject: RenderObject): number;
    /** Detaches the specific object, or all attachments when omitted. @returns the new attachment count (`0` when detaching all). */
    detach(renderObject?: RenderObject): number;
    /** Recomputes world transforms (rotate → scale → translate) for this subtree. */
    update(): void;
    /** Destroys the native node. */
    free(): void;

    position: XYZ;
    rotation: XYZ;
    scale: XYZ;
}

// ---------------------------------------------------------------------------
// AsyncLoader
// ---------------------------------------------------------------------------

declare class AsyncLoader {
    constructor(options?: { jobsPerStep?: number });

    /**
     * Queues a model load.
     * @param callback Invoked as `(path, renderData)` when the item is ready.
     * @param texture Optional Image bound during load; silently ignored if invalid.
     * @returns the resulting queue size.
     */
    enqueue(path: string, callback: (path: string, renderData: RenderData) => void, texture?: Image): number;
    /** Processes up to `budget` items from the queue (or the configured jobs-per-step if omitted). @returns number processed. */
    process(budget?: number): number;
    /** Clears pending items and releases their callbacks. */
    clear(): void;
    /** Clears and destroys the loader. */
    destroy(): void;
    /** Number of items still pending. */
    size(): number;
    getJobsPerStep(): number;
    /** Clamped to at least 1. @returns the applied value. */
    setJobsPerStep(n: number): number;
}

// ---------------------------------------------------------------------------
// AnimCollection
// ---------------------------------------------------------------------------

/** Opaque animation handle, indexable out of an {@link AnimCollection} and passed to {@link RenderObject.playAnim}. */
type AnimHandle = number;

/**
 * Loads the animations inside a glTF file into an index/name-addressable
 * collection (`collection[0]`, `collection["run_fast"]`).
 */
declare class AnimCollection {
    constructor(file: string);
    /** Free this collection's resources immediately. */
    free(): void;
    [index: number]: AnimHandle;
    [name: string]: AnimHandle | Function;
}

// ---------------------------------------------------------------------------
// Lights
// ---------------------------------------------------------------------------

/**
 * Declared as an interface + const (rather than a namespace) because it
 * exposes a method literally named `new`, which TypeScript namespaces
 * cannot declare (`new` is a reserved word) — an interface can, using a
 * quoted property name.
 */
interface LightsModule {
    DIRECTION: number;
    AMBIENT: number;
    DIFFUSE: number;
    SPECULAR: number;

    /**
     * Allocates a new light, returning its id for use with {@link set}.
     * @remarks Allocates and returns a new light id.
     */
    "new"(): number;
    set(id: number, attribute: number, x: number, y: number, z: number): void;
}

declare const Lights: LightsModule;

// ---------------------------------------------------------------------------
// Camera (3D orbit/free camera)
// ---------------------------------------------------------------------------

/**
 * The 3D scene camera. Not to be confused with AthenaEnv's separate USB
 * webcam `Camera` module (`src/js_api/ath_camera.c`) — both are registered
 * under the native module name `"Camera"`; when both are compiled in
 * (`ATHENA_GRAPHICS` + `ATHENA_CAMERA`), which one `globalThis.Camera`
 * resolves to depends on native module registration order. This
 * declaration documents the 3D scene camera.
 */
declare namespace Camera {
    function position(x: number, y: number, z: number): void;
    function target(x: number, y: number, z: number): void;
    function orbit(yaw: number, pitch: number): void;
    function turn(yaw: number, pitch: number): void;
    function pan(x: number, y: number): void;
    function dolly(distance: number): void;
    function zoom(distance: number): void;

    interface CameraState {
        position: XYZ;
        target: XYZ;
        up: XYZ;
        local_up: XYZ;
    }
    /** Captures the current camera state. */
    function save(): CameraState;
    /** Restores a previously-saved camera state. `up`/`local_up` are optional. */
    function restore(state: { position: XYZ; target: XYZ; up?: XYZ; local_up?: XYZ }): void;

    /** Update camera state (must be called every frame). */
    function update(): void;
}

// ---------------------------------------------------------------------------
// Shadows
// ---------------------------------------------------------------------------

declare namespace Shadows {
    const SHADOW_BLEND_DARKEN: number;
    const SHADOW_BLEND_ALPHA: number;
    const SHADOW_BLEND_ADD: number;

    class Projector {
        constructor(texture?: Image);

        /** Sets a full custom 4x4 transform (flat 16-element array or Matrix4). */
        setTransform(matrix: Matrix4 | number[]): void;
        /** Shadow projection area size, in world units. */
        setSize(width: number, height: number): void;
        /** Grid resolution for shadow tessellation (minimum 2x2). */
        setGrid(gridX: number, gridZ: number): void;
        /** Directional light direction (automatically normalized). */
        setLightDir(x: number, y: number, z: number): void;
        /** Shadow bias to prevent z-fighting (default: 0.01). */
        setBias(bias: number): void;
        /** Offset along the light direction to shift the shadow center. */
        setLightOffset(offset: number): void;
        /** Maximum slope angle (cosine) for shadow projection. */
        setSlopeLimit(maxSlopeCos: number): void;
        /** Shadow color and alpha (0.0-1.0 range). */
        setColor(r: number, g: number, b: number, a: number): void;
        /** One of `Shadows.SHADOW_BLEND_*`. */
        setBlend(mode: number): void;
        /** Texture UV rectangle for the shadow's appearance. */
        setUVRect(u0: number, v0: number, u1: number, v1: number): void;
        /**
         * Enables/disables ODE ray casting for accurate shadow placement.
         * @remarks Argument order is `(space, enable, length)`. Requires the ODE build.
         */
        enableRaycast(space: ODESpace, enable: boolean, rayLength: number): void;
        /** Renders the shadow projector. Call every frame. */
        render(): void;

        position: XYZ;
        /** @remarks Only `x`,`y`,`z` are exposed — despite being described as a quaternion, there is no `w` component on this property. */
        rotation: XYZ;
        scale: XYZ;
    }
}
