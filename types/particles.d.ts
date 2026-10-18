/** Native pooled particles. Simulation runs in C; sprites use VU1/GS batches. */
declare namespace Particles {
    type Vec3 = readonly [number, number, number] | Float32Array;
    type RGBA = readonly [number, number, number, number] | Float32Array;
    type Range = number | readonly [number, number] | Float32Array;
    interface SystemOptions {
        texture: Image;
        /** Fixed pool, 1..16384; default 512. */
        capacity?: number;
        /** Pixels or world units. Default "screen". */
        space?: "screen" | "world";
        blend?: "alpha" | "additive";
        /** Default true for world, false for screen. */
        sort?: boolean;
        /** Test scene depth; particles never write depth. Default true for world. */
        depthTest?: boolean;
        seed?: number;
    }
    interface EmitterOptions {
        /** Seconds, default 1. */
        lifetime?: Range;
        /** Initial height in pixels/world units, default 16. */
        size?: Range;
        /** Final size multiplier, default 1. */
        scaleEnd?: number;
        /** Width/height, default 1. */
        aspect?: number;
        velocityMin?: Vec3;
        velocityMax?: Vec3;
        /** Spawn box half extents. */
        spread?: Vec3;
        acceleration?: Vec3;
        /** Exponential velocity damping per second. */
        drag?: number;
        /** GS modulation: RGB 128 = neutral; alpha 128 = opaque. */
        color?: RGBA;
        colorEnd?: RGBA;
        /** Fractions of lifetime, 0..1. */
        fadeIn?: number;
        fadeOut?: number;
        /** Atlas rectangle [x,y,width,height], default whole texture. */
        frame?: readonly [number, number, number, number] | Float32Array;
        frameCount?: number;
        /** Frames per row, default 1. */
        frameColumns?: number;
        frameRate?: number;
        frameLoop?: boolean;
        /** Particles/second; call start() to enable. Default 0. */
        rate?: number;
    }
    interface Stats {
        capacity: number;
        active: number;
        /** Last draw's visible sprites and VU batches. */
        visible: number;
        batches: number;
        /** Accepted/dropped particles since creation or clear(), saturating uint32. */
        emitted: number;
        dropped: number;
        sort: "none" | "cached" | "radix";
    }
    interface Emitter {
        setPosition(x: number, y: number, z?: number): void;
        /** One-shot position does not move this emitter. Returns accepted count. */
        burst(count: number): number;
        burst(count: number, x: number, y: number, z?: number): number;
        start(rate?: number): void;
        /** Stops births; live particles finish naturally. */
        stop(): void;
        /** Patch settings. Initial size/lifetime of live particles stay sampled. */
        configure(options: EmitterOptions): void;
    }
    class System {
        constructor(options: SystemOptions);
        readonly capacity: number;
        readonly activeCount: number;
        /** Create once/reuse. Up to 64 emitter definitions per system. */
        createEmitter(options?: EmitterOptions): Emitter;
        /** Finite nonnegative seconds; no hidden timestep clamp. */
        update(dt: number): void;
        /** World space uses current Camera/Render view. Returns visible count. */
        draw(): number;
        /** Clear particles/counters/carry; running emitters keep running. */
        clear(seed?: number): void;
        stats(): Stats;
        /** Releases pools and waits for pending sprite references. Idempotent. */
        dispose(): void;
    }
}
