/**
 * AthenaEnv — Native AOT compiler (`Native` global).
 *
 * `Native.compile()` translates a JavaScript function into native MIPS
 * R5900 machine code by reading the function's already-compiled QuickJS
 * bytecode (it does NOT re-parse source text) and emitting native
 * instructions for it. See `docs/NATIVE_COMPILER.md` for IR/ABI details.
 *
 * Verified against `src/js_api/ath_native.c` and `src/native_compiler/*`.
 * Notable behaviors and undocumented functions are called out in `@remarks`
 * below.
 */

/** Primitive type names accepted by {@link Native.compile}'s `args`/`returns`. */
type NativePrimitiveType =
    | "int" | "int32"
    | "uint" | "uint32"
    | "int64"
    | "uint64"
    | "bool"
    | "float" | "float32"
    | "Int32Array"
    | "Uint32Array"
    | "Float32Array"
    | "DynamicInt32Array"
    | "DynamicUint32Array"
    | "DynamicFloat32Array"
    | "string"
    | "String"
    | "StringView"
    | "ptr";

/** Return-type names accepted by {@link Native.compile} (`'void'` is only valid here). */
type NativeReturnType = NativePrimitiveType | "void";

/** Canonical type-name strings as reported back by {@link Native.getInfo}. */
type NativeCanonicalType =
    | "int32" | "uint32" | "int64" | "uint64" | "bool" | "float32"
    | "Int32Array" | "Uint32Array" | "Float32Array"
    | "DynamicInt32Array" | "DynamicUint32Array" | "DynamicFloat32Array"
    | "ptr" | "StructArray" | "string" | "StringView" | "void" | "unknown";

/** Field type string accepted by {@link Native.struct}'s `fields` object. `'float[3]'`-style suffixes declare fixed-size arrays. */
type NativeStructFieldType = NativePrimitiveType | `${NativePrimitiveType}[${number}]`;

/**
 * A single compiled-function argument slot in {@link Native.compile}'s
 * `signature.args`. Besides a plain type-name string, an argument may be:
 * - the literal `'self'` — ONLY valid inside {@link Native.struct}'s
 *   `methods` table; using it in a top-level `Native.compile()` call
 *   produces a non-callable "deferred" marker object, not a function
 *   (see {@link NativeCompileResult}).
 * - a struct constructor (as returned by {@link Native.struct}) — passed by pointer.
 * - a 1-element tuple `[StructConstructor]` — a pointer to a contiguous array of that struct type.
 */
type NativeArgSpec = NativePrimitiveType | "self" | NativeStructConstructor | readonly [NativeStructConstructor];

interface NativeCompileSignature {
    /** Up to 8 argument slots (`MAX_NATIVE_ARGS`); exceeding this throws `RangeError`. */
    args: NativeArgSpec[];
    /** Defaults to `'void'` when omitted. */
    returns?: NativeReturnType;
}

/**
 * A function object produced by {@link Native.compile}. It is directly
 * callable (`typeof fn === 'function'`) — not a handle/wrapper you need to
 * `.call()`. Two extra own properties are attached for internal bookkeeping
 * but are visible to userland JS as well.
 *
 * @remarks Functions compiled from a signature containing `'self'` outside
 * of {@link Native.struct} are NOT callable — see {@link Native.compile}.
 */
type NativeFunction<Args extends any[] = any[], R = any> = ((...args: Args) => R) & {
    readonly _isNative: true;
    /** Raw handle to the compiled function; pass this (not the function itself) to {@link Native.free} / {@link Native.getInfo}. */
    readonly _nativeHandle: number;
};

/** Metadata returned by {@link Native.getInfo}. */
interface NativeFunctionInfo {
    /** Size in bytes of the emitted MIPS machine code. */
    codeSize: number;
    argCount: number;
    returnType: NativeCanonicalType;
    argTypes: NativeCanonicalType[];
}

/** Struct field descriptor map for {@link Native.struct}. Values must be plain type-name strings — `{type, length}` object syntax is NOT supported (silently drops the field). */
type NativeStructFields = Record<string, NativeStructFieldType>;

/**
 * Method table for {@link Native.struct}. Each value should be produced by
 * {@link Native.compile} — either with `'self'` as its first `args` entry
 * (bound lazily to a pointer-to-this-struct-type when the struct
 * constructor runs), or already fully compiled (in which case the
 * instance's pointer is auto-prepended as the hidden first argument), or a
 * plain JS function (attached as-is, not natively compiled).
 */
