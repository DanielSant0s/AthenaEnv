/**
 * AthenaEnv — Core runtime globals.
 *
 * Covers `std`, `os` (QuickJS libc bindings), `System`, `IOP`, `Archive`,
 * `Mutex`, `Thread` and `Timer`. All symbols here are installed on
 * `globalThis` by the native `athena_js_globals_prelude()` bootstrap
 * (see `src/js_api/ath_env.c`) — there is no `import` required in user code.
 *
 * @see https://github.com/DanielSant0s/AthenaEnv
 */

// ---------------------------------------------------------------------------
// Global shell helpers (installed by js_std_add_helpers, quickjs-libc.c)
// ---------------------------------------------------------------------------

interface Console {
    /** Prints all arguments space-separated followed by a newline, to stdout. The only method `console` exposes. */
    log(...args: any[]): void;
}
declare const console: Console;

/** Equivalent to `console.log(...)`. */
declare function print(...args: any[]): void;

/** Command-line arguments the script was launched with. */
declare const scriptArgs: string[];

// ---------------------------------------------------------------------------
// std module (quickjs-libc)
// ---------------------------------------------------------------------------

/** A file handle returned by {@link std.open}, {@link std.fdopen}, {@link std.tmpfile} or {@link std.popen}. */
interface StdFile {
    /** Close the file. @returns 0 if OK or -errno in case of I/O error. */
    close(): number;
    /** Outputs the string with UTF-8 encoding. */
    puts(str: string): void;
    /**
     * Formatted printf. Supports the same conversions as the C library `printf`.
     * Integer conversions (e.g. `%d`) truncate to 32 bits; use the `l` modifier
     * (e.g. `%ld`) to truncate to 64 bits instead.
     */
    printf(fmt: string, ...args: any[]): void;
    /** Flush the buffered file. */
    flush(): void;
    /** Seek to a given file position. `offset` may be a number or a bigint. @returns 0 if OK or -errno on I/O error. */
    seek(offset: number | bigint, whence: number): number;
    /** Current file position. */
    tell(): number;
    /** Current file position as a bigint. */
    tello(): bigint;
    /** True if end of file has been reached. */
    eof(): boolean;
    /** The underlying OS file descriptor. */
    fileno(): number;
    /** True if the file is in an error state. */
    error(): boolean;
    /** Clear the error indicator. */
    clearerr(): void;
    /** Read `length` bytes from the file into `buffer` at byte offset `position`. @returns bytes read. */
    read(buffer: ArrayBuffer, position: number, length: number): number;
    /** Write `length` bytes from `buffer` (at byte offset `position`) to the file. @returns bytes written. */
    write(buffer: ArrayBuffer, position: number, length: number): number;
    /** Read the next line (UTF-8), excluding the trailing line feed. `null` at EOF. */
    getline(): string | null;
    /** Read up to `max_size` bytes as a UTF-8 string; reads to EOF when omitted. */
    readAsString(max_size?: number): string;
    /** Read the next byte, or -1 at end of file. */
    getByte(): number;
    /** Write one byte to the file. */
    putByte(c: number): void;
}

/**
 * The `std` global. Declared as an interface + const (rather than a
 * namespace) because it exposes a property literally named `in`, which
 * TypeScript namespaces cannot declare (`in` is a reserved word).
 */
interface StdModule {
    /** Terminates the process/VM with the given exit code. */
    exit(code: number): void;
    /** Manually invoke the cycle-removal (garbage collection) algorithm. */
    gc(): void;
    /** Returns the reference count of a value (debug builds only). */
    getRefCount(value: any): number;

    /**
     * Evaluate `str` as a script in the global scope.
     * @param options.backtrace_barrier When true, error backtraces don't list stack frames below this call. Default false.
     */
    evalScript(str: string, options?: { backtrace_barrier?: boolean }): any;
    /** Evaluate the contents of `filename` as a script (global eval). */
    loadScript(filename: string): any;
    /** Reloads the whole JavaScript environment (stack, modules, etc.) running `str` as the new entry script. */
    reload(str: string): void;

