/**
 * AthenaEnv — Audio/video playback: `Sound`, `Video`.
 * Verified against src/js_api/ath_sound.c (requires `ATHENA_AUDIO`) and
 * src/js_api/ath_mpeg.c (requires `ATHENA_MPEG_VIDEO`).
 */

declare class SoundStream {
    /** Calls the global `athena_sound_play`. */
    play(): void;
    /** Free audio stream from memory. */
    free(): void;
    /**
     * Pause audio stream.
     * @remarks Pausing is global, not per-instance — the underlying native
     * call takes no stream argument, so this pauses ALL stream playback.
     */
    pause(): void;
    playing(): boolean;
    /** Restart audio to its beginning (call {@link play} again if it's not the current track). */
    rewind(): void;

    /** Current track playtime, in milliseconds. */
    position: number;
    /** Current track duration, in milliseconds. Read-only. */
    readonly length: number;
    loop: boolean;
}

declare class SoundSfx {
    /**
     * Play the sound effect.
     * @param channel When omitted, automatically uses a free channel.
     * @returns the channel index used — always a number, even when `channel` is omitted.
     */
    play(channel?: number): number;
    /** Free sound effect from memory. */
    free(): void;
    playing(channel: number): boolean;

    /** Duration, in milliseconds. Read-only. */
    readonly length: number;
    /** 0 to 100. */
    volume: number;
    /** -100 (left) to 100 (right), 0 is center. */
    pan: number;
    /** -100 to 100, 0 is default. */
    pitch: number;
}

declare namespace Sound {
    /** Set master volume. */
    function setVolume(volume: number): void;
    /** Returns the first free channel found, for use with {@link SoundSfx.play}. */
    function findChannel(): number;
    /** Loads an audio stream file (WAV, OGG). Callable with or without `new`. */
    function Stream(path: string): SoundStream;
    /** Loads a sound effect (ADPCM). Callable with or without `new`. */
    function Sfx(path: string): SoundSfx;
}

declare class Video {
    /** @remarks `path` is required — omitting it throws `TypeError`, unlike most other AthenaEnv constructors which silently coerce missing args. */
    constructor(path: string);

    /** Video width in pixels. Read-only. */
    readonly width: number;
    /** Video height in pixels. Read-only. */
    readonly height: number;
    /** Read-only. */
    readonly fps: number;
    /** True once the video is loaded and ready for playback. Read-only. */
    readonly ready: boolean;
    /** True once playback has finished. Read-only. */
    readonly ended: boolean;
    /** Read-only. */
    readonly playing: boolean;
    loop: boolean;
    /** Current frame as an Image object (useful as a texture), or `null` if no frame decoded yet. Read-only. */
    readonly frame: Image | null;
    /** Read-only. */
    readonly currentFrame: number;

    play(): void;
    pause(): void;
    /** Stops playback and resets to the beginning. */
    stop(): void;
    /** Process video decoding (call every frame). @returns true if a new frame was decoded. */
    update(): boolean;
    /** Draw the current frame to screen. @param x,y,w,h Each independently defaults to 0 when omitted. */
    draw(x?: number, y?: number, w?: number, h?: number): void;
    /** Release video resources. */
    free(): void;
}