type NativeStructMethods = Record<string, Function>;

/** A live, indexable view over an array-typed struct field (e.g. `transform.position`). Not a real `Array`. */
interface NativeStructArrayView {
    readonly length: number;
    [index: number]: number;
}

/** One allocated instance of a struct type created via {@link Native.struct}. */
interface NativeStructInstance {
    [field: string]: number | NativeStructArrayView | Function;
}

/** A contiguous, GC-managed block of struct instances allocated by `StructConstructor.array()`. */
interface NativeStructInstanceArray {
    readonly length: number;
    [index: number]: NativeStructInstance;
}

/** The callable/`new`-able constructor returned by {@link Native.struct}. */
interface NativeStructConstructor {
    new (): NativeStructInstance;
    (): NativeStructInstance;
    /** Total size of one instance, in bytes. */
    readonly size: number;
    /** Allocates `count` contiguous instances in a single block. */
    array(count: number): NativeStructInstanceArray;
}

/** Opaque handle returned by {@link Native.createDynamicArray}, to be passed to a compiled function expecting a `Dynamic*Array` argument or back into the `Native.dynArray*` helpers. */
type NativeDynamicArrayHandle = number;

declare namespace Native {
    /**
     * Compiles `fn` to native MIPS machine code per `signature`.
     *
     * @remarks `fn` must be a plain JS (bytecode-backed) function — native
     * `Native.compile()`-produced functions and bound functions cannot be
     * recompiled and throw `InternalError`.
     * @remarks A `signature.args` entry of `'self'` makes this call return a
     * non-callable deferred marker instead of a compiled function; only use
     * `'self'` inside {@link Native.struct}'s `methods` table.
     * @remarks A `returns: 'ptr'` function always yields `undefined` to JS —
     * pointer return values are not currently marshaled back.
     */
    function compile<Args extends any[] = any[], R = any>(
        signature: NativeCompileSignature,
        fn: (...args: Args) => R
    ): NativeFunction<Args, R>;

    /** True only on real PS2 hardware/BIOS builds; always `false` on PC/host builds. */
    function isSupported(): boolean;

    /**
     * Frees a compiled function immediately (optional — the GC will
     * eventually reclaim it otherwise).
     * @remarks Takes the numeric `fn._nativeHandle`, not `fn` itself.
     */
    function free(handle: number): void;

    /**
     * @remarks Takes the numeric `fn._nativeHandle`, not `fn` itself.
     */
    function getInfo(handle: number): NativeFunctionInfo;

    /**
     * Runs `func` `iterations` times and returns elapsed milliseconds.
     * @remarks Always invokes `func` with zero arguments — only meaningful
     * for functions compiled with an empty `args` list.
     */
    function benchmark(func: NativeFunction, iterations: number): number;

    /**
     * Returns a MIPS disassembly listing as text.
     * @remarks The `func` argument is ignored; this always disassembles the
     * most recently compiled function (the global compiler's last emit
     * buffer), regardless of which function reference is passed.
     */
    function disassemble(func: NativeFunction): string;

    /**
     * Defines a C-layout-compatible struct type.
     *
     * @remarks There is no `name` parameter — `argv[0]` must be the fields
     * object directly (`Native.struct('Vec3', {...})` throws `TypeError`).
     * Every struct is internally named `"anonymous"`.
     * @remarks Array fields use a string suffix, e.g. `position: 'float[3]'`
     * — the `{type: 'float', length: 3}` object form is not supported and
     * silently drops the field instead of throwing.
     * @remarks Struct fields can only be primitive (+ fixed-array) types;
     * nesting one struct type inside another is not reachable from JS.
     */
    function struct(fields: NativeStructFields, methods?: NativeStructMethods): NativeStructConstructor;

    /**
     * Allocates a growable native array of `type` with initial `capacity`
     * (default 16). Returns an opaque pointer handle, e.g. for passing to a
     * function compiled with a `'Dynamic*Array'` parameter.
     *
     */
    function createDynamicArray(type: "int" | "int32" | "uint" | "uint32" | "float" | "float32", capacity?: number): NativeDynamicArrayHandle;
    function dynArrayLength(handle: NativeDynamicArrayHandle): number;
    /** Throws `RangeError` if `index` is out of bounds. */
    function dynArrayGet(handle: NativeDynamicArrayHandle, index: number): number;
    function dynArrayFree(handle: NativeDynamicArrayHandle): void;
}