    /** Read an environment variable. */
    getenv(name: string): string | undefined;
    /** Set an environment variable. */
    setenv(name: string, value: string): void;
    /** Unset an environment variable. */
    unsetenv(name: string): void;
    /** Returns an object with all environment variables as `{ [name]: value }`. */
    getenviron(): Record<string, string>;

    /** Returns true if `filename` exists. */
    exists(filename: string): boolean;
    /** Load `filename` and return its contents as a UTF-8 string, or `null` on I/O error. */
    loadFile(filename: string): string | null;

    /**
     * Open a file (wraps libc `fopen`).
     * @param flags e.g. `"r"`, `"w"`, `"rb"`, `"wb"`, `"a"`, ...
     * @param errorObj optional object whose `errno` property is set to the error code (or 0 on success).
     * @returns the file, or `null` on I/O error.
     */
    open(filename: string, flags: string, errorObj?: { errno?: number }): StdFile | null;
    /** Open a process pipe (wraps libc `popen`). */
    popen(command: string, flags: string, errorObj?: { errno?: number }): StdFile | null;
    /** Wrap an existing OS file descriptor `fd` into a {@link StdFile} (wraps libc `fdopen`). */
    fdopen(fd: number, flags: string, errorObj?: { errno?: number }): StdFile | null;
    /** Open a temporary file. */
    tmpfile(errorObj?: { errno?: number }): StdFile | null;

    /** Equivalent to `std.out.puts(str)`. */
    puts(str: string): void;
    /** Equivalent to `std.out.printf(fmt, ...args)`. */
    printf(fmt: string, ...args: any[]): void;
    /** Equivalent to the libc `sprintf()`. */
    sprintf(fmt: string, ...args: any[]): string;

    /** Standard input. */
    in: StdFile;
    /** Standard output. */
    out: StdFile;
    /** Standard error. */
    err: StdFile;

    /** Seek constants, for {@link StdFile.seek}. */
    SEEK_SET: number;
    SEEK_CUR: number;
    SEEK_END: number;

    /** Common libc errno values. Additional codes may be defined by the platform. */
    EINVAL: number;
    EIO: number;
    EACCES: number;
    EEXIST: number;
    ENOSPC: number;
    ENOSYS: number;
    EBUSY: number;
    ENOENT: number;
    EPERM: number;
    EPIPE: number;
    EBADF: number;

    /** Returns a string describing errno `errno`. */
    strerror(errno: number): string;

    /**
     * Parse `str` using a superset of `JSON.parse`. Extensions accepted:
     * single/multi-line comments, unquoted ASCII identifier keys, trailing
     * commas, single-quoted strings, `\f`/`\v` as whitespace, leading `+` on
     * numbers, and `0o`/`0x` octal/hex numbers.
     */
    parseExtJSON(str: string): any;
}

declare const std: StdModule;

// ---------------------------------------------------------------------------
// os module (quickjs-libc)
// ---------------------------------------------------------------------------

declare namespace os {
    // -- low level file access --------------------------------------------
    /** Open a file, POSIX-style. @returns a file descriptor, or < 0 on error. */
    function open(filename: string, flags: number, mode?: number): number;
    const O_RDONLY: number;
    const O_WRONLY: number;
    const O_RDWR: number;
    const O_APPEND: number;
    const O_CREAT: number;
    const O_EXCL: number;
    const O_TRUNC: number;

