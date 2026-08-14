/**
 * AthenaEnv — Math globals: `Vector2`, `Vector3`, `Vector4`, `Matrix4`.
 * Verified against src/js_api/ath_vector.c, ath_vector4.c, ath_matrix.c.
 *
 * None of these types support operator overloading (`+ - * / ==`) — every
 * arithmetic/comparison operation is a plain method, consistent across the
 * whole math module.
 */

declare class Vector2 {
    constructor(x: number, y: number);
    x: number;
    y: number;

    norm(): number;
    dot(other: Vector2): number;
    /** 2D "cross product" is a scalar (perp dot product), not a vector — unlike {@link Vector3.cross}. */
    cross(other: Vector2): number;
    /** @remarks Named `dist`, not `distance` (inconsistent with {@link Vector4.distance}). */
    dist(other: Vector2): number;
    /** @remarks Named `distsqr`, not `distance2` (inconsistent with {@link Vector4.distance2}). */
    distsqr(other: Vector2): number;
    toString(): string;

    add(other: Vector2): Vector2;
    sub(other: Vector2): Vector2;
    /** Component-wise multiply. */
    mul(other: Vector2): Vector2;
    /** Component-wise divide. */
    div(other: Vector2): Vector2;
}

declare class Vector3 {
    constructor(x: number, y: number, z: number);
    x: number;
    y: number;
    z: number;

    norm(): number;
    dot(other: Vector3): number;
    /** True 3D cross product — returns a new Vector3. */
    cross(other: Vector3): Vector3;
    /** @remarks Named `dist`, not `distance` (inconsistent with {@link Vector4.distance}). */
    dist(other: Vector3): number;
    /** @remarks Named `distsqr`, not `distance2` (inconsistent with {@link Vector4.distance2}). */
    distsqr(other: Vector3): number;
    toString(): string;

    add(other: Vector3): Vector3;
    sub(other: Vector3): Vector3;
    /** Component-wise multiply. */
    mul(other: Vector3): Vector3;
    /** Component-wise divide. */
    div(other: Vector3): Vector3;
}

declare class Vector4 {
    constructor(x: number, y: number, z: number, w: number);
    x: number;
    y: number;
    z: number;
    w: number;

    /** Vector length/norm. */
    norm(): number;
    dot(other: Vector4): number;
    cross(other: Vector4): Vector4;
    distance(other: Vector4): number;
    distance2(other: Vector4): number;
    toString(): string;

    add(other: Vector4): Vector4;
    sub(other: Vector4): Vector4;
    /** Component-wise multiply. */
    mul(other: Vector4): Vector4;
    /** Component-wise divide. */
    div(other: Vector4): Vector4;
    equals(other: Vector4): boolean;
}

declare class Matrix4 {
    /** 16 arguments in row-major order. Passing fewer than 16 does not throw — missing trailing values become `NaN`, not an error. */
    constructor(
        m1: number, m2: number, m3: number, m4: number,
        m5: number, m6: number, m7: number, m8: number,
        m9: number, m10: number, m11: number, m12: number,
        m13: number, m14: number, m15: number, m16: number
    );
    /** No arguments means identity matrix. */
    constructor();

    /** Always 16. */
    readonly length: number;
    /** Flat element access, `mat[n]` for `n` in 0..15. Out-of-range indices are silently ignored (no throw). */
    [index: number]: number;

    toString(): string;
    toArray(): number[];
    /** Puts the first 16 numbers of `arr` as the matrix elements. Chainable. */
    fromArray(arr: ArrayLike<number>): this;
    /** Create a new clone of this matrix. */
    clone(): Matrix4;
    /** Copy `other`'s contents into this matrix. Chainable. */
    copy(other: Matrix4): this;
    /** Mutates in place. Chainable. */
    transpose(): this;
    /** Mutates in place. Chainable. */
    invert(): this;
    /** Mutates in place. Chainable. */
    identity(): this;
    /** Non-mutating matrix multiplication — returns a new Matrix4. */
    mul(other: Matrix4): Matrix4;
    equals(other: Matrix4): boolean;
}