    /** Close the file descriptor `fd`. */
    function close(fd: number): number;
    /** Seek in the file. `offset` may be a number or bigint (a bigint is returned when `offset` is a bigint). */
    function seek(fd: number, offset: number, whence: number): number;
    function seek(fd: number, offset: bigint, whence: number): bigint;
    /** Read `length` bytes from `fd` into `buffer` at byte `offset`. @returns bytes read, or < 0 on error. */
    function read(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;
    /** Write `length` bytes from `buffer` (at byte `offset`) to `fd`. @returns bytes written, or < 0 on error. */
    function write(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;

    /** Remove (unlink) a file. @returns 0 if OK or -errno. */
    function remove(filename: string): number;
    /** Rename a file. @returns 0 if OK or -errno. */
    function rename(oldname: string, newname: string): number;
    /** @returns `[canonicalPath, errno]`. */
    function realpath(path: string): [string, number];
    /** @returns `[cwd, errno]`. */
    function getcwd(): [string, number];
    /** Change the current directory. @returns 0 if OK or -errno. */
    function chdir(path: string): number;
    /** Create a directory. @returns 0 if OK or -errno. */
    function mkdir(path: string, mode?: number): number;

    interface StatObject {
        dev: number; ino: number; mode: number; nlink: number;
        uid: number; gid: number; rdev: number; size: number; blocks: number;
        /** Milliseconds since epoch (1970). */
        atime: number; mtime: number; ctime: number;
    }
    /** @returns `[stat, errno]`. */
    function stat(path: string): [StatObject, number];
    /** Like {@link stat} but does not follow symlinks. */
    function lstat(path: string): [StatObject, number];

    /** `st_mode` interpretation constants (mirror `sys/stat.h`). */
    const S_IFMT: number;
    const S_IFIFO: number;
    const S_IFCHR: number;
    const S_IFDIR: number;
    const S_IFBLK: number;
    const S_IFREG: number;
    const S_IFSOCK: number;
    const S_IFLNK: number;
    const S_ISGID: number;
    const S_ISUID: number;

    /** Change the access/modification time of `path` (milliseconds since epoch). @returns 0 if OK or -errno. */
    function utimes(path: string, atime: number, mtime: number): number;
    /** @returns `[entries, errno]` — filenames of the directory `path`. */
    function readdir(path: string): [string[], number];

    /** Register a read-availability handler for `fd`. Pass `func = null` to remove it. Only one handler per fd. */
    function setReadHandler(fd: number, func: (() => void) | null): void;
    /** Register a write-availability handler for `fd`. Pass `func = null` to remove it. Only one handler per fd. */
    function setWriteHandler(fd: number, func: (() => void) | null): void;

    /** Sleep synchronously for `delay_ms` milliseconds. */
    function sleep(delay_ms: number): void;
    /** Call `func` after `delay` ms. @returns a timer handle usable with {@link clearTimeout}. */
    function setTimeout(func: () => void, delay: number): number;
    /** Call `func` every `interval` ms. @returns a handle usable with {@link clearInterval}. */
    function setInterval(func: () => void, interval: number): number;
    /** Schedule `func` to run as soon as possible. @returns a handle usable with {@link clearImmediate}. */
    function setImmediate(func: () => void): number;
    /** Cancel a timer created by {@link setTimeout}. */
    function clearTimeout(handle: number): void;
    /** Cancel an interval created by {@link setInterval}. */
    function clearInterval(handle: number): void;
    /** Cancel an immediate scheduled by {@link setImmediate}. */
    function clearImmediate(handle: number): void;

    /** Always `"ps2"` on AthenaEnv. */
    const platform: string;
}

// ---------------------------------------------------------------------------
// Mutex
// ---------------------------------------------------------------------------

/** A binary mutex for cooperating with {@link Thread}. */
declare class Mutex {
    constructor();
    /** Acquire the lock, blocking until it is available. */
    lock(): void;
    /** Release the lock. */
    unlock(): void;
}

// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------

interface ThreadListEntry {
    id: number;
    name: string;
    status: number;
    stack_size: number;
}

declare namespace Thread {
    /** List all system threads (OS level, not just JS threads). */
    function list(): ThreadListEntry[];
    /** Force-kill a thread by its internal ID. */
    function kill(id: number): void;
}

/** A cooperative user thread running `fn`. */
declare class Thread {
    /**
     * @param fn Entry point run on the new thread.
     * @param name Optional thread name, max 64 characters, surfaced in {@link Thread.list}.
     */
    constructor(fn: () => void, name?: string);
    /** Put the thread into an active (running) state. */
    start(): void;
    /** Stop the thread, returning it to the pre-`start()` state. */
    stop(): void;
    /** Internal thread ID (read-only). */
    readonly id: number;
    /** Thread name. */
    name: string;
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

/** Opaque handle to a {@link Timer} instance, as returned by {@link Timer.new}. */
type TimerHandle = number;

/**
 * A free-running, independently-resettable time source. Declared as an
 * interface + const because it exposes a method literally named `new`,
 * which TypeScript namespaces cannot declare (`new` is a reserved word) —
 * an interface can, using a quoted property name.
 */
interface TimerModule {
    /** Create a new timer instance. */
    "new"(): TimerHandle;
    /** Get the elapsed time of `timer`. */
    getTime(timer: TimerHandle): number;
    /** Set the elapsed time of `timer`. */
    setTime(timer: TimerHandle, value: number): void;
    /** Destroy `timer`, releasing its resources. */
    destroy(timer: TimerHandle): void;
    /** Pause `timer`. */
    pause(timer: TimerHandle): void;
    /** Resume a paused `timer`. */
    resume(timer: TimerHandle): void;
    /** Reset `timer` back to zero. */
    reset(timer: TimerHandle): void;
    /** Whether `timer` is currently running (not paused). */
    isPlaying(timer: TimerHandle): boolean;
}

declare const Timer: TimerModule;

// ---------------------------------------------------------------------------
// System
// ---------------------------------------------------------------------------

interface BDMInfo {
    name: string;
    index: number;
}

interface DirEntry {
    name: string;
    size: number;
    /** True if the entry is a directory. */
    dir: boolean;
}

interface MemoryCardInfo {
    type: number;
    freemem: number;
    format: number;
}

interface CPUInfo {
    implementation: number;
    revision: number;
    FPUimplementation: number;
    FPUrevision: number;
    ICacheSize: number;
    DCacheSize: number;
    RAMSize: number;
    MachineSize: number;
}

interface GPUInfo {
    id: number;
    revision: number;
}

interface MemoryStats {
    /** Kernel + native code size in RAM. */
    core: number;
    /** Kernel + native stack size. */
    nativeStack: number;
    /** Dynamically allocated memory tracked by Athena. */
    allocs: number;
    /** Sum of the above. */
    used: number;
}

interface FileProgress {
    current: number;
    final: number;
}

interface NativeCallArg {
    type: number;
    value: number | boolean | string;
}

declare namespace System {
    // -- native function calling --------------------------------------------
    /** Native argument type tags for {@link nativeCall}. */
    const T_LONG: number;
    const T_ULONG: number;
    const T_INT: number;
    const T_UINT: number;
    const T_SHORT: number;
    const T_USHORT: number;
    const T_CHAR: number;
    const T_UCHAR: number;
    const T_PTR: number;
    const T_BOOL: number;
    const T_FLOAT: number;
    const T_STRING: number;
    /** Argument type tag for passing/returning an `ArrayBuffer`. */
    const JS_BUFFER: number;

    /**
     * Call a native function by address.
     * @param address Memory address of the native function.
     * @param arguments Array of `{ type: System.T_*, value }` argument descriptors.
     * @param return_type One of `System.T_*`; omit for `void`.
     */
    function nativeCall(address: number, args: NativeCallArg[], return_type?: number): any;

    // -- relocatable modules (ERL) -------------------------------------------
    /** Loads a relocatable code/data module (library) from `path`. @returns a handle. */
    function loadReloc(path: string): number;
    /** Unloads a relocatable module. Ensure nothing still references its code before calling this. */
    function unloadReloc(reloc_id: number): void;
    /** Find a global symbol's address inside the Athena binary or any loaded relocatable module. */
    function findRelocObject(symbol_name: string): number | undefined;
    /** Find a symbol's address local to a specific relocatable module. */
    function findRelocLocalObject(reloc_id: number, symbol_name: string): number | undefined;

    // -- storage / filesystem -------------------------------------------------
    /** Info about a Block Device Manager device, e.g. `System.getBDMInfo("mass0")`. */
    function getBDMInfo(name: string): BDMInfo;
    /** Mount flag: read-only. */
    const READ_ONLY: number;
    /** Mount flag: prompt device selection. */
    const SELECT: number;
    /** Mount `device` at `mountpoint`. `mode` combines `System.READ_ONLY` / `System.SELECT`. */
    function mount(mountpoint: string, device: string, mode?: number): number;
    function umount(path: string): number;
    /** Available storage devices. */
    function devices(): { name: string; desc: string }[];
    function listDir(path?: string): DirEntry[];
    function removeDirectory(path: string): void;
    function copyFile(source: string, dest: string): void;
    function moveFile(source: string, dest: string): void;
    function rename(source: string, dest: string): void;

    // -- power / misc -----------------------------------------------------
    function sleep(sec: number): void;
    /** Busy-wait a single no-op cycle. */
    function delay(): void;
    function exitToBrowser(): void;
    function setDarkMode(value: boolean): void;

    /** Load and execute an ELF, optionally resetting the IOP first. */
    function loadELF(path: string, args?: string[], resetIop?: boolean): void;
    /** True if a valid PS1/PS2 disc is inserted. */
    function checkValidDisc(): boolean;
    /** Numeric disc-type identifier reported by `sceCdGetDiskType`. */
    function getDiscType(): number;
    /** True if the disc tray is currently open. */
    function checkDiscTray(): boolean;

    /** Only meaningful on SCPH-500XX and later models. */
    function getTemperature(): number;
    function getMCInfo(slot?: number): MemoryCardInfo;
    function getCPUInfo(): CPUInfo;
    function getGPUInfo(): GPUInfo;
    function getMemoryStats(): MemoryStats;

    // -- asynchronous -------------------------------------------------------
    /** Copy `source` to `dest` on a background thread; poll with {@link getFileProgress}. */
    function threadCopyFile(source: string, dest: string): void;
    function getFileProgress(): FileProgress;
}

// ---------------------------------------------------------------------------
// IOP
// ---------------------------------------------------------------------------

/** Opaque handle to a registered IOP module entry. */
type IOPModule = number;

interface IOPModuleListEntry {
    name: string;
    id: number;
}

interface IOPMemoryStats {
    free: number;
    used: number;
}

declare namespace IOP {
    /**
     * Register an IOP module for later loading.
     * @param name Module name.
     * @param data A file path (string) or the raw module image (ArrayBuffer).
     * @param options.deps IDs of modules that must be loaded first.
     * @param options.init Called (no args) once the module finishes initializing.
     * @param options.end Called (no args) on module shutdown/IOP reset.
     */
    function newModule(
        name: string,
        data: string | ArrayBuffer,
        options?: { deps?: number[]; init?: () => void; end?: () => void }
    ): IOPModule;
    /** Loads a module previously created with {@link newModule} (by handle or by name). */
    function loadModule(module: IOPModule | string, args?: string): void;
    /** Look up a registered module by name or numeric id. */
    function getModule(nameOrId: string | number): IOPModule | undefined;
    /** List all registered IOP modules. */
    function getModules(): IOPModuleListEntry[];
    /** Resets the IOP, unloading all modules. */
    function reset(): void;
    function getMemoryStats(): IOPMemoryStats;
}

// ---------------------------------------------------------------------------
// Archive
// ---------------------------------------------------------------------------

/** Opaque handle to an open archive, as returned by {@link Archive.open}. */
type ArchiveHandle = number;

interface ArchiveEntry {
    name: string;
    size: number;
    /** Modification time (archive-format-dependent unit). */
    mtime: number;
}

declare namespace Archive {
    /** Opens a ZIP, GZ or TAR file. */
    function open(fname: string): ArchiveHandle;
    /** Lists the entries of an open archive. */
    function list(zip: ArchiveHandle): ArchiveEntry[];
    /**
     * Extracts every entry of the archive to disk. For GZ archives (single
     * stream, no directory), instead returns the decompressed data as an
     * `ArrayBuffer`.
     */
    function extractAll(zip: ArchiveHandle): ArrayBuffer | undefined;
    function close(zip: ArchiveHandle): void;
    /** Extracts a `.tar` file directly (no handle required). */
    function untar(fname: string): void;
}
