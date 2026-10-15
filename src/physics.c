#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <float.h>

#include <physics.h>
#include <vector.h>
#include <matrix.h>
#include <athena_math.h>

/* ============================================================================
 * PS2 VU0 / MMI / FPU Hardware-Accelerated Primitives
 * ============================================================================ */

/* PS2 newlib malloc/calloc only guarantees 8-byte alignment, whereas VU0
 * (lqc2/sqc2) and MMI (lq/sq) hardware-mask effective addresses with & ~0xF.
 * Every struct containing VECTOR or MATRIX fields must be 16-byte aligned. */
static inline void *phys_calloc_aligned(size_t size) {
    size_t aligned_size = (size + 15u) & ~15u;
    void *ptr = memalign(16, aligned_size);
    if (ptr) {
        memset(ptr, 0, aligned_size);
    }
    return ptr;
}

static inline void vu0_vec_zero(VECTOR dst) {
    __asm__ __volatile__(
        "sqc2 $vf0, 0x0(%0)\n"
        : : "r"(dst) : "memory"
    );
    dst[3] = 0.0f;
}

static inline void vu0_vec_copy(VECTOR dst, const VECTOR src) {
    __asm__ __volatile__(
        "lq $8, 0x0(%1)\n"
        "sq $8, 0x0(%0)\n"
        : : "r"(dst), "r"(src) : "$8", "memory"
    );
}

static inline void vu0_vec_add(VECTOR dst, const VECTOR a, const VECTOR b) {
    __asm__ __volatile__(
        "lqc2     $vf4, 0x0(%1)\n"
        "lqc2     $vf5, 0x0(%2)\n"
        "vadd.xyz $vf6, $vf4, $vf5\n"
        "sqc2     $vf6, 0x0(%0)\n"
        : : "r"(dst), "r"(a), "r"(b) : "memory"
    );
}

static inline void vu0_vec_sub(VECTOR dst, const VECTOR a, const VECTOR b) {
    __asm__ __volatile__(
        "lqc2     $vf4, 0x0(%1)\n"
        "lqc2     $vf5, 0x0(%2)\n"
        "vsub.xyz $vf6, $vf4, $vf5\n"
        "sqc2     $vf6, 0x0(%0)\n"
        : : "r"(dst), "r"(a), "r"(b) : "memory"
    );
}

static inline void vu0_vec_scale(VECTOR dst, const VECTOR a, float s) {
    __asm__ __volatile__(
        "lqc2      $vf4, 0x0(%1)\n"
        "mfc1      $8, %2\n"
        "qmtc2     $8, $vf5\n"
        "vmulx.xyz $vf6, $vf4, $vf5\n"
        "sqc2      $vf6, 0x0(%0)\n"
        : : "r"(dst), "r"(a), "f"(s) : "$8", "memory"
    );
}

static inline void vu0_vec_madd(VECTOR dst, const VECTOR a, const VECTOR b, float s) {
    __asm__ __volatile__(
        "lqc2       $vf4, 0x0(%1)\n"
        "lqc2       $vf5, 0x0(%2)\n"
        "mfc1       $8, %3\n"
        "qmtc2      $8, $vf6\n"
        "vmulaw.xyz $ACC, $vf4, $vf0\n"
        "vmaddx.xyz $vf7, $vf5, $vf6\n"
        "sqc2       $vf7, 0x0(%0)\n"
        : : "r"(dst), "r"(a), "r"(b), "f"(s) : "$8", "memory"
    );
}

static inline float vu0_vec_dot(const VECTOR a, const VECTOR b) {
    float ret;
    __asm__ __volatile__(
        "lqc2     $vf4, 0x0(%1)\n"
        "lqc2     $vf5, 0x0(%2)\n"
        "vmul.xyz $vf6, $vf4, $vf5\n"
        "vaddy.x  $vf6, $vf6, $vf6\n"
        "vaddz.x  $vf6, $vf6, $vf6\n"
        "qmfc2    $8,   $vf6\n"
        "mtc1     $8,   %0\n"
        : "=f"(ret) : "r"(a), "r"(b) : "$8", "memory"
    );
    return ret;
}

static inline void vu0_vec_cross(VECTOR dst, const VECTOR a, const VECTOR b) {
    __asm__ __volatile__(
        "lqc2         $vf4, 0x0(%1)\n"
        "lqc2         $vf5, 0x0(%2)\n"
        "vopmula.xyz  $ACC, $vf4, $vf5\n"
        "vopmsub.xyz  $vf6, $vf5, $vf4\n"
        "vsub.w       $vf6, $vf6, $vf6\n"
        "sqc2         $vf6, 0x0(%0)\n"
        : : "r"(dst), "r"(a), "r"(b) : "memory"
    );
}

static inline void vu0_vec_min_max(VECTOR out_min, VECTOR out_max, const VECTOR a, const VECTOR b) {
    __asm__ __volatile__(
        "lqc2      $vf4, 0x0(%2)\n"
        "lqc2      $vf5, 0x0(%3)\n"
        "vmini.xyz $vf6, $vf4, $vf5\n"
        "vmax.xyz  $vf7, $vf4, $vf5\n"
        "sqc2      $vf6, 0x0(%0)\n"
        "sqc2      $vf7, 0x0(%1)\n"
        : : "r"(out_min), "r"(out_max), "r"(a), "r"(b) : "memory"
    );
}

/* Returns 1 if AABBs overlap on all 3 axes, 0 otherwise */
static inline int vu0_aabb_overlap(const VECTOR a_min, const VECTOR a_max,
                                   const VECTOR b_min, const VECTOR b_max) {
    uint32_t mac;
    __asm__ __volatile__(
        "lqc2     $vf1, 0x0(%1)\n" /* a_min */
        "lqc2     $vf2, 0x0(%2)\n" /* a_max */
        "lqc2     $vf3, 0x0(%3)\n" /* b_min */
        "lqc2     $vf4, 0x0(%4)\n" /* b_max */
        "vsub.xyz $vf5, $vf2, $vf3\n" /* a_max - b_min */
        "vsub.xyz $vf6, $vf4, $vf1\n" /* b_max - a_min */
        "vmini.xyz $vf7, $vf5, $vf6\n" /* min(a_max - b_min, b_max - a_min) */
        "vnop\n"
        "vnop\n"
        "vnop\n"
        "vnop\n"
        "cfc2     %0, $vi17\n" /* MAC flags: sign bits of x,y,z are in bits 7,6,5 (0xE0) */
        : "=r"(mac)
        : "r"(a_min), "r"(a_max), "r"(b_min), "r"(b_max)
        : "memory"
    );
    return (mac & 0x00E0u) == 0;
}

static inline float vu0_vec_normalize_len(VECTOR dst, const VECTOR src) {
    float len_sq = vu0_vec_dot(src, src);
    if (len_sq > 1e-12f) {
        float len = sqrtf(len_sq);
        float inv = 1.0f / len;
        vu0_vec_scale(dst, src, inv);
        dst[3] = 0.0f;
        return len;
    }
    dst[0] = 0.0f; dst[1] = 1.0f; dst[2] = 0.0f; dst[3] = 0.0f;
    return 0.0f;
}

/* Transform 3D direction/point by 3x3 rotation matrix R (rows 0..2 of MATRIX): out = R * v */
static inline void vu0_mat3_mul_vec(VECTOR dst, const MATRIX R, const VECTOR v) {
    __asm__ __volatile__(
        "lqc2     $vf1, 0x00(%1)\n"
        "lqc2     $vf2, 0x10(%1)\n"
        "lqc2     $vf3, 0x20(%1)\n"
        "lqc2     $vf4, 0x00(%2)\n"
        "vmul.xyz $vf1, $vf1, $vf4\n"
        "vmul.xyz $vf2, $vf2, $vf4\n"
        "vmul.xyz $vf3, $vf3, $vf4\n"
        "vaddy.x  $vf1, $vf1, $vf1\n"
        "vaddz.x  $vf1, $vf1, $vf1\n"
        "vaddx.y  $vf2, $vf2, $vf2\n"
        "vaddz.y  $vf2, $vf2, $vf2\n"
        "vaddx.z  $vf3, $vf3, $vf3\n"
        "vaddy.z  $vf3, $vf3, $vf3\n"
        "vmove.y  $vf1, $vf2\n"
        "vmove.z  $vf1, $vf3\n"
        "vsub.w   $vf1, $vf1, $vf1\n"
        "sqc2     $vf1, 0x00(%0)\n"
        : : "r"(dst), "r"(R), "r"(v) : "memory"
    );
}

/* Transform 3D vector by transpose of 3x3 rotation matrix: out = R^T * v
 * Because R is stored row-major, R^T * v = v.x * row0 + v.y * row1 + v.z * row2,
 * which maps directly to VU0 broadcast multiply-add! */
static inline void vu0_mat3_mul_transpose_vec(VECTOR dst, const MATRIX R, const VECTOR v) {
    __asm__ __volatile__(
        "lqc2       $vf1, 0x00(%1)\n"
        "lqc2       $vf2, 0x10(%1)\n"
        "lqc2       $vf3, 0x20(%1)\n"
        "lqc2       $vf4, 0x00(%2)\n"
        "vmulax.xyz $ACC, $vf1, $vf4\n"
        "vmadday.xyz $ACC, $vf2, $vf4\n"
        "vmaddz.xyz $vf5, $vf3, $vf4\n"
        "vsub.w     $vf5, $vf5, $vf5\n"
        "sqc2       $vf5, 0x00(%0)\n"
        : : "r"(dst), "r"(R), "r"(v) : "memory"
    );
}

/* Compute world inertia tensor: I_world = R * diag(I_local) * R^T */
static void update_body_inertia_world(AthenaPhysicsBody *b) {
    const float *R = b->rotation;
    float ix = b->inv_inertia_local[0];
    float iy = b->inv_inertia_local[1];
    float iz = b->inv_inertia_local[2];

    /* C = R * diag(ix, iy, iz) */
    float c00 = R[0] * ix, c01 = R[1] * iy, c02 = R[2]  * iz;
    float c10 = R[4] * ix, c11 = R[5] * iy, c12 = R[6]  * iz;
    float c20 = R[8] * ix, c21 = R[9] * iy, c22 = R[10] * iz;

    float *Iw = b->inv_inertia_world;
    Iw[0]  = c00 * R[0] + c01 * R[1] + c02 * R[2];
    Iw[1]  = c00 * R[4] + c01 * R[5] + c02 * R[6];
    Iw[2]  = c00 * R[8] + c01 * R[9] + c02 * R[10];
    Iw[3]  = 0.0f;

    Iw[4]  = Iw[1];
    Iw[5]  = c10 * R[4] + c11 * R[5] + c12 * R[6];
    Iw[6]  = c10 * R[8] + c11 * R[9] + c12 * R[10];
    Iw[7]  = 0.0f;

    Iw[8]  = Iw[2];
    Iw[9]  = Iw[6];
    Iw[10] = c20 * R[8] + c21 * R[9] + c22 * R[10];
    Iw[11] = 0.0f;

    Iw[12] = 0.0f; Iw[13] = 0.0f; Iw[14] = 0.0f; Iw[15] = 1.0f;
}

static void quat_to_rot_matrix(MATRIX R, const VECTOR q) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    R[0]  = 1.0f - (yy + zz); R[1]  = xy - wz;          R[2]  = xz + wy;          R[3]  = 0.0f;
    R[4]  = xy + wz;          R[5]  = 1.0f - (xx + zz); R[6]  = yz - wx;          R[7]  = 0.0f;
    R[8]  = xz - wy;          R[9]  = yz + wx;          R[10] = 1.0f - (xx + yy); R[11] = 0.0f;
    R[12] = 0.0f;             R[13] = 0.0f;             R[14] = 0.0f;             R[15] = 1.0f;
}

static void rot_matrix_to_quat(VECTOR q, const MATRIX R) {
    float tr = R[0] + R[5] + R[10];
    if (tr > 0.0f) {
        float s = sqrtf(tr + 1.0f) * 2.0f;
        q[3] = 0.25f * s;
        q[0] = (R[9] - R[6]) / s;
        q[1] = (R[2] - R[8]) / s;
        q[2] = (R[4] - R[1]) / s;
    } else if ((R[0] > R[5]) && (R[0] > R[10])) {
        float s = sqrtf(1.0f + R[0] - R[5] - R[10]) * 2.0f;
        q[3] = (R[9] - R[6]) / s;
        q[0] = 0.25f * s;
        q[1] = (R[1] + R[4]) / s;
        q[2] = (R[2] + R[8]) / s;
    } else if (R[5] > R[10]) {
        float s = sqrtf(1.0f + R[5] - R[0] - R[10]) * 2.0f;
        q[3] = (R[2] - R[8]) / s;
        q[0] = (R[1] + R[4]) / s;
        q[1] = 0.25f * s;
        q[2] = (R[6] + R[9]) / s;
    } else {
        float s = sqrtf(1.0f + R[10] - R[0] - R[5]) * 2.0f;
        q[3] = (R[4] - R[1]) / s;
        q[0] = (R[2] + R[8]) / s;
        q[1] = (R[6] + R[9]) / s;
        q[2] = 0.25f * s;
    }
    float len = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (len > 1e-8f) {
        float inv = 1.0f / len;
        q[0] *= inv; q[1] *= inv; q[2] *= inv; q[3] *= inv;
    } else {
        q[0] = 0.0f; q[1] = 0.0f; q[2] = 0.0f; q[3] = 1.0f;
    }
}

static void quat_mul(VECTOR out, const VECTOR a, const VECTOR b) {
    float ax = a[0], ay = a[1], az = a[2], aw = a[3];
    float bx = b[0], by = b[1], bz = b[2], bw = b[3];
    out[0] = aw * bx + ax * bw + ay * bz - az * by;
    out[1] = aw * by - ax * bz + ay * bw + az * bx;
    out[2] = aw * bz + ax * by - ay * bx + az * bw;
    out[3] = aw * bw - ax * bx - ay * by - az * bz;
}

static void quat_conjugate(VECTOR out, const VECTOR q) {
    out[0] = -q[0]; out[1] = -q[1]; out[2] = -q[2]; out[3] = q[3];
}

/* Multiply two 3x3 rotation matrices padded in 4x4 MATRIX using VU0 */
static void mat3_mul(MATRIX out, const MATRIX A, const MATRIX B) {
    __asm__ __volatile__(
        "lqc2        $vf1, 0x00(%2)\n"
        "lqc2        $vf2, 0x10(%2)\n"
        "lqc2        $vf3, 0x20(%2)\n"
        "lqc2        $vf4, 0x00(%1)\n"
        "lqc2        $vf5, 0x10(%1)\n"
        "lqc2        $vf6, 0x20(%1)\n"
        "vmulax.xyz  $ACC, $vf1, $vf4\n"
        "vmadday.xyz $ACC, $vf2, $vf4\n"
        "vmaddz.xyz  $vf7, $vf3, $vf4\n"
        "vsub.w      $vf7, $vf7, $vf7\n"
        "vmulax.xyz  $ACC, $vf1, $vf5\n"
        "vmadday.xyz $ACC, $vf2, $vf5\n"
        "vmaddz.xyz  $vf8, $vf3, $vf5\n"
        "vsub.w      $vf8, $vf8, $vf8\n"
        "vmulax.xyz  $ACC, $vf1, $vf6\n"
        "vmadday.xyz $ACC, $vf2, $vf6\n"
        "vmaddz.xyz  $vf9, $vf3, $vf6\n"
        "vsub.w      $vf9, $vf9, $vf9\n"
        "sqc2        $vf7, 0x00(%0)\n"
        "sqc2        $vf8, 0x10(%0)\n"
        "sqc2        $vf9, 0x20(%0)\n"
        "sqc2        $vf0, 0x30(%0)\n"
        : : "r"(out), "r"(A), "r"(B) : "memory"
    );
}

static inline void vu0_mat_identity(MATRIX m) {
    memset(m, 0, sizeof(MATRIX));
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static inline void vu0_vec_expand_bounds(VECTOR bmin, VECTOR bmax, const VECTOR pt) {
    __asm__ __volatile__(
        "lqc2      $vf4, 0x0(%0)\n"
        "lqc2      $vf5, 0x0(%1)\n"
        "lqc2      $vf6, 0x0(%2)\n"
        "vmini.xyz $vf4, $vf4, $vf6\n"
        "vmax.xyz  $vf5, $vf5, $vf6\n"
        "sqc2      $vf4, 0x0(%0)\n"
        "sqc2      $vf5, 0x0(%1)\n"
        : : "r"(bmin), "r"(bmax), "r"(pt) : "memory"
    );
}

/* ============================================================================
 * Geometry Pose & AABB Computation
 * ============================================================================ */

static void geom_get_world_pose(const AthenaPhysicsGeom *g, VECTOR out_pos, MATRIX out_rot) {
    if (g->body) {
        vu0_vec_copy(out_pos, g->body->position);
        memcpy(out_rot, g->body->rotation, sizeof(MATRIX));
    } else {
        vu0_vec_copy(out_pos, g->position);
        memcpy(out_rot, g->rotation, sizeof(MATRIX));
    }
}

static void geom_compute_aabb(AthenaPhysicsGeom *g) {
    VECTOR pos;
    MATRIX rot;
    geom_get_world_pose(g, pos, rot);

    switch (g->type) {
        case ATH_GEOM_SPHERE: {
            float r = g->sphere.radius;
            g->aabb_min[0] = pos[0] - r;
            g->aabb_min[1] = pos[1] - r;
            g->aabb_min[2] = pos[2] - r;
            g->aabb_min[3] = 0.0f;
            g->aabb_max[0] = pos[0] + r;
            g->aabb_max[1] = pos[1] + r;
            g->aabb_max[2] = pos[2] + r;
            g->aabb_max[3] = 0.0f;
            break;
        }
        case ATH_GEOM_BOX: {
            float hx = g->box.half_extents[0];
            float hy = g->box.half_extents[1];
            float hz = g->box.half_extents[2];
            float ex = fabsf(rot[0]) * hx + fabsf(rot[1]) * hy + fabsf(rot[2])  * hz;
            float ey = fabsf(rot[4]) * hx + fabsf(rot[5]) * hy + fabsf(rot[6])  * hz;
            float ez = fabsf(rot[8]) * hx + fabsf(rot[9]) * hy + fabsf(rot[10]) * hz;
            g->aabb_min[0] = pos[0] - ex;
            g->aabb_min[1] = pos[1] - ey;
            g->aabb_min[2] = pos[2] - ez;
            g->aabb_min[3] = 0.0f;
            g->aabb_max[0] = pos[0] + ex;
            g->aabb_max[1] = pos[1] + ey;
            g->aabb_max[2] = pos[2] + ez;
            g->aabb_max[3] = 0.0f;
            break;
        }
        case ATH_GEOM_PLANE: {
            g->aabb_min[0] = -1e20f; g->aabb_min[1] = -1e20f; g->aabb_min[2] = -1e20f; g->aabb_min[3] = 0.0f;
            g->aabb_max[0] =  1e20f; g->aabb_max[1] =  1e20f; g->aabb_max[2] =  1e20f; g->aabb_max[3] = 0.0f;
            break;
        }
        case ATH_GEOM_RAY: {
            float len = g->ray.length;
            VECTOR end_pt;
            end_pt[0] = pos[0] + g->ray.dir[0] * len;
            end_pt[1] = pos[1] + g->ray.dir[1] * len;
            end_pt[2] = pos[2] + g->ray.dir[2] * len;
            end_pt[3] = 0.0f;
            vu0_vec_min_max(g->aabb_min, g->aabb_max, pos, end_pt);
            g->aabb_min[0] -= 0.001f; g->aabb_min[1] -= 0.001f; g->aabb_min[2] -= 0.001f;
            g->aabb_max[0] += 0.001f; g->aabb_max[1] += 0.001f; g->aabb_max[2] += 0.001f;
            break;
        }
        case ATH_GEOM_TRIMESH: {
            AthenaPhysicsTriMesh *tm = g->trimesh;
            if (!tm) {
                vu0_vec_zero(g->aabb_min);
                vu0_vec_zero(g->aabb_max);
                break;
            }
            VECTOR sx_min, sx_max;
            for (int i = 0; i < 3; i++) {
                float s = g->scale[i];
                float a = tm->local_min[i] * s;
                float b = tm->local_max[i] * s;
                sx_min[i] = (a < b) ? a : b;
                sx_max[i] = (a > b) ? a : b;
            }
            VECTOR center, half;
            for (int i = 0; i < 3; i++) {
                center[i] = 0.5f * (sx_min[i] + sx_max[i]);
                half[i]   = 0.5f * (sx_max[i] - sx_min[i]);
            }
            center[3] = 0.0f;
            VECTOR wc;
            vu0_mat3_mul_vec(wc, rot, center);
            vu0_vec_add(wc, wc, pos);
            float ex = fabsf(rot[0]) * half[0] + fabsf(rot[1]) * half[1] + fabsf(rot[2])  * half[2];
            float ey = fabsf(rot[4]) * half[0] + fabsf(rot[5]) * half[1] + fabsf(rot[6])  * half[2];
            float ez = fabsf(rot[8]) * half[0] + fabsf(rot[9]) * half[1] + fabsf(rot[10]) * half[2];
            g->aabb_min[0] = wc[0] - ex;
            g->aabb_min[1] = wc[1] - ey;
            g->aabb_min[2] = wc[2] - ez;
            g->aabb_min[3] = 0.0f;
            g->aabb_max[0] = wc[0] + ex;
            g->aabb_max[1] = wc[1] + ey;
            g->aabb_max[2] = wc[2] + ez;
            g->aabb_max[3] = 0.0f;
            break;
        }
        case ATH_GEOM_TRANSFORM: {
            AthenaPhysicsGeom *child = g->transform.child;
            if (!child) {
                vu0_vec_zero(g->aabb_min);
                vu0_vec_zero(g->aabb_max);
                break;
            }
            /* Temporarily place child in world space to compute its AABB */
            VECTOR saved_pos;
            MATRIX saved_rot;
            AthenaPhysicsBody *saved_body = child->body;
            vu0_vec_copy(saved_pos, child->position);
            memcpy(saved_rot, child->rotation, sizeof(MATRIX));

            VECTOR world_offset;
            vu0_mat3_mul_vec(world_offset, rot, saved_pos);
            vu0_vec_add(child->position, pos, world_offset);
            mat3_mul(child->rotation, rot, saved_rot);
            child->body = NULL;

            geom_compute_aabb(child);
            vu0_vec_copy(g->aabb_min, child->aabb_min);
            vu0_vec_copy(g->aabb_max, child->aabb_max);

            vu0_vec_copy(child->position, saved_pos);
            memcpy(child->rotation, saved_rot, sizeof(MATRIX));
            child->body = saved_body;
            break;
        }
        case ATH_GEOM_SPACE: {
            AthenaPhysicsSpace *sub = g->subspace;
            if (!sub || !sub->geoms) {
                vu0_vec_zero(g->aabb_min);
                vu0_vec_zero(g->aabb_max);
                break;
            }
            g->aabb_min[0] =  1e20f; g->aabb_min[1] =  1e20f; g->aabb_min[2] =  1e20f; g->aabb_min[3] = 0.0f;
            g->aabb_max[0] = -1e20f; g->aabb_max[1] = -1e20f; g->aabb_max[2] = -1e20f; g->aabb_max[3] = 0.0f;
            for (AthenaPhysicsGeom *sg = sub->geoms; sg; sg = sg->next) {
                geom_compute_aabb(sg);
                vu0_vec_expand_bounds(g->aabb_min, g->aabb_max, sg->aabb_min);
                vu0_vec_expand_bounds(g->aabb_min, g->aabb_max, sg->aabb_max);
            }
            break;
        }
    }
    g->aabb_dirty = 0;
}

/* ============================================================================
 * BVH Builder for TriMesh (Top-down Median Split, 32-byte LQC2 Nodes)
 * ============================================================================ */

static inline uint32_t pack_u32_as_bits(uint32_t v) { return v; }

static inline void bvh_set_node_meta(AthenaBVHNode *node, uint32_t left_or_first, uint32_t count) {
    memcpy(&node->aabb_min[3], &left_or_first, sizeof(uint32_t));
    memcpy(&node->aabb_max[3], &count, sizeof(uint32_t));
}

static inline uint32_t bvh_node_get_first(const AthenaBVHNode *node) {
    uint32_t v;
    memcpy(&v, &node->aabb_min[3], sizeof(uint32_t));
    return v;
}

static inline uint32_t bvh_node_get_count(const AthenaBVHNode *node) {
    uint32_t v;
    memcpy(&v, &node->aabb_max[3], sizeof(uint32_t));
    return v;
}

static void bvh_build_recursive(AthenaPhysicsTriMesh *tm, VECTOR *centroids,
                                uint32_t node_idx, uint32_t first, uint32_t count,
                                uint32_t *next_free_node, uint32_t max_nodes) {
    AthenaBVHNode *node = &tm->nodes[node_idx];
    VECTOR bmin = {  1e30f,  1e30f,  1e30f, 0.0f };
    VECTOR bmax = { -1e30f, -1e30f, -1e30f, 0.0f };

    for (uint32_t i = 0; i < count; i++) {
        const AthenaTri *t = &tm->tris[first + i];
        VECTOR v0, v1, v2;
        vu0_vec_copy(v0, t->v0);
        vu0_vec_add(v1, t->v0, t->e1);
        vu0_vec_add(v2, t->v0, t->e2);
        for (int a = 0; a < 3; a++) {
            float lo = fminf(v0[a], fminf(v1[a], v2[a]));
            float hi = fmaxf(v0[a], fmaxf(v1[a], v2[a]));
            if (lo < bmin[a]) bmin[a] = lo;
            if (hi > bmax[a]) bmax[a] = hi;
        }
    }

    node->aabb_min[0] = bmin[0] - 1e-4f;
    node->aabb_min[1] = bmin[1] - 1e-4f;
    node->aabb_min[2] = bmin[2] - 1e-4f;
    node->aabb_max[0] = bmax[0] + 1e-4f;
    node->aabb_max[1] = bmax[1] + 1e-4f;
    node->aabb_max[2] = bmax[2] + 1e-4f;

    if (count <= 4 || (*next_free_node + 2) > max_nodes) {
        bvh_set_node_meta(node, first, count);
        return;
    }

    /* Choose longest axis of centroid bounds */
    VECTOR cmin = {  1e30f,  1e30f,  1e30f, 0.0f };
    VECTOR cmax = { -1e30f, -1e30f, -1e30f, 0.0f };
    for (uint32_t i = 0; i < count; i++) {
        const float *c = centroids[first + i];
        for (int a = 0; a < 3; a++) {
            if (c[a] < cmin[a]) cmin[a] = c[a];
            if (c[a] > cmax[a]) cmax[a] = c[a];
        }
    }

    int axis = 0;
    float ext0 = cmax[0] - cmin[0];
    float ext1 = cmax[1] - cmin[1];
    float ext2 = cmax[2] - cmin[2];
    if (ext1 > ext0 && ext1 >= ext2) axis = 1;
    else if (ext2 > ext0 && ext2 > ext1) axis = 2;

    float split_pos = 0.5f * (cmin[axis] + cmax[axis]);

    /* Partition triangles in-place around split_pos */
    uint32_t i = first;
    uint32_t j = first + count - 1;
    while (i <= j) {
        if (centroids[i][axis] < split_pos) {
            i++;
        } else {
            AthenaTri tmp_t = tm->tris[i];
            tm->tris[i] = tm->tris[j];
            tm->tris[j] = tmp_t;

            VECTOR tmp_c;
            vu0_vec_copy(tmp_c, centroids[i]);
            vu0_vec_copy(centroids[i], centroids[j]);
            vu0_vec_copy(centroids[j], tmp_c);

            if (j == 0) break;
            j--;
        }
    }

    uint32_t left_count = i - first;
    if (left_count == 0 || left_count == count) {
        left_count = count / 2;
    }

    uint32_t left_child = *next_free_node;
    *next_free_node += 2;
    bvh_set_node_meta(node, left_child, 0); /* count == 0 marks internal node */

    bvh_build_recursive(tm, centroids, left_child,     first,              left_count,         next_free_node, max_nodes);
    bvh_build_recursive(tm, centroids, left_child + 1, first + left_count, count - left_count, next_free_node, max_nodes);
}

static AthenaPhysicsTriMesh *trimesh_build(const VECTOR *verts, uint32_t vert_count,
                                           const uint32_t *indices, uint32_t index_count) {
    uint32_t tri_count = index_count / 3;
    if (tri_count == 0 || !verts)
        return NULL;

    AthenaPhysicsTriMesh *tm = (AthenaPhysicsTriMesh *)memalign(16, sizeof(AthenaPhysicsTriMesh));
    if (!tm) return NULL;
    memset(tm, 0, sizeof(*tm));

    tm->tris = (AthenaTri *)memalign(16, sizeof(AthenaTri) * tri_count);
    uint32_t max_nodes = tri_count * 2 + 1;
    tm->nodes = (AthenaBVHNode *)memalign(16, sizeof(AthenaBVHNode) * max_nodes);
    VECTOR *centroids = (VECTOR *)memalign(16, sizeof(VECTOR) * tri_count);

    if (!tm->tris || !tm->nodes || !centroids) {
        free(tm->tris);
        free(tm->nodes);
        free(centroids);
        free(tm);
        return NULL;
    }

    tm->local_min[0] =  1e30f; tm->local_min[1] =  1e30f; tm->local_min[2] =  1e30f; tm->local_min[3] = 0.0f;
    tm->local_max[0] = -1e30f; tm->local_max[1] = -1e30f; tm->local_max[2] = -1e30f; tm->local_max[3] = 0.0f;

    uint32_t valid_tris = 0;
    for (uint32_t i = 0; i < tri_count; i++) {
        uint32_t i0 = indices ? indices[i * 3 + 0] : (i * 3 + 0);
        uint32_t i1 = indices ? indices[i * 3 + 1] : (i * 3 + 1);
        uint32_t i2 = indices ? indices[i * 3 + 2] : (i * 3 + 2);
        if (i0 >= vert_count || i1 >= vert_count || i2 >= vert_count)
            continue;

        const float *p0 = verts[i0];
        const float *p1 = verts[i1];
        const float *p2 = verts[i2];

        AthenaTri *t = &tm->tris[valid_tris];
        t->v0[0] = p0[0]; t->v0[1] = p0[1]; t->v0[2] = p0[2]; t->v0[3] = 0.0f;
        t->e1[0] = p1[0] - p0[0]; t->e1[1] = p1[1] - p0[1]; t->e1[2] = p1[2] - p0[2]; t->e1[3] = 0.0f;
        t->e2[0] = p2[0] - p0[0]; t->e2[1] = p2[1] - p0[1]; t->e2[2] = p2[2] - p0[2]; t->e2[3] = 0.0f;

        vu0_vec_cross(t->normal, t->e1, t->e2);
        float len = vu0_vec_normalize_len(t->normal, t->normal);
        if (len < 1e-7f)
            continue; /* Skip degenerate zero-area triangles */
        t->normal[3] = vu0_vec_dot(t->normal, t->v0);

        centroids[valid_tris][0] = (p0[0] + p1[0] + p2[0]) * (1.0f / 3.0f);
        centroids[valid_tris][1] = (p0[1] + p1[1] + p2[1]) * (1.0f / 3.0f);
        centroids[valid_tris][2] = (p0[2] + p1[2] + p2[2]) * (1.0f / 3.0f);
        centroids[valid_tris][3] = 0.0f;

        for (int a = 0; a < 3; a++) {
            float lo = fminf(p0[a], fminf(p1[a], p2[a]));
            float hi = fmaxf(p0[a], fmaxf(p1[a], p2[a]));
            if (lo < tm->local_min[a]) tm->local_min[a] = lo;
            if (hi > tm->local_max[a]) tm->local_max[a] = hi;
        }

        valid_tris++;
    }

    if (valid_tris == 0) {
        free(tm->tris);
        free(tm->nodes);
        free(centroids);
        free(tm);
        return NULL;
    }

    tm->tri_count = valid_tris;
    uint32_t next_free_node = 1;
    bvh_build_recursive(tm, centroids, 0, 0, valid_tris, &next_free_node, max_nodes);
    tm->node_count = next_free_node;

    free(centroids);
    return tm;
}

/* ============================================================================
 * Narrowphase Collision Detection
 * ============================================================================ */

static inline void init_contact_defaults(AthenaContact *c, AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2) {
    c->mu = 0.5f;
    c->bounce = 0.1f;
    c->soft_cfm = 0.01f;
    c->g1 = g1;
    c->g2 = g2;
    c->tri_index = 0;
    c->_pad = 0;
}

static inline void flip_contacts(AthenaContact *contacts, int n, AthenaPhysicsGeom *orig_g1, AthenaPhysicsGeom *orig_g2) {
    for (int i = 0; i < n; i++) {
        contacts[i].normal[0] = -contacts[i].normal[0];
        contacts[i].normal[1] = -contacts[i].normal[1];
        contacts[i].normal[2] = -contacts[i].normal[2];
        contacts[i].g1 = orig_g1;
        contacts[i].g2 = orig_g2;
    }
}

/* Closest point on triangle (v0, v0+e1, v0+e2) to point p using VU0 vector math */
static void closest_point_on_triangle(VECTOR out, const VECTOR p, const AthenaTri *tri) {
    VECTOR diff;
    vu0_vec_sub(diff, p, tri->v0);

    float d1 = vu0_vec_dot(tri->e1, diff);
    float d2 = vu0_vec_dot(tri->e2, diff);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        vu0_vec_copy(out, tri->v0);
        return;
    }

    VECTOR v1, diff1;
    vu0_vec_add(v1, tri->v0, tri->e1);
    vu0_vec_sub(diff1, p, v1);
    float d3 = vu0_vec_dot(tri->e1, diff1);
    float d4 = vu0_vec_dot(tri->e2, diff1);
    if (d3 >= 0.0f && d4 <= d3) {
        vu0_vec_copy(out, v1);
        return;
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = (d1 - d3 > 1e-8f) ? (d1 / (d1 - d3)) : 0.0f;
        vu0_vec_madd(out, tri->v0, tri->e1, v);
        return;
    }

    VECTOR v2, diff2;
    vu0_vec_add(v2, tri->v0, tri->e2);
    vu0_vec_sub(diff2, p, v2);
    float d5 = vu0_vec_dot(tri->e1, diff2);
    float d6 = vu0_vec_dot(tri->e2, diff2);
    if (d6 >= 0.0f && d5 <= d6) {
        vu0_vec_copy(out, v2);
        return;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = (d2 - d6 > 1e-8f) ? (d2 / (d2 - d6)) : 0.0f;
        vu0_vec_madd(out, tri->v0, tri->e2, w);
        return;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float denom = (d4 - d3) + (d5 - d6);
        float w = (denom > 1e-8f) ? ((d4 - d3) / denom) : 0.0f;
        VECTOR e12;
        vu0_vec_sub(e12, v2, v1);
        vu0_vec_madd(out, v1, e12, w);
        return;
    }

    float denom_sum = va + vb + vc;
    if (fabsf(denom_sum) > 1e-8f) {
        float inv_denom = 1.0f / denom_sum;
        float v = vb * inv_denom;
        float w = vc * inv_denom;
        vu0_vec_madd(out, tri->v0, tri->e1, v);
        vu0_vec_madd(out, out, tri->e2, w);
    } else {
        vu0_vec_copy(out, tri->v0);
    }
}

/* 1. Sphere vs Sphere (normal points from g2 to g1) */
static int collide_sphere_sphere(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                 AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR p1, p2, diff;
    MATRIX r1, r2;
    geom_get_world_pose(g1, p1, r1);
    geom_get_world_pose(g2, p2, r2);

    float rad1 = g1->sphere.radius;
    float rad2 = g2->sphere.radius;
    float r_sum = rad1 + rad2;

    vu0_vec_sub(diff, p1, p2);
    float dist_sq = vu0_vec_dot(diff, diff);
    if (dist_sq > r_sum * r_sum)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g1, g2);

    float dist = sqrtf(dist_sq);
    if (dist > 1e-6f) {
        vu0_vec_scale(c->normal, diff, 1.0f / dist);
    } else {
        c->normal[0] = 0.0f; c->normal[1] = 1.0f; c->normal[2] = 0.0f; c->normal[3] = 0.0f;
    }
    c->depth = r_sum - dist;
    vu0_vec_madd(c->pos, p2, c->normal, rad2 - 0.5f * c->depth);
    return 1;
}

/* 2. Sphere (g1) vs Plane (g2) (normal points out of plane into sphere) */
static int collide_sphere_plane(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR p1;
    MATRIX r1;
    geom_get_world_pose(g1, p1, r1);

    float rad = g1->sphere.radius;
    float dist = vu0_vec_dot(p1, g2->plane.normal) - g2->plane.normal[3];
    float depth = rad - dist;
    if (depth < 0.0f)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g1, g2);
    c->normal[0] = g2->plane.normal[0];
    c->normal[1] = g2->plane.normal[1];
    c->normal[2] = g2->plane.normal[2];
    c->normal[3] = 0.0f;
    c->depth = depth;
    vu0_vec_madd(c->pos, p1, c->normal, -(rad - 0.5f * depth));
    return 1;
}

/* 3. Box (g1) vs Plane (g2) */
static int collide_box_plane(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                             AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR p1;
    MATRIX r1;
    geom_get_world_pose(g1, p1, r1);

    float hx = g1->box.half_extents[0];
    float hy = g1->box.half_extents[1];
    float hz = g1->box.half_extents[2];
    const float *pn = g2->plane.normal;
    float pd = pn[3];

    int count = 0;
    for (int i = 0; i < 8; i++) {
        VECTOR local_c, world_c;
        local_c[0] = (i & 1) ? hx : -hx;
        local_c[1] = (i & 2) ? hy : -hy;
        local_c[2] = (i & 4) ? hz : -hz;
        local_c[3] = 0.0f;

        vu0_mat3_mul_vec(world_c, r1, local_c);
        vu0_vec_add(world_c, p1, world_c);

        float dist = vu0_vec_dot(world_c, pn) - pd;
        if (dist <= 0.0f) {
            float depth = -dist;
            int slot = count;
            if (count < max_contacts) {
                count++;
            } else {
                /* Replace shallowest contact if this corner penetrates deeper */
                int min_idx = 0;
                for (int k = 1; k < max_contacts; k++) {
                    if (contacts[k].depth < contacts[min_idx].depth)
                        min_idx = k;
                }
                if (depth <= contacts[min_idx].depth)
                    continue;
                slot = min_idx;
            }
            AthenaContact *c = &contacts[slot];
            init_contact_defaults(c, g1, g2);
            c->normal[0] = pn[0]; c->normal[1] = pn[1]; c->normal[2] = pn[2]; c->normal[3] = 0.0f;
            c->depth = depth;
            vu0_vec_madd(c->pos, world_c, c->normal, 0.5f * depth);
        }
    }
    return count;
}

/* 4. Sphere (g1) vs Box (g2) */
static int collide_sphere_box(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                              AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR p1, p2, diff, local_p;
    MATRIX r1, r2;
    geom_get_world_pose(g1, p1, r1);
    geom_get_world_pose(g2, p2, r2);

    vu0_vec_sub(diff, p1, p2);
    vu0_mat3_mul_transpose_vec(local_p, r2, diff);

    float hx = g2->box.half_extents[0];
    float hy = g2->box.half_extents[1];
    float hz = g2->box.half_extents[2];
    float rad = g1->sphere.radius;

    if (fabsf(local_p[0]) - rad > hx ||
        fabsf(local_p[1]) - rad > hy ||
        fabsf(local_p[2]) - rad > hz) {
        return 0;
    }

    VECTOR closest_local;
    closest_local[0] = fmaxf(-hx, fminf(hx, local_p[0]));
    closest_local[1] = fmaxf(-hy, fminf(hy, local_p[1]));
    closest_local[2] = fmaxf(-hz, fminf(hz, local_p[2]));
    closest_local[3] = 0.0f;

    VECTOR delta_local;
    vu0_vec_sub(delta_local, local_p, closest_local);
    float dist_sq = vu0_vec_dot(delta_local, delta_local);
    if (dist_sq > rad * rad)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g1, g2);

    VECTOR normal_local;
    if (dist_sq > 1e-10f) {
        float dist = sqrtf(dist_sq);
        vu0_vec_scale(normal_local, delta_local, 1.0f / dist);
        c->depth = rad - dist;
    } else {
        /* Sphere center is inside box: find closest box face */
        float dx = hx - fabsf(local_p[0]);
        float dy = hy - fabsf(local_p[1]);
        float dz = hz - fabsf(local_p[2]);
        vu0_vec_zero(normal_local);
        if (dx <= dy && dx <= dz) {
            normal_local[0] = (local_p[0] >= 0.0f) ? 1.0f : -1.0f;
            closest_local[0] = (local_p[0] >= 0.0f) ? hx : -hx;
            c->depth = dx + rad;
        } else if (dy <= dz) {
            normal_local[1] = (local_p[1] >= 0.0f) ? 1.0f : -1.0f;
            closest_local[1] = (local_p[1] >= 0.0f) ? hy : -hy;
            c->depth = dy + rad;
        } else {
            normal_local[2] = (local_p[2] >= 0.0f) ? 1.0f : -1.0f;
            closest_local[2] = (local_p[2] >= 0.0f) ? hz : -hz;
            c->depth = dz + rad;
        }
    }

    vu0_mat3_mul_vec(c->normal, r2, normal_local);
    VECTOR world_closest;
    vu0_mat3_mul_vec(world_closest, r2, closest_local);
    vu0_vec_add(c->pos, p2, world_closest);
    return 1;
}

/* Helper for Box-Box Sutherland-Hodgman polygon clipping against a 2D half-plane */
static int clip_poly_against_axis(const VECTOR *in_pts, int in_count,
                                  VECTOR *out_pts, int axis, float sign, float limit) {
    int out_count = 0;
    if (in_count <= 0) return 0;
    for (int i = 0; i < in_count; i++) {
        const float *p1 = in_pts[i];
        const float *p2 = in_pts[(i + 1) % in_count];
        float d1 = sign * p1[axis] - limit;
        float d2 = sign * p2[axis] - limit;
        int in1 = (d1 <= 0.0f);
        int in2 = (d2 <= 0.0f);

        if (in1 && in2) {
            vu0_vec_copy(out_pts[out_count++], p2);
        } else if (in1 && !in2) {
            float t = d1 / (d1 - d2);
            out_pts[out_count][0] = p1[0] + t * (p2[0] - p1[0]);
            out_pts[out_count][1] = p1[1] + t * (p2[1] - p1[1]);
            out_pts[out_count][2] = p1[2] + t * (p2[2] - p1[2]);
            out_pts[out_count][3] = 0.0f;
            out_count++;
        } else if (!in1 && in2) {
            float t = d1 / (d1 - d2);
            out_pts[out_count][0] = p1[0] + t * (p2[0] - p1[0]);
            out_pts[out_count][1] = p1[1] + t * (p2[1] - p1[1]);
            out_pts[out_count][2] = p1[2] + t * (p2[2] - p1[2]);
            out_pts[out_count][3] = 0.0f;
            out_count++;
            vu0_vec_copy(out_pts[out_count++], p2);
        }
    }
    return out_count;
}

/* 5. Box (g1) vs Box (g2) — Full 15-axis OBB SAT + Manifold Clipping */
static int collide_box_box(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                           AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR p1, p2, d;
    MATRIX R1, R2;
    geom_get_world_pose(g1, p1, R1);
    geom_get_world_pose(g2, p2, R2);

    /* Extract world axes of Box 1 (u) and Box 2 (v) from columns of R1 and R2 */
    VECTOR u[3], v[3];
    for (int i = 0; i < 3; i++) {
        u[i][0] = R1[0 + i]; u[i][1] = R1[4 + i]; u[i][2] = R1[8 + i]; u[i][3] = 0.0f;
        v[i][0] = R2[0 + i]; v[i][1] = R2[4 + i]; v[i][2] = R2[8 + i]; v[i][3] = 0.0f;
    }

    const float *e1 = g1->box.half_extents;
    const float *e2 = g2->box.half_extents;

    /* Vector from Box 2 to Box 1 (consistent with normal pointing from g2 to g1) */
    vu0_vec_sub(d, p1, p2);

    float C[3][3], absC[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            C[i][j] = vu0_vec_dot(u[i], v[j]);
            absC[i][j] = fabsf(C[i][j]) + 1e-6f;
        }
    }

    float min_pen = 1e30f;
    int best_axis = -1;
    VECTOR best_normal = { 0.0f, 1.0f, 0.0f, 0.0f };

    /* Test 3 axes of Box 1 */
    float du[3];
    for (int i = 0; i < 3; i++) {
        du[i] = vu0_vec_dot(d, u[i]);
        float rA = e1[i];
        float rB = e2[0] * absC[i][0] + e2[1] * absC[i][1] + e2[2] * absC[i][2];
        float pen = (rA + rB) - fabsf(du[i]);
        if (pen < 0.0f) return 0;
        if (pen < min_pen) {
            min_pen = pen;
            best_axis = i;
            float s = (du[i] >= 0.0f) ? 1.0f : -1.0f;
            vu0_vec_scale(best_normal, u[i], s);
        }
    }

    /* Test 3 axes of Box 2 */
    float dv[3];
    for (int j = 0; j < 3; j++) {
        dv[j] = vu0_vec_dot(d, v[j]);
        float rA = e1[0] * absC[0][j] + e1[1] * absC[1][j] + e1[2] * absC[2][j];
        float rB = e2[j];
        float pen = (rA + rB) - fabsf(dv[j]);
        if (pen < 0.0f) return 0;
        if (pen < min_pen) {
            min_pen = pen;
            best_axis = 3 + j;
            float s = (dv[j] >= 0.0f) ? 1.0f : -1.0f;
            vu0_vec_scale(best_normal, v[j], s);
        }
    }

    /* Test 9 edge-edge cross product axes (favor face axes slightly by 0.95 factor for stability) */
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            VECTOR axis;
            vu0_vec_cross(axis, u[i], v[j]);
            float len = vu0_vec_normalize_len(axis, axis);
            if (len < 1e-4f) continue;

            float dist = vu0_vec_dot(d, axis);
            int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
            int j1 = (j + 1) % 3, j2 = (j + 2) % 3;
            float rA = (e1[i1] * absC[i2][j] + e1[i2] * absC[i1][j]) / len;
            float rB = (e2[j1] * absC[i][j2] + e2[j2] * absC[i][j1]) / len;
            float pen = (rA + rB) - fabsf(dist);
            if (pen < 0.0f) return 0;
            if (pen * 0.95f < min_pen) {
                min_pen = pen;
                best_axis = 6 + i * 3 + j;
                float s = (dist >= 0.0f) ? 1.0f : -1.0f;
                vu0_vec_scale(best_normal, axis, s);
            }
        }
    }

    if (best_axis < 0) return 0;

    /* Edge-Edge contact */
    if (best_axis >= 6) {
        int i = (best_axis - 6) / 3;
        int j = (best_axis - 6) % 3;

        /* Find support edge midpoint on Box 1 (opposing best_normal) and Box 2 (along best_normal) */
        VECTOR pt1, pt2;
        vu0_vec_copy(pt1, p1);
        vu0_vec_copy(pt2, p2);
        for (int k = 0; k < 3; k++) {
            if (k != i) {
                float sign1 = (vu0_vec_dot(u[k], best_normal) > 0.0f) ? -e1[k] : e1[k];
                vu0_vec_madd(pt1, pt1, u[k], sign1);
            }
            if (k != j) {
                float sign2 = (vu0_vec_dot(v[k], best_normal) > 0.0f) ? e2[k] : -e2[k];
                vu0_vec_madd(pt2, pt2, v[k], sign2);
            }
        }

        /* Closest point between two lines (pt1 + s*u[i]) and (pt2 + t*v[j]) */
        VECTOR r;
        vu0_vec_sub(r, pt1, pt2);
        float b = vu0_vec_dot(u[i], v[j]);
        float c_dot = vu0_vec_dot(u[i], r);
        float f_dot = vu0_vec_dot(v[j], r);
        float denom = 1.0f - b * b;
        float s = 0.0f, t = 0.0f;
        if (denom > 1e-6f) {
            s = fmaxf(-e1[i], fminf(e1[i], (b * f_dot - c_dot) / denom));
            t = fmaxf(-e2[j], fminf(e2[j], (f_dot - b * c_dot) / denom));
        }
        VECTOR c1, c2;
        vu0_vec_madd(c1, pt1, u[i], s);
        vu0_vec_madd(c2, pt2, v[j], t);

        AthenaContact *c = &contacts[0];
        init_contact_defaults(c, g1, g2);
        vu0_vec_copy(c->normal, best_normal);
        c->depth = min_pen;
        c->pos[0] = 0.5f * (c1[0] + c2[0]);
        c->pos[1] = 0.5f * (c1[1] + c2[1]);
        c->pos[2] = 0.5f * (c1[2] + c2[2]);
        c->pos[3] = 0.0f;
        return 1;
    }

    /* Face-Face / Face-Vertex contact via Sutherland-Hodgman clipping */
    int ref_is_box1 = (best_axis < 3);
    int ref_ax = ref_is_box1 ? best_axis : (best_axis - 3);
    const float *ref_pos = ref_is_box1 ? p1 : p2;
    const float *inc_pos = ref_is_box1 ? p2 : p1;
    const float *ref_R   = ref_is_box1 ? R1 : R2;
    const float *ref_ext = ref_is_box1 ? e1 : e2;
    const float *inc_ext = ref_is_box1 ? e2 : e1;
    VECTOR *ref_u = ref_is_box1 ? u : v;
    VECTOR *inc_u = ref_is_box1 ? v : u;

    /* Normal pointing outward from reference box towards incident box */
    VECTOR ref_n;
    if (ref_is_box1) {
        vu0_vec_scale(ref_n, best_normal, -1.0f);
    } else {
        vu0_vec_copy(ref_n, best_normal);
    }

    /* Find most anti-parallel face on incident box */
    int inc_ax = 0;
    float min_dot = 1e30f;
    float inc_sign = 1.0f;
    for (int k = 0; k < 3; k++) {
        float dt = vu0_vec_dot(inc_u[k], ref_n);
        if (dt < min_dot) { min_dot = dt; inc_ax = k; inc_sign = 1.0f; }
        if (-dt < min_dot) { min_dot = -dt; inc_ax = k; inc_sign = -1.0f; }
    }

    /* Build 4 vertices of incident face in world space, then transform to reference box local space */
    int ia1 = (inc_ax + 1) % 3, ia2 = (inc_ax + 2) % 3;
    VECTOR inc_center;
    vu0_vec_madd(inc_center, inc_pos, inc_u[inc_ax], inc_sign * inc_ext[inc_ax]);

    VECTOR polyA[16], polyB[16];
    const float s1[4] = { -1.0f,  1.0f, 1.0f, -1.0f };
    const float s2[4] = { -1.0f, -1.0f, 1.0f,  1.0f };
    for (int k = 0; k < 4; k++) {
        VECTOR wpt, rel;
        vu0_vec_madd(wpt, inc_center, inc_u[ia1], s1[k] * inc_ext[ia1]);
        vu0_vec_madd(wpt, wpt,        inc_u[ia2], s2[k] * inc_ext[ia2]);
        vu0_vec_sub(rel, wpt, ref_pos);
        vu0_mat3_mul_transpose_vec(polyA[k], ref_R, rel);
    }

    int ra1 = (ref_ax + 1) % 3, ra2 = (ref_ax + 2) % 3;
    int n_pts = 4;
    n_pts = clip_poly_against_axis(polyA, n_pts, polyB, ra1,  1.0f, ref_ext[ra1]);
    n_pts = clip_poly_against_axis(polyB, n_pts, polyA, ra1, -1.0f, ref_ext[ra1]);
    n_pts = clip_poly_against_axis(polyA, n_pts, polyB, ra2,  1.0f, ref_ext[ra2]);
    n_pts = clip_poly_against_axis(polyB, n_pts, polyA, ra2, -1.0f, ref_ext[ra2]);

    float ref_face_sign = (vu0_vec_dot(ref_u[ref_ax], ref_n) >= 0.0f) ? 1.0f : -1.0f;
    int count = 0;
    for (int k = 0; k < n_pts; k++) {
        float pen = ref_ext[ref_ax] - ref_face_sign * polyA[k][ref_ax];
        if (pen >= -1e-4f) {
            if (pen < 0.0f) pen = 0.0f;
            VECTOR wpt;
            vu0_mat3_mul_vec(wpt, ref_R, polyA[k]);
            vu0_vec_add(wpt, ref_pos, wpt);

            int slot = count;
            if (count < max_contacts) {
                count++;
            } else {
                int min_idx = 0;
                for (int m = 1; m < max_contacts; m++) {
                    if (contacts[m].depth < contacts[min_idx].depth)
                        min_idx = m;
                }
                if (pen <= contacts[min_idx].depth)
                    continue;
                slot = min_idx;
            }
            AthenaContact *c = &contacts[slot];
            init_contact_defaults(c, g1, g2);
            vu0_vec_copy(c->normal, best_normal);
            c->depth = pen;
            vu0_vec_copy(c->pos, wpt);
        }
    }

    if (count == 0) {
        /* Fallback single contact at midpoint if numeric precision clipped all vertices */
        AthenaContact *c = &contacts[0];
        init_contact_defaults(c, g1, g2);
        vu0_vec_copy(c->normal, best_normal);
        c->depth = min_pen;
        c->pos[0] = 0.5f * (p1[0] + p2[0]);
        c->pos[1] = 0.5f * (p1[1] + p2[1]);
        c->pos[2] = 0.5f * (p1[2] + p2[2]);
        c->pos[3] = 0.0f;
        return 1;
    }
    return count;
}

/* Helper to transform a triangle from TriMesh local space into world space */
static inline void trimesh_tri_to_world(const AthenaTri *src, const VECTOR pos,
                                        const MATRIX rot, const VECTOR scale,
                                        AthenaTri *dst) {
    VECTOR v0s, p1s, p2s, w0, w1, w2;
    v0s[0] = src->v0[0] * scale[0];
    v0s[1] = src->v0[1] * scale[1];
    v0s[2] = src->v0[2] * scale[2];
    v0s[3] = 0.0f;

    p1s[0] = (src->v0[0] + src->e1[0]) * scale[0];
    p1s[1] = (src->v0[1] + src->e1[1]) * scale[1];
    p1s[2] = (src->v0[2] + src->e1[2]) * scale[2];
    p1s[3] = 0.0f;

    p2s[0] = (src->v0[0] + src->e2[0]) * scale[0];
    p2s[1] = (src->v0[1] + src->e2[1]) * scale[1];
    p2s[2] = (src->v0[2] + src->e2[2]) * scale[2];
    p2s[3] = 0.0f;

    vu0_mat3_mul_vec(w0, rot, v0s); vu0_vec_add(dst->v0, pos, w0);
    vu0_mat3_mul_vec(w1, rot, p1s); vu0_vec_add(w1, pos, w1);
    vu0_mat3_mul_vec(w2, rot, p2s); vu0_vec_add(w2, pos, w2);

    vu0_vec_sub(dst->e1, w1, dst->v0);
    vu0_vec_sub(dst->e2, w2, dst->v0);
    vu0_vec_cross(dst->normal, dst->e1, dst->e2);
    vu0_vec_normalize_len(dst->normal, dst->normal);
    /* If odd number of negative scale axes, flip normal to preserve outward orientation */
    if (scale[0] * scale[1] * scale[2] < 0.0f) {
        vu0_vec_scale(dst->normal, dst->normal, -1.0f);
    }
    dst->normal[3] = vu0_vec_dot(dst->normal, dst->v0);
}

/* Compute world-space AABB (wmin, wmax) into TriMesh local unscaled AABB (lmin, lmax) for BVH culling */
static void world_aabb_to_trimesh_local(const VECTOR wmin, const VECTOR wmax,
                                        const VECTOR tm_pos, const MATRIX tm_rot, const VECTOR tm_scale,
                                        VECTOR lmin, VECTOR lmax) {
    float inv_sx = (fabsf(tm_scale[0]) > 1e-6f) ? (1.0f / tm_scale[0]) : 1.0f;
    float inv_sy = (fabsf(tm_scale[1]) > 1e-6f) ? (1.0f / tm_scale[1]) : 1.0f;
    float inv_sz = (fabsf(tm_scale[2]) > 1e-6f) ? (1.0f / tm_scale[2]) : 1.0f;

    lmin[0] = lmin[1] = lmin[2] =  1e30f; lmin[3] = 0.0f;
    lmax[0] = lmax[1] = lmax[2] = -1e30f; lmax[3] = 0.0f;

    for (int i = 0; i < 8; i++) {
        VECTOR wc, rel, lc;
        wc[0] = (i & 1) ? wmax[0] : wmin[0];
        wc[1] = (i & 2) ? wmax[1] : wmin[1];
        wc[2] = (i & 4) ? wmax[2] : wmin[2];
        wc[3] = 0.0f;
        vu0_vec_sub(rel, wc, tm_pos);
        vu0_mat3_mul_transpose_vec(lc, tm_rot, rel);
        lc[0] *= inv_sx;
        lc[1] *= inv_sy;
        lc[2] *= inv_sz;
        vu0_vec_expand_bounds(lmin, lmax, lc);
    }
}

/* 6. Sphere (g1) vs TriMesh (g2) — VU0 BVH Traversal + Closest Point on Triangle */
static int collide_sphere_trimesh(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                  AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0 || !g2->trimesh || !g2->trimesh->nodes)
        return 0;

    AthenaPhysicsTriMesh *tm = g2->trimesh;
    VECTOR sp_pos, tm_pos;
    MATRIX sp_rot, tm_rot;
    geom_get_world_pose(g1, sp_pos, sp_rot);
    geom_get_world_pose(g2, tm_pos, tm_rot);

    float rad = g1->sphere.radius;
    float rad_sq = rad * rad;

    VECTOR wmin = { sp_pos[0] - rad, sp_pos[1] - rad, sp_pos[2] - rad, 0.0f };
    VECTOR wmax = { sp_pos[0] + rad, sp_pos[1] + rad, sp_pos[2] + rad, 0.0f };
    VECTOR qmin, qmax;
    world_aabb_to_trimesh_local(wmin, wmax, tm_pos, tm_rot, g2->scale, qmin, qmax);

    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = 0;

    int count = 0;
    while (sp > 0) {
        uint32_t node_idx = stack[--sp];
        const AthenaBVHNode *node = &tm->nodes[node_idx];

        if (!vu0_aabb_overlap(qmin, qmax, node->aabb_min, node->aabb_max))
            continue;

        uint32_t tri_cnt = bvh_node_get_count(node);
        uint32_t first   = bvh_node_get_first(node);

        if (tri_cnt > 0) {
            for (uint32_t t = 0; t < tri_cnt; t++) {
                uint32_t tidx = first + t;
                AthenaTri wtri;
                trimesh_tri_to_world(&tm->tris[tidx], tm_pos, tm_rot, g2->scale, &wtri);

                VECTOR closest, diff;
                closest_point_on_triangle(closest, sp_pos, &wtri);
                vu0_vec_sub(diff, sp_pos, closest);
                float dist_sq = vu0_vec_dot(diff, diff);
                if (dist_sq > rad_sq)
                    continue;

                float dist = sqrtf(dist_sq);
                float depth = rad - dist;
                VECTOR n;
                if (dist > 1e-6f) {
                    vu0_vec_scale(n, diff, 1.0f / dist);
                } else {
                    vu0_vec_copy(n, wtri.normal);
                }

                /* Merge with existing contact if very close in position/normal */
                int merged = 0;
                for (int k = 0; k < count; k++) {
                    VECTOR pd;
                    vu0_vec_sub(pd, contacts[k].pos, closest);
                    if (vu0_vec_dot(pd, pd) < 0.01f && vu0_vec_dot(contacts[k].normal, n) > 0.9f) {
                        if (depth > contacts[k].depth) {
                            contacts[k].depth = depth;
                            vu0_vec_copy(contacts[k].normal, n);
                            vu0_vec_copy(contacts[k].pos, closest);
                            contacts[k].tri_index = (int)tidx;
                        }
                        merged = 1;
                        break;
                    }
                }
                if (merged) continue;

                int slot = count;
                if (count < max_contacts) {
                    count++;
                } else {
                    int min_idx = 0;
                    for (int k = 1; k < max_contacts; k++) {
                        if (contacts[k].depth < contacts[min_idx].depth)
                            min_idx = k;
                    }
                    if (depth <= contacts[min_idx].depth)
                        continue;
                    slot = min_idx;
                }

                AthenaContact *c = &contacts[slot];
                init_contact_defaults(c, g1, g2);
                vu0_vec_copy(c->normal, n);
                vu0_vec_copy(c->pos, closest);
                c->depth = depth;
                c->tri_index = (int)tidx;
            }
        } else {
            if (sp + 2 <= 64) {
                stack[sp++] = first + 1;
                stack[sp++] = first;
            }
        }
    }
    return count;
}

/* 7. Box (g1) vs TriMesh (g2) — BVH Traversal + OBB vs Triangle */
static int collide_box_trimesh(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                               AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0 || !g2->trimesh || !g2->trimesh->nodes)
        return 0;

    AthenaPhysicsTriMesh *tm = g2->trimesh;
    VECTOR box_pos, tm_pos;
    MATRIX box_rot, tm_rot;
    geom_get_world_pose(g1, box_pos, box_rot);
    geom_get_world_pose(g2, tm_pos, tm_rot);

    VECTOR box_axes[3];
    for (int i = 0; i < 3; i++) {
        box_axes[i][0] = box_rot[0 + i];
        box_axes[i][1] = box_rot[4 + i];
        box_axes[i][2] = box_rot[8 + i];
        box_axes[i][3] = 0.0f;
    }
    const float *he = g1->box.half_extents;

    /* Compute Box world AABB, then transform into TriMesh local AABB */
    geom_compute_aabb(g1);
    VECTOR qmin, qmax;
    world_aabb_to_trimesh_local(g1->aabb_min, g1->aabb_max, tm_pos, tm_rot, g2->scale, qmin, qmax);

    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = 0;

    int count = 0;
    while (sp > 0) {
        uint32_t node_idx = stack[--sp];
        const AthenaBVHNode *node = &tm->nodes[node_idx];

        if (!vu0_aabb_overlap(qmin, qmax, node->aabb_min, node->aabb_max))
            continue;

        uint32_t tri_cnt = bvh_node_get_count(node);
        uint32_t first   = bvh_node_get_first(node);

        if (tri_cnt > 0) {
            for (uint32_t t = 0; t < tri_cnt; t++) {
                uint32_t tidx = first + t;
                AthenaTri wtri;
                trimesh_tri_to_world(&tm->tris[tidx], tm_pos, tm_rot, g2->scale, &wtri);

                VECTOR tv[3];
                vu0_vec_copy(tv[0], wtri.v0);
                vu0_vec_add(tv[1], wtri.v0, wtri.e1);
                vu0_vec_add(tv[2], wtri.v0, wtri.e2);

                /* SAT check 1: Triangle normal axis */
                float tri_proj = wtri.normal[3];
                float box_c = vu0_vec_dot(box_pos, wtri.normal);
                float box_r = he[0] * fabsf(vu0_vec_dot(box_axes[0], wtri.normal)) +
                              he[1] * fabsf(vu0_vec_dot(box_axes[1], wtri.normal)) +
                              he[2] * fabsf(vu0_vec_dot(box_axes[2], wtri.normal));
                float dist_n = box_c - tri_proj;
                float pen_n = box_r - fabsf(dist_n);
                if (pen_n < 0.0f)
                    continue;

                float min_pen = pen_n;
                VECTOR best_n;
                vu0_vec_scale(best_n, wtri.normal, (dist_n >= 0.0f) ? 1.0f : -1.0f);

                /* SAT check 2: 3 Box face axes */
                int separated = 0;
                for (int a = 0; a < 3; a++) {
                    float p0 = vu0_vec_dot(tv[0], box_axes[a]) - vu0_vec_dot(box_pos, box_axes[a]);
                    float p1 = vu0_vec_dot(tv[1], box_axes[a]) - vu0_vec_dot(box_pos, box_axes[a]);
                    float p2 = vu0_vec_dot(tv[2], box_axes[a]) - vu0_vec_dot(box_pos, box_axes[a]);
                    float tmin = fminf(p0, fminf(p1, p2));
                    float tmax = fmaxf(p0, fmaxf(p1, p2));
                    if (tmin > he[a] || tmax < -he[a]) {
                        separated = 1;
                        break;
                    }
                    float pen_pos = he[a] - tmin;
                    float pen_neg = tmax + he[a];
                    float pen_a = (pen_pos < pen_neg) ? pen_pos : pen_neg;
                    if (pen_a < min_pen) {
                        min_pen = pen_a;
                        float s = (pen_pos < pen_neg) ? -1.0f : 1.0f;
                        vu0_vec_scale(best_n, box_axes[a], s);
                    }
                }
                if (separated) continue;

                /* Approximate contact point by closest point on triangle to box center clamped to box */
                VECTOR tri_center = {
                    (tv[0][0] + tv[1][0] + tv[2][0]) * (1.0f / 3.0f),
                    (tv[0][1] + tv[1][1] + tv[2][1]) * (1.0f / 3.0f),
                    (tv[0][2] + tv[1][2] + tv[2][2]) * (1.0f / 3.0f),
                    0.0f
                };
                VECTOR rel_tc, local_tc, world_box_pt, contact_pt;
                vu0_vec_sub(rel_tc, tri_center, box_pos);
                vu0_mat3_mul_transpose_vec(local_tc, box_rot, rel_tc);
                local_tc[0] = fmaxf(-he[0], fminf(he[0], local_tc[0]));
                local_tc[1] = fmaxf(-he[1], fminf(he[1], local_tc[1]));
                local_tc[2] = fmaxf(-he[2], fminf(he[2], local_tc[2]));
                local_tc[3] = 0.0f;
                vu0_mat3_mul_vec(world_box_pt, box_rot, local_tc);
                vu0_vec_add(world_box_pt, box_pos, world_box_pt);
                closest_point_on_triangle(contact_pt, world_box_pt, &wtri);

                int slot = count;
                if (count < max_contacts) {
                    count++;
                } else {
                    int min_idx = 0;
                    for (int k = 1; k < max_contacts; k++) {
                        if (contacts[k].depth < contacts[min_idx].depth)
                            min_idx = k;
                    }
                    if (min_pen <= contacts[min_idx].depth)
                        continue;
                    slot = min_idx;
                }

                AthenaContact *c = &contacts[slot];
                init_contact_defaults(c, g1, g2);
                vu0_vec_copy(c->normal, best_n);
                vu0_vec_copy(c->pos, contact_pt);
                c->depth = min_pen;
                c->tri_index = (int)tidx;
            }
        } else {
            if (sp + 2 <= 64) {
                stack[sp++] = first + 1;
                stack[sp++] = first;
            }
        }
    }
    return count;
}

/* 8. TriMesh (g1) vs Plane (g2) */
static int collide_trimesh_plane(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                 AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0 || !g1->trimesh || !g1->trimesh->nodes)
        return 0;

    AthenaPhysicsTriMesh *tm = g1->trimesh;
    VECTOR tm_pos;
    MATRIX tm_rot;
    geom_get_world_pose(g1, tm_pos, tm_rot);

    const float *pn = g2->plane.normal;
    float pd = pn[3];

    int count = 0;
    for (uint32_t t = 0; t < tm->tri_count; t++) {
        AthenaTri wtri;
        trimesh_tri_to_world(&tm->tris[t], tm_pos, tm_rot, g1->scale, &wtri);

        VECTOR tv[3];
        vu0_vec_copy(tv[0], wtri.v0);
        vu0_vec_add(tv[1], wtri.v0, wtri.e1);
        vu0_vec_add(tv[2], wtri.v0, wtri.e2);

        for (int v = 0; v < 3; v++) {
            float dist = vu0_vec_dot(tv[v], pn) - pd;
            if (dist <= 0.0f) {
                float depth = -dist;
                int slot = count;
                if (count < max_contacts) {
                    count++;
                } else {
                    int min_idx = 0;
                    for (int k = 1; k < max_contacts; k++) {
                        if (contacts[k].depth < contacts[min_idx].depth)
                            min_idx = k;
                    }
                    if (depth <= contacts[min_idx].depth)
                        continue;
                    slot = min_idx;
                }
                AthenaContact *c = &contacts[slot];
                init_contact_defaults(c, g1, g2);
                c->normal[0] = pn[0]; c->normal[1] = pn[1]; c->normal[2] = pn[2]; c->normal[3] = 0.0f;
                c->depth = depth;
                vu0_vec_madd(c->pos, tv[v], c->normal, 0.5f * depth);
                c->tri_index = (int)t;
            }
        }
    }
    return count;
}

/* 9. Ray (g1) vs Sphere (g2) */
static int collide_ray_sphere(AthenaPhysicsGeom *g_ray, AthenaPhysicsGeom *g_sph,
                              AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR r_pos, s_pos, m;
    MATRIX r_rot, s_rot;
    geom_get_world_pose(g_ray, r_pos, r_rot);
    geom_get_world_pose(g_sph, s_pos, s_rot);

    const float *dir = g_ray->ray.dir;
    float max_len = g_ray->ray.length;
    float rad = g_sph->sphere.radius;

    vu0_vec_sub(m, r_pos, s_pos);
    float b = vu0_vec_dot(m, dir);
    float c_val = vu0_vec_dot(m, m) - rad * rad;

    if (c_val > 0.0f && b > 0.0f)
        return 0;

    float discr = b * b - c_val;
    if (discr < 0.0f)
        return 0;

    float sqrt_d = sqrtf(discr);
    float t = -b - sqrt_d;
    if (t < 0.0f) {
        if (g_ray->ray.backface_cull)
            return 0;
        t = 0.0f;
    }
    if (t > max_len)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g_ray, g_sph);
    vu0_vec_madd(c->pos, r_pos, dir, t);
    vu0_vec_sub(c->normal, c->pos, s_pos);
    if (vu0_vec_normalize_len(c->normal, c->normal) < 1e-6f) {
        vu0_vec_scale(c->normal, dir, -1.0f);
    }
    c->depth = t;
    return 1;
}

/* 10. Ray (g1) vs Plane (g2) */
static int collide_ray_plane(AthenaPhysicsGeom *g_ray, AthenaPhysicsGeom *g_plane,
                             AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR r_pos;
    MATRIX r_rot;
    geom_get_world_pose(g_ray, r_pos, r_rot);

    const float *dir = g_ray->ray.dir;
    const float *pn  = g_plane->plane.normal;
    float pd = pn[3];

    float denom = vu0_vec_dot(dir, pn);
    if (fabsf(denom) < 1e-7f)
        return 0;

    if (g_ray->ray.backface_cull && denom > 0.0f)
        return 0;

    float t = (pd - vu0_vec_dot(r_pos, pn)) / denom;
    if (t < 0.0f || t > g_ray->ray.length)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g_ray, g_plane);
    vu0_vec_madd(c->pos, r_pos, dir, t);
    c->normal[0] = pn[0]; c->normal[1] = pn[1]; c->normal[2] = pn[2]; c->normal[3] = 0.0f;
    if (denom > 0.0f && !g_ray->ray.backface_cull) {
        vu0_vec_scale(c->normal, c->normal, -1.0f);
    }
    c->depth = t;
    return 1;
}

/* 11. Ray (g1) vs Box (g2) — Local-space slab intersection */
static int collide_ray_box(AthenaPhysicsGeom *g_ray, AthenaPhysicsGeom *g_box,
                           AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0) return 0;
    VECTOR r_pos, b_pos, diff, lo, ld;
    MATRIX r_rot, b_rot;
    geom_get_world_pose(g_ray, r_pos, r_rot);
    geom_get_world_pose(g_box, b_pos, b_rot);

    vu0_vec_sub(diff, r_pos, b_pos);
    vu0_mat3_mul_transpose_vec(lo, b_rot, diff);
    vu0_mat3_mul_transpose_vec(ld, b_rot, g_ray->ray.dir);

    const float *he = g_box->box.half_extents;
    float tmin = 0.0f;
    float tmax = g_ray->ray.length;
    int hit_axis = -1;
    float hit_sign = 0.0f;

    for (int a = 0; a < 3; a++) {
        if (fabsf(ld[a]) < 1e-7f) {
            if (lo[a] < -he[a] || lo[a] > he[a])
                return 0;
        } else {
            float inv_d = 1.0f / ld[a];
            float t1 = (-he[a] - lo[a]) * inv_d;
            float t2 = ( he[a] - lo[a]) * inv_d;
            float s = -1.0f;
            if (t1 > t2) {
                float tmp = t1; t1 = t2; t2 = tmp;
                s = 1.0f;
            }
            if (t1 > tmin) {
                tmin = t1;
                hit_axis = a;
                hit_sign = s;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax)
                return 0;
        }
    }

    if (hit_axis < 0 && g_ray->ray.backface_cull)
        return 0;

    AthenaContact *c = &contacts[0];
    init_contact_defaults(c, g_ray, g_box);
    vu0_vec_madd(c->pos, r_pos, g_ray->ray.dir, tmin);

    VECTOR n_local = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (hit_axis >= 0) {
        n_local[hit_axis] = hit_sign;
        vu0_mat3_mul_vec(c->normal, b_rot, n_local);
    } else {
        vu0_vec_scale(c->normal, g_ray->ray.dir, -1.0f);
    }
    c->depth = tmin;
    return 1;
}

/* Ray vs AABB slab test returning near t (or > max_t if miss) */
static inline float ray_aabb_intersect(const VECTOR ro, const VECTOR inv_rd,
                                       const VECTOR bmin, const VECTOR bmax, float max_t) {
    float tx1 = (bmin[0] - ro[0]) * inv_rd[0];
    float tx2 = (bmax[0] - ro[0]) * inv_rd[0];
    float tmin = fminf(tx1, tx2);
    float tmax = fmaxf(tx1, tx2);

    float ty1 = (bmin[1] - ro[1]) * inv_rd[1];
    float ty2 = (bmax[1] - ro[1]) * inv_rd[1];
    tmin = fmaxf(tmin, fminf(ty1, ty2));
    tmax = fminf(tmax, fmaxf(ty1, ty2));

    float tz1 = (bmin[2] - ro[2]) * inv_rd[2];
    float tz2 = (bmax[2] - ro[2]) * inv_rd[2];
    tmin = fmaxf(tmin, fminf(tz1, tz2));
    tmax = fminf(tmax, fmaxf(tz1, tz2));

    if (tmax >= fmaxf(0.0f, tmin) && tmin <= max_t)
        return fmaxf(0.0f, tmin);
    return 1e30f;
}

/* 12. Ray (g1) vs TriMesh (g2) — BVH Slab Traversal + VU0 Möller-Trumbore */
static int collide_ray_trimesh(AthenaPhysicsGeom *g_ray, AthenaPhysicsGeom *g_tm,
                               AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0 || !g_tm->trimesh || !g_tm->trimesh->nodes)
        return 0;

    AthenaPhysicsTriMesh *tm = g_tm->trimesh;
    VECTOR r_pos, tm_pos, rel_o, lo, ld, inv_ld;
    MATRIX r_rot, tm_rot;
    geom_get_world_pose(g_ray, r_pos, r_rot);
    geom_get_world_pose(g_tm, tm_pos, tm_rot);

    float sx = (fabsf(g_tm->scale[0]) > 1e-6f) ? g_tm->scale[0] : 1.0f;
    float sy = (fabsf(g_tm->scale[1]) > 1e-6f) ? g_tm->scale[1] : 1.0f;
    float sz = (fabsf(g_tm->scale[2]) > 1e-6f) ? g_tm->scale[2] : 1.0f;

    /* Transform ray origin and direction into TriMesh unscaled local space.
     * Note: if we parameterize local ray as P(t) = lo + t * ld where ld = (R^T * dir) / scale,
     * then t in local space equals the EXACT world-space distance t along unit dir! */
    vu0_vec_sub(rel_o, r_pos, tm_pos);
    vu0_mat3_mul_transpose_vec(lo, tm_rot, rel_o);
    lo[0] /= sx; lo[1] /= sy; lo[2] /= sz; lo[3] = 0.0f;

    vu0_mat3_mul_transpose_vec(ld, tm_rot, g_ray->ray.dir);
    ld[0] /= sx; ld[1] /= sy; ld[2] /= sz; ld[3] = 0.0f;

    inv_ld[0] = (fabsf(ld[0]) > 1e-8f) ? (1.0f / ld[0]) : ((ld[0] >= 0.0f) ? 1e20f : -1e20f);
    inv_ld[1] = (fabsf(ld[1]) > 1e-8f) ? (1.0f / ld[1]) : ((ld[1] >= 0.0f) ? 1e20f : -1e20f);
    inv_ld[2] = (fabsf(ld[2]) > 1e-8f) ? (1.0f / ld[2]) : ((ld[2] >= 0.0f) ? 1e20f : -1e20f);
    inv_ld[3] = 0.0f;

    float max_t = g_ray->ray.length;
    int closest_only = g_ray->ray.closest_hit || (max_contacts == 1);
    int first_only   = g_ray->ray.first_contact;
    int bf_cull      = g_ray->ray.backface_cull;
    float scale_sign = (sx * sy * sz < 0.0f) ? -1.0f : 1.0f;

    uint32_t stack[64];
    int sp = 0;
    if (ray_aabb_intersect(lo, inv_ld, tm->nodes[0].aabb_min, tm->nodes[0].aabb_max, max_t) <= max_t) {
        stack[sp++] = 0;
    }

    int count = 0;
    float best_t = max_t;

    while (sp > 0) {
        uint32_t node_idx = stack[--sp];
        const AthenaBVHNode *node = &tm->nodes[node_idx];

        uint32_t tri_cnt = bvh_node_get_count(node);
        uint32_t first   = bvh_node_get_first(node);

        if (tri_cnt > 0) {
            for (uint32_t i = 0; i < tri_cnt; i++) {
                uint32_t tidx = first + i;
                const AthenaTri *tri = &tm->tris[tidx];

                /* Möller-Trumbore Ray-Triangle Intersection using VU0 cross & dot products */
                VECTOR pvec, tvec, qvec;
                vu0_vec_cross(pvec, ld, tri->e2);
                float det = vu0_vec_dot(tri->e1, pvec);

                if (bf_cull) {
                    if (det * scale_sign < 1e-7f) continue;
                } else {
                    if (fabsf(det) < 1e-7f) continue;
                }

                float inv_det = 1.0f / det;
                vu0_vec_sub(tvec, lo, tri->v0);
                float u = vu0_vec_dot(tvec, pvec) * inv_det;
                if (u < 0.0f || u > 1.0f) continue;

                vu0_vec_cross(qvec, tvec, tri->e1);
                float v = vu0_vec_dot(ld, qvec) * inv_det;
                if (v < 0.0f || (u + v) > 1.0f) continue;

                float t = vu0_vec_dot(tri->e2, qvec) * inv_det;
                if (t < 0.0f || t > best_t) continue;

                /* Transform triangle normal to world space (inverse-transpose of scale) */
                VECTOR n_local, n_world;
                n_local[0] = (tri->normal[0] / sx) * scale_sign;
                n_local[1] = (tri->normal[1] / sy) * scale_sign;
                n_local[2] = (tri->normal[2] / sz) * scale_sign;
                n_local[3] = 0.0f;
                vu0_mat3_mul_vec(n_world, tm_rot, n_local);
                vu0_vec_normalize_len(n_world, n_world);

                /* If double-sided and ray hits back face, orient normal against ray */
                if (!bf_cull && vu0_vec_dot(n_world, g_ray->ray.dir) > 0.0f) {
                    vu0_vec_scale(n_world, n_world, -1.0f);
                }

                int slot = 0;
                if (closest_only) {
                    best_t = t;
                    count = 1;
                    slot = 0;
                } else if (count < max_contacts) {
                    slot = count++;
                } else {
                    /* Replace farthest hit if this hit is closer */
                    int max_idx = 0;
                    for (int k = 1; k < max_contacts; k++) {
                        if (contacts[k].depth > contacts[max_idx].depth)
                            max_idx = k;
                    }
                    if (t >= contacts[max_idx].depth)
                        continue;
                    slot = max_idx;
                }

                AthenaContact *c = &contacts[slot];
                init_contact_defaults(c, g_ray, g_tm);
                vu0_vec_madd(c->pos, r_pos, g_ray->ray.dir, t);
                vu0_vec_copy(c->normal, n_world);
                c->depth = t;
                c->tri_index = (int)tidx;

                if (first_only)
                    return count;
            }
        } else {
            uint32_t left  = first;
            uint32_t right = first + 1;
            float tl = ray_aabb_intersect(lo, inv_ld, tm->nodes[left].aabb_min,  tm->nodes[left].aabb_max,  best_t);
            float tr = ray_aabb_intersect(lo, inv_ld, tm->nodes[right].aabb_min, tm->nodes[right].aabb_max, best_t);

            /* Push farther child first so closer child is popped first! */
            if (tl <= best_t && tr <= best_t) {
                if (sp + 2 <= 64) {
                    if (tl < tr) {
                        stack[sp++] = right;
                        stack[sp++] = left;
                    } else {
                        stack[sp++] = left;
                        stack[sp++] = right;
                    }
                }
            } else if (tl <= best_t) {
                if (sp < 64) stack[sp++] = left;
            } else if (tr <= best_t) {
                if (sp < 64) stack[sp++] = right;
            }
        }
    }
    return count;
}

/* 13. TriMesh (g1) vs TriMesh (g2) — Dual BVH overlap + vertex/triangle tests */
static int collide_trimesh_trimesh(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                   AthenaContact *contacts, int max_contacts) {
    if (max_contacts <= 0 || !g1->trimesh || !g2->trimesh)
        return 0;

    AthenaPhysicsTriMesh *tm1 = g1->trimesh;
    AthenaPhysicsTriMesh *tm2 = g2->trimesh;
    if (!tm1->nodes || !tm2->nodes) return 0;

    VECTOR p1, p2;
    MATRIX r1, r2;
    geom_get_world_pose(g1, p1, r1);
    geom_get_world_pose(g2, p2, r2);

    geom_compute_aabb(g1);
    geom_compute_aabb(g2);
    if (!vu0_aabb_overlap(g1->aabb_min, g1->aabb_max, g2->aabb_min, g2->aabb_max))
        return 0;

    VECTOR q1_min, q1_max, q2_min, q2_max;
    world_aabb_to_trimesh_local(g2->aabb_min, g2->aabb_max, p1, r1, g1->scale, q1_min, q1_max);
    world_aabb_to_trimesh_local(g1->aabb_min, g1->aabb_max, p2, r2, g2->scale, q2_min, q2_max);

    /* Collect candidate triangles from g1 that overlap g2's AABB */
    uint32_t stack[64];
    int sp = 0;
    stack[sp++] = 0;

    int count = 0;
    while (sp > 0 && count < max_contacts) {
        uint32_t n1_idx = stack[--sp];
        const AthenaBVHNode *n1 = &tm1->nodes[n1_idx];
        if (!vu0_aabb_overlap(q1_min, q1_max, n1->aabb_min, n1->aabb_max))
            continue;

        uint32_t cnt1 = bvh_node_get_count(n1);
        uint32_t fst1 = bvh_node_get_first(n1);
        if (cnt1 > 0) {
            for (uint32_t i = 0; i < cnt1 && count < max_contacts; i++) {
                AthenaTri wt1;
                trimesh_tri_to_world(&tm1->tris[fst1 + i], p1, r1, g1->scale, &wt1);
                VECTOR tc1 = {
                    wt1.v0[0] + (wt1.e1[0] + wt1.e2[0]) * (1.0f / 3.0f),
                    wt1.v0[1] + (wt1.e1[1] + wt1.e2[1]) * (1.0f / 3.0f),
                    wt1.v0[2] + (wt1.e1[2] + wt1.e2[2]) * (1.0f / 3.0f),
                    0.0f
                };

                /* Query tm2 BVH around wt1 centroid */
                float rad = 0.5f * sqrtf(vu0_vec_dot(wt1.e1, wt1.e1) + vu0_vec_dot(wt1.e2, wt1.e2));
                VECTOR wmin = { tc1[0] - rad, tc1[1] - rad, tc1[2] - rad, 0.0f };
                VECTOR wmax = { tc1[0] + rad, tc1[1] + rad, tc1[2] + rad, 0.0f };
                VECTOR lmin, lmax;
                world_aabb_to_trimesh_local(wmin, wmax, p2, r2, g2->scale, lmin, lmax);

                uint32_t stack2[64];
                int sp2 = 0;
                stack2[sp2++] = 0;
                while (sp2 > 0 && count < max_contacts) {
                    uint32_t n2_idx = stack2[--sp2];
                    const AthenaBVHNode *n2 = &tm2->nodes[n2_idx];
                    if (!vu0_aabb_overlap(lmin, lmax, n2->aabb_min, n2->aabb_max))
                        continue;
                    uint32_t cnt2 = bvh_node_get_count(n2);
                    uint32_t fst2 = bvh_node_get_first(n2);
                    if (cnt2 > 0) {
                        for (uint32_t j = 0; j < cnt2 && count < max_contacts; j++) {
                            AthenaTri wt2;
                            trimesh_tri_to_world(&tm2->tris[fst2 + j], p2, r2, g2->scale, &wt2);
                            VECTOR cp, diff;
                            closest_point_on_triangle(cp, tc1, &wt2);
                            vu0_vec_sub(diff, tc1, cp);
                            float d_sq = vu0_vec_dot(diff, diff);
                            if (d_sq < 0.05f) {
                                AthenaContact *c = &contacts[count++];
                                init_contact_defaults(c, g1, g2);
                                vu0_vec_copy(c->normal, wt2.normal);
                                vu0_vec_copy(c->pos, cp);
                                c->depth = 0.2236f - sqrtf(d_sq);
                                c->tri_index = (int)(fst2 + j);
                            }
                        }
                    } else if (sp2 + 2 <= 64) {
                        stack2[sp2++] = fst2 + 1;
                        stack2[sp2++] = fst2;
                    }
                }
            }
        } else if (sp + 2 <= 64) {
            stack[sp++] = fst1 + 1;
            stack[sp++] = fst1;
        }
    }
    return count;
}

/* Master Narrowphase Collision Dispatcher */
int athena_physics_collide(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                           int max_contacts, AthenaContact *contacts) {
    if (!g1 || !g2 || !contacts || max_contacts <= 0)
        return 0;

    /* Unwrap ATH_GEOM_TRANSFORM by temporarily composing world pose on child geom */
    if (g1->type == ATH_GEOM_TRANSFORM) {
        AthenaPhysicsGeom *child = g1->transform.child;
        if (!child) return 0;

        VECTOR saved_pos;
        MATRIX saved_rot;
        AthenaPhysicsBody *saved_body = child->body;
        vu0_vec_copy(saved_pos, child->position);
        memcpy(saved_rot, child->rotation, sizeof(MATRIX));

        VECTOR p_pos, rot_offset;
        MATRIX p_rot;
        geom_get_world_pose(g1, p_pos, p_rot);
        vu0_mat3_mul_vec(rot_offset, p_rot, saved_pos);
        vu0_vec_add(child->position, p_pos, rot_offset);
        mat3_mul(child->rotation, p_rot, saved_rot);
        child->body = NULL;
        child->aabb_dirty = 1;

        int n = athena_physics_collide(child, g2, max_contacts, contacts);

        vu0_vec_copy(child->position, saved_pos);
        memcpy(child->rotation, saved_rot, sizeof(MATRIX));
        child->body = saved_body;
        child->aabb_dirty = 1;

        for (int i = 0; i < n; i++) {
            if (contacts[i].g1 == child) contacts[i].g1 = g1;
            if (contacts[i].g2 == child) contacts[i].g2 = g1;
        }
        return n;
    }

    if (g2->type == ATH_GEOM_TRANSFORM) {
        AthenaPhysicsGeom *child = g2->transform.child;
        if (!child) return 0;

        VECTOR saved_pos;
        MATRIX saved_rot;
        AthenaPhysicsBody *saved_body = child->body;
        vu0_vec_copy(saved_pos, child->position);
        memcpy(saved_rot, child->rotation, sizeof(MATRIX));

        VECTOR p_pos, rot_offset;
        MATRIX p_rot;
        geom_get_world_pose(g2, p_pos, p_rot);
        vu0_mat3_mul_vec(rot_offset, p_rot, saved_pos);
        vu0_vec_add(child->position, p_pos, rot_offset);
        mat3_mul(child->rotation, p_rot, saved_rot);
        child->body = NULL;
        child->aabb_dirty = 1;

        int n = athena_physics_collide(g1, child, max_contacts, contacts);

        vu0_vec_copy(child->position, saved_pos);
        memcpy(child->rotation, saved_rot, sizeof(MATRIX));
        child->body = saved_body;
        child->aabb_dirty = 1;

        for (int i = 0; i < n; i++) {
            if (contacts[i].g1 == child) contacts[i].g1 = g2;
            if (contacts[i].g2 == child) contacts[i].g2 = g2;
        }
        return n;
    }

    /* Unwrap ATH_GEOM_SPACE */
    if (g1->type == ATH_GEOM_SPACE && g1->subspace) {
        int total = 0;
        geom_compute_aabb(g2);
        for (AthenaPhysicsGeom *it = g1->subspace->geoms; it && total < max_contacts; it = it->next) {
            geom_compute_aabb(it);
            if (vu0_aabb_overlap(it->aabb_min, it->aabb_max, g2->aabb_min, g2->aabb_max)) {
                total += athena_physics_collide(it, g2, max_contacts - total, &contacts[total]);
            }
        }
        return total;
    }

    if (g2->type == ATH_GEOM_SPACE && g2->subspace) {
        int total = 0;
        geom_compute_aabb(g1);
        for (AthenaPhysicsGeom *it = g2->subspace->geoms; it && total < max_contacts; it = it->next) {
            geom_compute_aabb(it);
            if (vu0_aabb_overlap(g1->aabb_min, g1->aabb_max, it->aabb_min, it->aabb_max)) {
                total += athena_physics_collide(g1, it, max_contacts - total, &contacts[total]);
            }
        }
        return total;
    }

    AthenaGeomType t1 = g1->type;
    AthenaGeomType t2 = g2->type;

    /* Ray collisions: keep surface normal at hit point and depth = distance along ray */
    if (t1 == ATH_GEOM_RAY) {
        switch (t2) {
            case ATH_GEOM_SPHERE:  return collide_ray_sphere(g1, g2, contacts, max_contacts);
            case ATH_GEOM_PLANE:   return collide_ray_plane(g1, g2, contacts, max_contacts);
            case ATH_GEOM_BOX:     return collide_ray_box(g1, g2, contacts, max_contacts);
            case ATH_GEOM_TRIMESH: return collide_ray_trimesh(g1, g2, contacts, max_contacts);
            default: return 0;
        }
    }
    if (t2 == ATH_GEOM_RAY) {
        int n = 0;
        switch (t1) {
            case ATH_GEOM_SPHERE:  n = collide_ray_sphere(g2, g1, contacts, max_contacts); break;
            case ATH_GEOM_PLANE:   n = collide_ray_plane(g2, g1, contacts, max_contacts); break;
            case ATH_GEOM_BOX:     n = collide_ray_box(g2, g1, contacts, max_contacts); break;
            case ATH_GEOM_TRIMESH: n = collide_ray_trimesh(g2, g1, contacts, max_contacts); break;
            default: return 0;
        }
        for (int i = 0; i < n; i++) {
            contacts[i].g1 = g1;
            contacts[i].g2 = g2;
        }
        return n;
    }

    /* Sphere pairs */
    if (t1 == ATH_GEOM_SPHERE && t2 == ATH_GEOM_SPHERE)
        return collide_sphere_sphere(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_SPHERE && t2 == ATH_GEOM_PLANE)
        return collide_sphere_plane(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_PLANE && t2 == ATH_GEOM_SPHERE) {
        int n = collide_sphere_plane(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }
    if (t1 == ATH_GEOM_SPHERE && t2 == ATH_GEOM_BOX)
        return collide_sphere_box(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_BOX && t2 == ATH_GEOM_SPHERE) {
        int n = collide_sphere_box(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }
    if (t1 == ATH_GEOM_SPHERE && t2 == ATH_GEOM_TRIMESH)
        return collide_sphere_trimesh(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_TRIMESH && t2 == ATH_GEOM_SPHERE) {
        int n = collide_sphere_trimesh(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }

    /* Box pairs */
    if (t1 == ATH_GEOM_BOX && t2 == ATH_GEOM_BOX)
        return collide_box_box(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_BOX && t2 == ATH_GEOM_PLANE)
        return collide_box_plane(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_PLANE && t2 == ATH_GEOM_BOX) {
        int n = collide_box_plane(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }
    if (t1 == ATH_GEOM_BOX && t2 == ATH_GEOM_TRIMESH)
        return collide_box_trimesh(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_TRIMESH && t2 == ATH_GEOM_BOX) {
        int n = collide_box_trimesh(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }

    /* TriMesh pairs */
    if (t1 == ATH_GEOM_TRIMESH && t2 == ATH_GEOM_PLANE)
        return collide_trimesh_plane(g1, g2, contacts, max_contacts);
    if (t1 == ATH_GEOM_PLANE && t2 == ATH_GEOM_TRIMESH) {
        int n = collide_trimesh_plane(g2, g1, contacts, max_contacts);
        flip_contacts(contacts, n, g1, g2);
        return n;
    }
    if (t1 == ATH_GEOM_TRIMESH && t2 == ATH_GEOM_TRIMESH)
        return collide_trimesh_trimesh(g1, g2, contacts, max_contacts);

    return 0;
}

/* ============================================================================
 * Broadphase Space & Fast Hardware-Accelerated Raycast
 * ============================================================================ */

AthenaPhysicsSpace *athena_physics_space_create(AthenaPhysicsSpace *parent) {
    AthenaPhysicsSpace *space = (AthenaPhysicsSpace *)phys_calloc_aligned(sizeof(AthenaPhysicsSpace));
    if (!space) return NULL;
    space->parent = parent;
    space->geom_wrapper.type = ATH_GEOM_SPACE;
    space->geom_wrapper.subspace = space;
    vu0_mat_identity(space->geom_wrapper.rotation);
    space->geom_wrapper.scale[0] = 1.0f;
    space->geom_wrapper.scale[1] = 1.0f;
    space->geom_wrapper.scale[2] = 1.0f;
    space->geom_wrapper.aabb_dirty = 1;
    if (parent) {
        athena_physics_space_add(parent, &space->geom_wrapper);
    }
    return space;
}

void athena_physics_space_destroy(AthenaPhysicsSpace *space) {
    if (!space) return;
    AthenaPhysicsGeom *g = space->geoms;
    while (g) {
        AthenaPhysicsGeom *next = g->next;
        g->space = NULL;
        g->next = NULL;
        g = next;
    }
    free(space);
}

void athena_physics_space_add(AthenaPhysicsSpace *space, AthenaPhysicsGeom *geom) {
    if (!space || !geom) return;
    if (geom->space == space) return;
    if (geom->space) {
        athena_physics_space_remove(geom->space, geom);
    }
    geom->space = space;
    geom->next = space->geoms;
    space->geoms = geom;
    space->geom_count++;
}

void athena_physics_space_remove(AthenaPhysicsSpace *space, AthenaPhysicsGeom *geom) {
    if (!space || !geom || geom->space != space) return;
    AthenaPhysicsGeom **pp = &space->geoms;
    while (*pp) {
        if (*pp == geom) {
            *pp = geom->next;
            geom->next = NULL;
            geom->space = NULL;
            if (space->geom_count > 0) space->geom_count--;
            return;
        }
        pp = &((*pp)->next);
    }
}

void athena_physics_space_collide(AthenaPhysicsSpace *space, void *data, AthenaNearCallback callback) {
    if (!space || !callback) return;

    /* 1. Update AABBs for all geometries in the space */
    for (AthenaPhysicsGeom *g = space->geoms; g; g = g->next) {
        if (g->aabb_dirty || (g->body && g->body->enabled) || g->type == ATH_GEOM_TRANSFORM) {
            geom_compute_aabb(g);
        }
    }

    /* 2. Broadphase pair pruning using VU0 MAC sign flags (vu0_aabb_overlap) */
    for (AthenaPhysicsGeom *g1 = space->geoms; g1; g1 = g1->next) {
        for (AthenaPhysicsGeom *g2 = g1->next; g2; g2 = g2->next) {
            if (g1->type == ATH_GEOM_PLANE && g2->type == ATH_GEOM_PLANE)
                continue;
            if (g1->body && g1->body == g2->body)
                continue;
            if (vu0_aabb_overlap(g1->aabb_min, g1->aabb_max, g2->aabb_min, g2->aabb_max)) {
                callback(data, g1, g2);
            }
        }
    }
}

void athena_physics_space_collide2(AthenaPhysicsGeom *g1, AthenaPhysicsGeom *g2,
                                   void *data, AthenaNearCallback callback) {
    if (!g1 || !g2 || !callback) return;

    if (g1->type == ATH_GEOM_SPACE && g1->subspace) {
        for (AthenaPhysicsGeom *it = g1->subspace->geoms; it; it = it->next) {
            athena_physics_space_collide2(it, g2, data, callback);
        }
        return;
    }

    if (g2->type == ATH_GEOM_SPACE && g2->subspace) {
        geom_compute_aabb(g1);
        for (AthenaPhysicsGeom *it = g2->subspace->geoms; it; it = it->next) {
            if (it->aabb_dirty || (it->body && it->body->enabled) || it->type == ATH_GEOM_TRANSFORM) {
                geom_compute_aabb(it);
            }
            if (vu0_aabb_overlap(g1->aabb_min, g1->aabb_max, it->aabb_min, it->aabb_max)) {
                callback(data, g1, it);
            }
        }
        return;
    }

    geom_compute_aabb(g1);
    geom_compute_aabb(g2);
    if (vu0_aabb_overlap(g1->aabb_min, g1->aabb_max, g2->aabb_min, g2->aabb_max)) {
        callback(data, g1, g2);
    }
}

/* Direct high-speed raycast against space (zero callback overhead, ideal for shadow grids) */
int athena_physics_space_raycast(AthenaPhysicsSpace *space, const VECTOR origin, const VECTOR dir,
                                 float max_len, int backface_cull, AthenaRaycastHit *out_hit) {
    if (!space || !origin || !dir || max_len <= 0.0f)
        return 0;

    AthenaPhysicsGeom ray_geom __attribute__((aligned(16)));
    memset(&ray_geom, 0, sizeof(ray_geom));
    ray_geom.type = ATH_GEOM_RAY;
    vu0_vec_copy(ray_geom.position, origin);
    vu0_mat_identity(ray_geom.rotation);
    ray_geom.scale[0] = ray_geom.scale[1] = ray_geom.scale[2] = 1.0f;
    vu0_vec_normalize_len(ray_geom.ray.dir, dir);
    ray_geom.ray.length = max_len;
    ray_geom.ray.first_contact = 0;
    ray_geom.ray.backface_cull = backface_cull;
    ray_geom.ray.closest_hit = 1;
    geom_compute_aabb(&ray_geom);

    int hit_any = 0;
    float best_dist = max_len;
    AthenaContact best_contact __attribute__((aligned(16)));

    for (AthenaPhysicsGeom *g = space->geoms; g; g = g->next) {
        if (g->type == ATH_GEOM_RAY)
            continue;
        if (g->aabb_dirty || (g->body && g->body->enabled) || g->type == ATH_GEOM_TRANSFORM) {
            geom_compute_aabb(g);
        }
        if (!vu0_aabb_overlap(ray_geom.aabb_min, ray_geom.aabb_max, g->aabb_min, g->aabb_max))
            continue;

        AthenaContact c __attribute__((aligned(16)));
        ray_geom.ray.length = best_dist;
        if (athena_physics_collide(&ray_geom, g, 1, &c) > 0) {
            if (c.depth <= best_dist) {
                best_dist = c.depth;
                best_contact = c;
                hit_any = 1;
            }
        }
    }

    if (hit_any && out_hit) {
        vu0_vec_copy(out_hit->pos, best_contact.pos);
        vu0_vec_copy(out_hit->normal, best_contact.normal);
        out_hit->depth = best_contact.depth;
        out_hit->geom = best_contact.g2;
        out_hit->tri_index = best_contact.tri_index;
    }
    return hit_any;
}

/* ============================================================================
 * Geometry API Implementation
 * ============================================================================ */

static AthenaPhysicsGeom *alloc_geom(AthenaPhysicsSpace *space, AthenaGeomType type) {
    AthenaPhysicsGeom *g = (AthenaPhysicsGeom *)phys_calloc_aligned(sizeof(AthenaPhysicsGeom));
    if (!g) return NULL;
    g->type = type;
    vu0_mat_identity(g->rotation);
    g->scale[0] = 1.0f;
    g->scale[1] = 1.0f;
    g->scale[2] = 1.0f;
    g->scale[3] = 0.0f;
    g->aabb_dirty = 1;
    if (space) {
        athena_physics_space_add(space, g);
    }
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_sphere(AthenaPhysicsSpace *space, float radius) {
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_SPHERE);
    if (!g) return NULL;
    g->sphere.radius = (radius > 0.0f) ? radius : 0.001f;
    geom_compute_aabb(g);
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_box(AthenaPhysicsSpace *space, float lx, float ly, float lz) {
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_BOX);
    if (!g) return NULL;
    g->box.half_extents[0] = 0.5f * fabsf(lx);
    g->box.half_extents[1] = 0.5f * fabsf(ly);
    g->box.half_extents[2] = 0.5f * fabsf(lz);
    g->box.half_extents[3] = 0.0f;
    geom_compute_aabb(g);
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_plane(AthenaPhysicsSpace *space, float a, float b, float c, float d) {
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_PLANE);
    if (!g) return NULL;
    VECTOR n = { a, b, c, 0.0f };
    float len = vu0_vec_normalize_len(g->plane.normal, n);
    if (len > 1e-6f) {
        g->plane.normal[3] = d / len;
    } else {
        g->plane.normal[0] = 0.0f;
        g->plane.normal[1] = 1.0f;
        g->plane.normal[2] = 0.0f;
        g->plane.normal[3] = d;
    }
    geom_compute_aabb(g);
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_ray(AthenaPhysicsSpace *space, float length) {
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_RAY);
    if (!g) return NULL;
    g->ray.length = (length >= 0.0f) ? length : 0.0f;
    g->ray.dir[0] = 0.0f;
    g->ray.dir[1] = 0.0f;
    g->ray.dir[2] = 1.0f;
    g->ray.dir[3] = 0.0f;
    g->ray.first_contact = 0;
    g->ray.backface_cull = 0;
    g->ray.closest_hit = 1;
    geom_compute_aabb(g);
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_transform(AthenaPhysicsSpace *space, AthenaPhysicsGeom *child) {
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_TRANSFORM);
    if (!g) return NULL;
    g->transform.child = child;
    geom_compute_aabb(g);
    return g;
}

AthenaPhysicsGeom *athena_physics_geom_create_trimesh(AthenaPhysicsSpace *space, const VECTOR *verts,
                                                      uint32_t vert_count, const uint32_t *indices, uint32_t index_count) {
    AthenaPhysicsTriMesh *tm = trimesh_build(verts, vert_count, indices, index_count);
    if (!tm) return NULL;
    AthenaPhysicsGeom *g = alloc_geom(space, ATH_GEOM_TRIMESH);
    if (!g) {
        free(tm->tris);
        free(tm->nodes);
        free(tm);
        return NULL;
    }
    g->trimesh = tm;
    geom_compute_aabb(g);
    return g;
}

/* Helper: convert Euler angles (radians) in obj->rotation to a 3x3 rotation matrix in MATRIX */
static void euler_to_rot_matrix(const VECTOR rot_euler, MATRIX out_rot) {
    float rx = rot_euler[0], ry = rot_euler[1], rz = rot_euler[2];
    float cx = cosf(rx), sx = sinf(rx);
    float cy = cosf(ry), sy = sinf(ry);
    float cz = cosf(rz), sz = sinf(rz);

    /* Matches Athena's matrix_functions->rotate Euler angle convention */
    out_rot[0]  =  cy * cz;
    out_rot[1]  = -cy * sz;
    out_rot[2]  =  sy;
    out_rot[3]  =  0.0f;

    out_rot[4]  =  sx * sy * cz + cx * sz;
    out_rot[5]  = -sx * sy * sz + cx * cz;
    out_rot[6]  = -sx * cy;
    out_rot[7]  =  0.0f;

    out_rot[8]  = -cx * sy * cz + sx * sz;
    out_rot[9]  =  cx * sy * sz + sx * cz;
    out_rot[10] =  cx * cy;
    out_rot[11] =  0.0f;

    out_rot[12] =  0.0f;
    out_rot[13] =  0.0f;
    out_rot[14] =  0.0f;
    out_rot[15] =  1.0f;
}

AthenaPhysicsGeom *athena_physics_geom_create_from_render_object(AthenaPhysicsSpace *space, athena_object_data *obj) {
    if (!obj || !obj->data || !obj->data->positions || obj->data->index_count < 3)
        return NULL;

    athena_render_data *data = obj->data;
    AthenaPhysicsGeom *g = athena_physics_geom_create_trimesh(
        space,
        (const VECTOR *)data->positions,
        data->index_count,
        data->indices,
        data->index_count
    );
    if (!g) return NULL;

    g->position[0] = obj->position[0];
    g->position[1] = obj->position[1];
    g->position[2] = obj->position[2];
    g->position[3] = 0.0f;
    euler_to_rot_matrix(obj->rotation, g->rotation);
    if (fabsf(obj->scale[0]) > 1e-6f && fabsf(obj->scale[1]) > 1e-6f && fabsf(obj->scale[2]) > 1e-6f) {
        g->scale[0] = obj->scale[0];
        g->scale[1] = obj->scale[1];
        g->scale[2] = obj->scale[2];
        g->scale[3] = 0.0f;
    }
    geom_compute_aabb(g);
    return g;
}

void athena_physics_geom_destroy(AthenaPhysicsGeom *geom) {
    if (!geom) return;
    if (geom->space) {
        athena_physics_space_remove(geom->space, geom);
    }
    if (geom->type == ATH_GEOM_TRIMESH && geom->trimesh) {
        if (geom->trimesh->ref_count <= 1) {
            free(geom->trimesh->tris);
            free(geom->trimesh->nodes);
            free(geom->trimesh);
        } else {
            geom->trimesh->ref_count--;
        }
        geom->trimesh = NULL;
    }
    free(geom);
}

void athena_physics_geom_set_position(AthenaPhysicsGeom *geom, float x, float y, float z) {
    if (!geom) return;
    if (geom->body) {
        athena_physics_body_set_position(geom->body, x, y, z);
    } else {
        geom->position[0] = x;
        geom->position[1] = y;
        geom->position[2] = z;
        geom->position[3] = 0.0f;
    }
    geom->aabb_dirty = 1;
}

void athena_physics_geom_get_position(const AthenaPhysicsGeom *geom, float *x, float *y, float *z) {
    if (!geom) return;
    const float *p = geom->body ? geom->body->position : geom->position;
    if (x) *x = p[0];
    if (y) *y = p[1];
    if (z) *z = p[2];
}

void athena_physics_geom_set_rotation(AthenaPhysicsGeom *geom, const float *r9_or_r12) {
    if (!geom || !r9_or_r12) return;
    if (geom->body) {
        athena_physics_body_set_rotation(geom->body, r9_or_r12);
    } else {
        geom->rotation[0]  = r9_or_r12[0]; geom->rotation[1]  = r9_or_r12[1]; geom->rotation[2]  = r9_or_r12[2]; geom->rotation[3]  = 0.0f;
        geom->rotation[4]  = r9_or_r12[3]; geom->rotation[5]  = r9_or_r12[4]; geom->rotation[6]  = r9_or_r12[5]; geom->rotation[7]  = 0.0f;
        geom->rotation[8]  = r9_or_r12[6]; geom->rotation[9]  = r9_or_r12[7]; geom->rotation[10] = r9_or_r12[8]; geom->rotation[11] = 0.0f;
        geom->rotation[12] = 0.0f;         geom->rotation[13] = 0.0f;         geom->rotation[14] = 0.0f;         geom->rotation[15] = 1.0f;
        if (geom->type == ATH_GEOM_RAY) {
            geom->ray.dir[0] = geom->rotation[2];
            geom->ray.dir[1] = geom->rotation[6];
            geom->ray.dir[2] = geom->rotation[10];
            geom->ray.dir[3] = 0.0f;
            vu0_vec_normalize_len(geom->ray.dir, geom->ray.dir);
        }
    }
    geom->aabb_dirty = 1;
}

void athena_physics_geom_get_rotation(const AthenaPhysicsGeom *geom, float *out_r9) {
    if (!geom || !out_r9) return;
    const float *R = geom->body ? geom->body->rotation : geom->rotation;
    out_r9[0] = R[0]; out_r9[1] = R[1]; out_r9[2] = R[2];
    out_r9[3] = R[4]; out_r9[4] = R[5]; out_r9[5] = R[6];
    out_r9[6] = R[8]; out_r9[7] = R[9]; out_r9[8] = R[10];
}

void athena_physics_geom_set_body(AthenaPhysicsGeom *geom, AthenaPhysicsBody *body) {
    if (!geom) return;
    geom->body = body;
    geom->aabb_dirty = 1;
}

AthenaPhysicsBody *athena_physics_geom_get_body(const AthenaPhysicsGeom *geom) {
    return geom ? geom->body : NULL;
}

void athena_physics_geom_set_data(AthenaPhysicsGeom *geom, void *data) {
    if (geom) geom->user_data = data;
}

void *athena_physics_geom_get_data(const AthenaPhysicsGeom *geom) {
    return geom ? geom->user_data : NULL;
}

void athena_physics_ray_set(AthenaPhysicsGeom *ray, float px, float py, float pz, float dx, float dy, float dz) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    ray->position[0] = px;
    ray->position[1] = py;
    ray->position[2] = pz;
    ray->position[3] = 0.0f;
    VECTOR d = { dx, dy, dz, 0.0f };
    if (vu0_vec_normalize_len(ray->ray.dir, d) < 1e-6f) {
        ray->ray.dir[0] = 0.0f;
        ray->ray.dir[1] = 0.0f;
        ray->ray.dir[2] = 1.0f;
        ray->ray.dir[3] = 0.0f;
    }
    /* Keep rotation column 2 synchronized with ray direction */
    ray->rotation[2]  = ray->ray.dir[0];
    ray->rotation[6]  = ray->ray.dir[1];
    ray->rotation[10] = ray->ray.dir[2];
    ray->aabb_dirty = 1;
}

void athena_physics_ray_get(const AthenaPhysicsGeom *ray, VECTOR start, VECTOR dir) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    if (start) vu0_vec_copy(start, ray->position);
    if (dir)   vu0_vec_copy(dir, ray->ray.dir);
}

void athena_physics_ray_set_length(AthenaPhysicsGeom *ray, float length) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    ray->ray.length = (length >= 0.0f) ? length : 0.0f;
    ray->aabb_dirty = 1;
}

float athena_physics_ray_get_length(const AthenaPhysicsGeom *ray) {
    return (ray && ray->type == ATH_GEOM_RAY) ? ray->ray.length : 0.0f;
}

void athena_physics_ray_set_params(AthenaPhysicsGeom *ray, int first_contact, int backface_cull) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    ray->ray.first_contact = first_contact;
    ray->ray.backface_cull = backface_cull;
}

void athena_physics_ray_get_params(const AthenaPhysicsGeom *ray, int *first_contact, int *backface_cull) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    if (first_contact) *first_contact = ray->ray.first_contact;
    if (backface_cull) *backface_cull = ray->ray.backface_cull;
}

void athena_physics_ray_set_closest_hit(AthenaPhysicsGeom *ray, int closest_hit) {
    if (!ray || ray->type != ATH_GEOM_RAY) return;
    ray->ray.closest_hit = closest_hit;
}

int athena_physics_ray_get_closest_hit(const AthenaPhysicsGeom *ray) {
    return (ray && ray->type == ATH_GEOM_RAY) ? ray->ray.closest_hit : 0;
}

/* ============================================================================
 * Rigid Body API Implementation
 * ============================================================================ */

AthenaPhysicsBody *athena_physics_body_create(AthenaPhysicsWorld *world) {
    if (!world) return NULL;
    AthenaPhysicsBody *b = (AthenaPhysicsBody *)phys_calloc_aligned(sizeof(AthenaPhysicsBody));
    if (!b) return NULL;

    b->world = world;
    b->quat[0] = 0.0f; b->quat[1] = 0.0f; b->quat[2] = 0.0f; b->quat[3] = 1.0f;
    vu0_mat_identity(b->rotation);

    /* Default unit sphere mass (1.0) so bodies work immediately even if setMass isn't called */
    b->mass = 1.0f;
    b->inv_mass = 1.0f;
    b->inv_inertia_local[0] = 2.5f;
    b->inv_inertia_local[1] = 2.5f;
    b->inv_inertia_local[2] = 2.5f;
    b->inv_inertia_local[3] = 0.0f;
    update_body_inertia_world(b);

    b->linear_damping  = 0.005f;
    b->angular_damping = 0.01f;
    b->enabled = true;

    b->next = world->bodies;
    world->bodies = b;
    world->body_count++;
    return b;
}

void athena_physics_body_destroy(AthenaPhysicsBody *body) {
    if (!body) return;
    if (body->world) {
        AthenaPhysicsBody **pp = &body->world->bodies;
        while (*pp) {
            if (*pp == body) {
                *pp = body->next;
                if (body->world->body_count > 0) body->world->body_count--;
                break;
            }
            pp = &((*pp)->next);
        }
    }
    free(body);
}

void athena_physics_body_set_position(AthenaPhysicsBody *body, float x, float y, float z) {
    if (!body) return;
    body->position[0] = x;
    body->position[1] = y;
    body->position[2] = z;
    body->position[3] = 0.0f;
}

void athena_physics_body_get_position(const AthenaPhysicsBody *body, float *x, float *y, float *z) {
    if (!body) return;
    if (x) *x = body->position[0];
    if (y) *y = body->position[1];
    if (z) *z = body->position[2];
}

void athena_physics_body_set_rotation(AthenaPhysicsBody *body, const float *r9_or_r12) {
    if (!body || !r9_or_r12) return;
    body->rotation[0]  = r9_or_r12[0]; body->rotation[1]  = r9_or_r12[1]; body->rotation[2]  = r9_or_r12[2]; body->rotation[3]  = 0.0f;
    body->rotation[4]  = r9_or_r12[3]; body->rotation[5]  = r9_or_r12[4]; body->rotation[6]  = r9_or_r12[5]; body->rotation[7]  = 0.0f;
    body->rotation[8]  = r9_or_r12[6]; body->rotation[9]  = r9_or_r12[7]; body->rotation[10] = r9_or_r12[8]; body->rotation[11] = 0.0f;
    body->rotation[12] = 0.0f;         body->rotation[13] = 0.0f;         body->rotation[14] = 0.0f;         body->rotation[15] = 1.0f;
    rot_matrix_to_quat(body->quat, body->rotation);
    update_body_inertia_world(body);
}

void athena_physics_body_get_rotation(const AthenaPhysicsBody *body, float *out_r9) {
    if (!body || !out_r9) return;
    const float *R = body->rotation;
    out_r9[0] = R[0]; out_r9[1] = R[1]; out_r9[2] = R[2];
    out_r9[3] = R[4]; out_r9[4] = R[5]; out_r9[5] = R[6];
    out_r9[6] = R[8]; out_r9[7] = R[9]; out_r9[8] = R[10];
}

void athena_physics_body_set_linear_vel(AthenaPhysicsBody *body, float x, float y, float z) {
    if (!body) return;
    body->linear_vel[0] = x;
    body->linear_vel[1] = y;
    body->linear_vel[2] = z;
    body->linear_vel[3] = 0.0f;
}

void athena_physics_body_get_linear_vel(const AthenaPhysicsBody *body, float *x, float *y, float *z) {
    if (!body) return;
    if (x) *x = body->linear_vel[0];
    if (y) *y = body->linear_vel[1];
    if (z) *z = body->linear_vel[2];
}

void athena_physics_body_set_angular_vel(AthenaPhysicsBody *body, float x, float y, float z) {
    if (!body) return;
    body->angular_vel[0] = x;
    body->angular_vel[1] = y;
    body->angular_vel[2] = z;
    body->angular_vel[3] = 0.0f;
}

void athena_physics_body_get_angular_vel(const AthenaPhysicsBody *body, float *x, float *y, float *z) {
    if (!body) return;
    if (x) *x = body->angular_vel[0];
    if (y) *y = body->angular_vel[1];
    if (z) *z = body->angular_vel[2];
}

void athena_physics_body_set_mass(AthenaPhysicsBody *body, float mass) {
    athena_physics_body_set_mass_sphere(body, mass, 1.0f);
}

void athena_physics_body_set_mass_box(AthenaPhysicsBody *body, float mass, float lx, float ly, float lz) {
    if (!body) return;
    if (mass <= 1e-6f) {
        body->mass = 0.0f;
        body->inv_mass = 0.0f;
        vu0_vec_zero(body->inv_inertia_local);
    } else {
        body->mass = mass;
        body->inv_mass = 1.0f / mass;
        float ixx = (mass / 12.0f) * (ly * ly + lz * lz);
        float iyy = (mass / 12.0f) * (lx * lx + lz * lz);
        float izz = (mass / 12.0f) * (lx * lx + ly * ly);
        body->inv_inertia_local[0] = (ixx > 1e-6f) ? (1.0f / ixx) : 0.0f;
        body->inv_inertia_local[1] = (iyy > 1e-6f) ? (1.0f / iyy) : 0.0f;
        body->inv_inertia_local[2] = (izz > 1e-6f) ? (1.0f / izz) : 0.0f;
        body->inv_inertia_local[3] = 0.0f;
    }
    update_body_inertia_world(body);
}

void athena_physics_body_set_mass_sphere(AthenaPhysicsBody *body, float mass, float radius) {
    if (!body) return;
    if (mass <= 1e-6f) {
        body->mass = 0.0f;
        body->inv_mass = 0.0f;
        vu0_vec_zero(body->inv_inertia_local);
    } else {
        body->mass = mass;
        body->inv_mass = 1.0f / mass;
        float r = (radius > 1e-4f) ? radius : 1.0f;
        float inertia = 0.4f * mass * r * r;
        float inv_i = (inertia > 1e-6f) ? (1.0f / inertia) : 0.0f;
        body->inv_inertia_local[0] = inv_i;
        body->inv_inertia_local[1] = inv_i;
        body->inv_inertia_local[2] = inv_i;
        body->inv_inertia_local[3] = 0.0f;
    }
    update_body_inertia_world(body);
}

void athena_physics_body_add_force(AthenaPhysicsBody *body, float fx, float fy, float fz) {
    if (!body) return;
    body->force_acc[0] += fx;
    body->force_acc[1] += fy;
    body->force_acc[2] += fz;
}

void athena_physics_body_add_torque(AthenaPhysicsBody *body, float fx, float fy, float fz) {
    if (!body) return;
    body->torque_acc[0] += fx;
    body->torque_acc[1] += fy;
    body->torque_acc[2] += fz;
}

void athena_physics_body_enable(AthenaPhysicsBody *body) {
    if (body) body->enabled = true;
}

void athena_physics_body_disable(AthenaPhysicsBody *body) {
    if (body) body->enabled = false;
}

bool athena_physics_body_is_enabled(const AthenaPhysicsBody *body) {
    return body ? body->enabled : false;
}

int athena_physics_bodies_connected_excluding(const AthenaPhysicsBody *b1, const AthenaPhysicsBody *b2, AthenaJointType joint_type) {
    if (!b1 || !b2 || !b1->world) return 0;
    for (AthenaPhysicsJoint *j = b1->world->joints; j; j = j->next) {
        if (j->type == joint_type) continue;
        if ((j->body1 == b1 && j->body2 == b2) || (j->body1 == b2 && j->body2 == b1))
            return 1;
    }
    return 0;
}

/* ============================================================================
 * Joints & JointGroups Implementation
 * ============================================================================ */

AthenaPhysicsJointGroup *athena_physics_joint_group_create(void) {
    return (AthenaPhysicsJointGroup *)phys_calloc_aligned(sizeof(AthenaPhysicsJointGroup));
}

void athena_physics_joint_group_empty(AthenaPhysicsJointGroup *group) {
    if (!group) return;
    AthenaPhysicsJoint *j = group->joints;
    while (j) {
        AthenaPhysicsJoint *next = j->group_next;
        if (j->world) {
            AthenaPhysicsJoint **pp = &j->world->joints;
            while (*pp) {
                if (*pp == j) {
                    *pp = j->next;
                    break;
                }
                pp = &((*pp)->next);
            }
        }
        free(j);
        j = next;
    }
    group->joints = NULL;
}

void athena_physics_joint_group_destroy(AthenaPhysicsJointGroup *group) {
    if (!group) return;
    athena_physics_joint_group_empty(group);
    free(group);
}

static AthenaPhysicsJoint *alloc_joint(AthenaPhysicsWorld *world, AthenaPhysicsJointGroup *group, AthenaJointType type) {
    AthenaPhysicsJoint *j = (AthenaPhysicsJoint *)phys_calloc_aligned(sizeof(AthenaPhysicsJoint));
    if (!j) return NULL;
    j->type = type;
    j->world = world;
    j->group = group;
    j->axis1_local[0] = 1.0f;
    j->axis2_local[1] = 1.0f;
    j->axis3_local[2] = 1.0f;
    j->qrel_init[3] = 1.0f;

    if (world) {
        j->next = world->joints;
        world->joints = j;
    }
    if (group) {
        j->group_next = group->joints;
        group->joints = j;
    }
    return j;
}

AthenaPhysicsJoint *athena_physics_joint_create(AthenaPhysicsWorld *world, AthenaPhysicsJointGroup *group, AthenaJointType type) {
    return alloc_joint(world, group, type);
}

AthenaPhysicsJoint *athena_physics_joint_create_contact(AthenaPhysicsWorld *world, AthenaPhysicsJointGroup *group, const AthenaContact *contact) {
    AthenaPhysicsJoint *j = alloc_joint(world, group, ATH_JOINT_CONTACT);
    if (j && contact) {
        j->contact = *contact;
    }
    return j;
}

void athena_physics_joint_destroy(AthenaPhysicsJoint *joint) {
    if (!joint) return;
    if (joint->world) {
        AthenaPhysicsJoint **pp = &joint->world->joints;
        while (*pp) {
            if (*pp == joint) {
                *pp = joint->next;
                break;
            }
            pp = &((*pp)->next);
        }
    }
    if (joint->group) {
        AthenaPhysicsJoint **pp = &joint->group->joints;
        while (*pp) {
            if (*pp == joint) {
                *pp = joint->group_next;
                break;
            }
            pp = &((*pp)->group_next);
        }
    }
    free(joint);
}

void athena_physics_joint_attach(AthenaPhysicsJoint *joint, AthenaPhysicsBody *b1, AthenaPhysicsBody *b2) {
    if (!joint) return;
    joint->body1 = b1;
    joint->body2 = b2;
    if (joint->type == ATH_JOINT_FIXED || joint->type == ATH_JOINT_SLIDER) {
        VECTOR q1 = { 0.0f, 0.0f, 0.0f, 1.0f };
        VECTOR q2 = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (b1) vu0_vec_copy(q1, b1->quat);
        if (b2) vu0_vec_copy(q2, b2->quat);
        VECTOR q1_conj;
        quat_conjugate(q1_conj, q1);
        quat_mul(joint->qrel_init, q1_conj, q2);
    }
}

AthenaPhysicsBody *athena_physics_joint_get_body(const AthenaPhysicsJoint *joint, int index) {
    if (!joint) return NULL;
    return (index == 0) ? joint->body1 : ((index == 1) ? joint->body2 : NULL);
}

/* Helper: transform world point into body local space (or leave in world if body == NULL) */
static void world_pt_to_body_local(const AthenaPhysicsBody *b, const VECTOR wpt, VECTOR out_local) {
    if (b) {
        VECTOR rel;
        vu0_vec_sub(rel, wpt, b->position);
        vu0_mat3_mul_transpose_vec(out_local, b->rotation, rel);
    } else {
        vu0_vec_copy(out_local, wpt);
    }
}

/* Helper: transform body local point into world space */
static void body_local_pt_to_world(const AthenaPhysicsBody *b, const VECTOR lpt, VECTOR out_world) {
    if (b) {
        VECTOR rot_l;
        vu0_mat3_mul_vec(rot_l, b->rotation, lpt);
        vu0_vec_add(out_world, b->position, rot_l);
    } else {
        vu0_vec_copy(out_world, lpt);
    }
}

/* Helper: transform world direction into body local space */
static void world_dir_to_body_local(const AthenaPhysicsBody *b, const VECTOR wdir, VECTOR out_local) {
    VECTOR u;
    vu0_vec_normalize_len(u, wdir);
    if (b) {
        vu0_mat3_mul_transpose_vec(out_local, b->rotation, u);
    } else {
        vu0_vec_copy(out_local, u);
    }
}

/* Helper: transform body local direction into world space */
static void body_local_dir_to_world(const AthenaPhysicsBody *b, const VECTOR ldir, VECTOR out_world) {
    if (b) {
        vu0_mat3_mul_vec(out_world, b->rotation, ldir);
        vu0_vec_normalize_len(out_world, out_world);
    } else {
        vu0_vec_copy(out_world, ldir);
    }
}

void athena_physics_joint_set_ball_anchor(AthenaPhysicsJoint *j, float x, float y, float z) {
    if (!j) return;
    VECTOR w = { x, y, z, 0.0f };
    world_pt_to_body_local(j->body1, w, j->anchor1_local);
    world_pt_to_body_local(j->body2, w, j->anchor2_local);
}

void athena_physics_joint_get_ball_anchor(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_pt_to_world(j->body1, j->anchor1_local, out);
}

void athena_physics_joint_get_ball_anchor2(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_pt_to_world(j->body2, j->anchor2_local, out);
}

void athena_physics_joint_set_hinge_anchor(AthenaPhysicsJoint *j, float x, float y, float z) {
    athena_physics_joint_set_ball_anchor(j, x, y, z);
}

void athena_physics_joint_get_hinge_anchor(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor(j, out);
}

void athena_physics_joint_get_hinge_anchor2(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor2(j, out);
}

void athena_physics_joint_set_hinge_axis(AthenaPhysicsJoint *j, float x, float y, float z) {
    if (!j) return;
    VECTOR w = { x, y, z, 0.0f };
    world_dir_to_body_local(j->body1, w, j->axis1_local);
    world_dir_to_body_local(j->body2, w, j->axis2_local);
}

void athena_physics_joint_get_hinge_axis(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_dir_to_world(j->body1, j->axis1_local, out);
}

float athena_physics_joint_get_hinge_angle(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    VECTOR q1 = { 0.0f, 0.0f, 0.0f, 1.0f }, q2 = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (j->body1) vu0_vec_copy(q1, j->body1->quat);
    if (j->body2) vu0_vec_copy(q2, j->body2->quat);
    VECTOR q1_conj, qrel, waxis;
    quat_conjugate(q1_conj, q1);
    quat_mul(qrel, q1_conj, q2);
    float s = j->axis1_local[0] * qrel[0] + j->axis1_local[1] * qrel[1] + j->axis1_local[2] * qrel[2];
    waxis[0] = qrel[0]; waxis[1] = qrel[1]; waxis[2] = qrel[2]; waxis[3] = 0.0f;
    float len = sqrtf(vu0_vec_dot(waxis, waxis));
    float angle = 2.0f * atan2f(len, qrel[3]);
    return (s >= 0.0f) ? angle : -angle;
}

float athena_physics_joint_get_hinge_angle_rate(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    VECTOR axis;
    body_local_dir_to_world(j->body1, j->axis1_local, axis);
    float w1 = j->body1 ? vu0_vec_dot(j->body1->angular_vel, axis) : 0.0f;
    float w2 = j->body2 ? vu0_vec_dot(j->body2->angular_vel, axis) : 0.0f;
    return w1 - w2;
}

void athena_physics_joint_add_hinge_torque(AthenaPhysicsJoint *j, float torque) {
    if (!j) return;
    VECTOR axis;
    body_local_dir_to_world(j->body1, j->axis1_local, axis);
    if (j->body1) athena_physics_body_add_torque(j->body1,  axis[0] * torque,  axis[1] * torque,  axis[2] * torque);
    if (j->body2) athena_physics_body_add_torque(j->body2, -axis[0] * torque, -axis[1] * torque, -axis[2] * torque);
}

void athena_physics_joint_set_slider_axis(AthenaPhysicsJoint *j, float x, float y, float z) {
    if (!j) return;
    VECTOR w = { x, y, z, 0.0f };
    world_dir_to_body_local(j->body1, w, j->axis1_local);
}

void athena_physics_joint_get_slider_axis(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_dir_to_world(j->body1, j->axis1_local, out);
}

float athena_physics_joint_get_slider_position(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    VECTOR axis, p1 = {0,0,0,0}, p2 = {0,0,0,0}, diff;
    body_local_dir_to_world(j->body1, j->axis1_local, axis);
    if (j->body1) vu0_vec_copy(p1, j->body1->position);
    if (j->body2) vu0_vec_copy(p2, j->body2->position);
    vu0_vec_sub(diff, p1, p2);
    return vu0_vec_dot(diff, axis);
}

float athena_physics_joint_get_slider_position_rate(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    VECTOR axis;
    body_local_dir_to_world(j->body1, j->axis1_local, axis);
    float v1 = j->body1 ? vu0_vec_dot(j->body1->linear_vel, axis) : 0.0f;
    float v2 = j->body2 ? vu0_vec_dot(j->body2->linear_vel, axis) : 0.0f;
    return v1 - v2;
}

void athena_physics_joint_add_slider_force(AthenaPhysicsJoint *j, float force) {
    if (!j) return;
    VECTOR axis;
    body_local_dir_to_world(j->body1, j->axis1_local, axis);
    if (j->body1) athena_physics_body_add_force(j->body1,  axis[0] * force,  axis[1] * force,  axis[2] * force);
    if (j->body2) athena_physics_body_add_force(j->body2, -axis[0] * force, -axis[1] * force, -axis[2] * force);
}

void athena_physics_joint_set_hinge2_anchor(AthenaPhysicsJoint *j, float x, float y, float z) {
    athena_physics_joint_set_ball_anchor(j, x, y, z);
}

void athena_physics_joint_get_hinge2_anchor(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor(j, out);
}

void athena_physics_joint_get_hinge2_anchor2(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor2(j, out);
}

void athena_physics_joint_set_hinge2_axis1(AthenaPhysicsJoint *j, float x, float y, float z) {
    if (!j) return;
    VECTOR w = { x, y, z, 0.0f };
    world_dir_to_body_local(j->body1, w, j->axis1_local);
}

void athena_physics_joint_get_hinge2_axis1(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_dir_to_world(j->body1, j->axis1_local, out);
}

void athena_physics_joint_set_hinge2_axis2(AthenaPhysicsJoint *j, float x, float y, float z) {
    if (!j) return;
    VECTOR w = { x, y, z, 0.0f };
    world_dir_to_body_local(j->body2, w, j->axis2_local);
}

void athena_physics_joint_get_hinge2_axis2(const AthenaPhysicsJoint *j, VECTOR out) {
    if (!j || !out) return;
    body_local_dir_to_world(j->body2, j->axis2_local, out);
}

float athena_physics_joint_get_hinge2_angle1(const AthenaPhysicsJoint *j) {
    return athena_physics_joint_get_hinge_angle(j);
}

float athena_physics_joint_get_hinge2_angle1_rate(const AthenaPhysicsJoint *j) {
    return athena_physics_joint_get_hinge_angle_rate(j);
}

float athena_physics_joint_get_hinge2_angle2_rate(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    VECTOR axis2;
    body_local_dir_to_world(j->body2, j->axis2_local, axis2);
    float w1 = j->body1 ? vu0_vec_dot(j->body1->angular_vel, axis2) : 0.0f;
    float w2 = j->body2 ? vu0_vec_dot(j->body2->angular_vel, axis2) : 0.0f;
    return w1 - w2;
}

void athena_physics_joint_add_hinge2_torques(AthenaPhysicsJoint *j, float torque1, float torque2) {
    if (!j) return;
    VECTOR a1, a2;
    body_local_dir_to_world(j->body1, j->axis1_local, a1);
    body_local_dir_to_world(j->body2, j->axis2_local, a2);
    VECTOR t = {
        a1[0] * torque1 + a2[0] * torque2,
        a1[1] * torque1 + a2[1] * torque2,
        a1[2] * torque1 + a2[2] * torque2,
        0.0f
    };
    if (j->body1) athena_physics_body_add_torque(j->body1,  t[0],  t[1],  t[2]);
    if (j->body2) athena_physics_body_add_torque(j->body2, -t[0], -t[1], -t[2]);
}

void athena_physics_joint_set_universal_anchor(AthenaPhysicsJoint *j, float x, float y, float z) {
    athena_physics_joint_set_ball_anchor(j, x, y, z);
}

void athena_physics_joint_get_universal_anchor(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor(j, out);
}

void athena_physics_joint_get_universal_anchor2(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_ball_anchor2(j, out);
}

void athena_physics_joint_set_universal_axis1(AthenaPhysicsJoint *j, float x, float y, float z) {
    athena_physics_joint_set_hinge2_axis1(j, x, y, z);
}

void athena_physics_joint_get_universal_axis1(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_hinge2_axis1(j, out);
}

void athena_physics_joint_set_universal_axis2(AthenaPhysicsJoint *j, float x, float y, float z) {
    athena_physics_joint_set_hinge2_axis2(j, x, y, z);
}

void athena_physics_joint_get_universal_axis2(const AthenaPhysicsJoint *j, VECTOR out) {
    athena_physics_joint_get_hinge2_axis2(j, out);
}

float athena_physics_joint_get_universal_angle1(const AthenaPhysicsJoint *j) {
    return athena_physics_joint_get_hinge_angle(j);
}

float athena_physics_joint_get_universal_angle2(const AthenaPhysicsJoint *j) {
    if (!j) return 0.0f;
    return j->amotor_angles[1];
}

float athena_physics_joint_get_universal_angle1_rate(const AthenaPhysicsJoint *j) {
    return athena_physics_joint_get_hinge2_angle1_rate(j);
}

float athena_physics_joint_get_universal_angle2_rate(const AthenaPhysicsJoint *j) {
    return athena_physics_joint_get_hinge2_angle2_rate(j);
}

void athena_physics_joint_add_universal_torques(AthenaPhysicsJoint *j, float torque1, float torque2) {
    athena_physics_joint_add_hinge2_torques(j, torque1, torque2);
}

void athena_physics_joint_set_fixed(AthenaPhysicsJoint *j) {
    if (!j) return;
    VECTOR p1 = {0,0,0,0};
    if (j->body1) vu0_vec_copy(p1, j->body1->position);
    athena_physics_joint_set_ball_anchor(j, p1[0], p1[1], p1[2]);
    athena_physics_joint_attach(j, j->body1, j->body2);
}

void athena_physics_joint_set_amotor_num_axes(AthenaPhysicsJoint *j, int num) {
    if (!j) return;
    if (num < 0) num = 0;
    if (num > 3) num = 3;
    j->amotor_num_axes = num;
}

int athena_physics_joint_get_amotor_num_axes(const AthenaPhysicsJoint *j) {
    return j ? j->amotor_num_axes : 0;
}

void athena_physics_joint_set_amotor_axis(AthenaPhysicsJoint *j, int anum, int rel, float x, float y, float z) {
    if (!j || anum < 0 || anum > 2) return;
    j->amotor_rel[anum] = rel;
    VECTOR w = { x, y, z, 0.0f };
    VECTOR *dst = (anum == 0) ? &j->axis1_local : ((anum == 1) ? &j->axis2_local : &j->axis3_local);
    if (rel == 1 && j->body1) {
        world_dir_to_body_local(j->body1, w, *dst);
    } else if (rel == 2 && j->body2) {
        world_dir_to_body_local(j->body2, w, *dst);
    } else {
        vu0_vec_normalize_len(*dst, w);
    }
}

void athena_physics_joint_get_amotor_axis(const AthenaPhysicsJoint *j, int anum, VECTOR out) {
    if (!j || !out || anum < 0 || anum > 2) return;
    const float *src = (anum == 0) ? j->axis1_local : ((anum == 1) ? j->axis2_local : j->axis3_local);
    int rel = j->amotor_rel[anum];
    if (rel == 1 && j->body1) {
        body_local_dir_to_world(j->body1, src, out);
    } else if (rel == 2 && j->body2) {
        body_local_dir_to_world(j->body2, src, out);
    } else {
        vu0_vec_copy(out, src);
    }
}

int athena_physics_joint_get_amotor_axis_rel(const AthenaPhysicsJoint *j, int anum) {
    if (!j || anum < 0 || anum > 2) return 0;
    return j->amotor_rel[anum];
}

void athena_physics_joint_set_amotor_angle(AthenaPhysicsJoint *j, int anum, float angle) {
    if (j && anum >= 0 && anum <= 2) j->amotor_angles[anum] = angle;
}

float athena_physics_joint_get_amotor_angle(const AthenaPhysicsJoint *j, int anum) {
    return (j && anum >= 0 && anum <= 2) ? j->amotor_angles[anum] : 0.0f;
}

float athena_physics_joint_get_amotor_angle_rate(const AthenaPhysicsJoint *j, int anum) {
    if (!j || anum < 0 || anum > 2) return 0.0f;
    VECTOR axis;
    athena_physics_joint_get_amotor_axis(j, anum, axis);
    float w1 = j->body1 ? vu0_vec_dot(j->body1->angular_vel, axis) : 0.0f;
    float w2 = j->body2 ? vu0_vec_dot(j->body2->angular_vel, axis) : 0.0f;
    return w1 - w2;
}

void athena_physics_joint_set_amotor_mode(AthenaPhysicsJoint *j, int mode) {
    if (j) j->amotor_mode = mode;
}

int athena_physics_joint_get_amotor_mode(const AthenaPhysicsJoint *j) {
    return j ? j->amotor_mode : 0;
}

void athena_physics_joint_add_amotor_torques(AthenaPhysicsJoint *j, float torque1, float torque2, float torque3) {
    if (!j) return;
    float t_vals[3] = { torque1, torque2, torque3 };
    VECTOR total = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < j->amotor_num_axes && i < 3; i++) {
        VECTOR ax;
        athena_physics_joint_get_amotor_axis(j, i, ax);
        vu0_vec_madd(total, total, ax, t_vals[i]);
    }
    if (j->body1) athena_physics_body_add_torque(j->body1,  total[0],  total[1],  total[2]);
    if (j->body2) athena_physics_body_add_torque(j->body2, -total[0], -total[1], -total[2]);
}

/* ============================================================================
 * World & VU0 Sequential Impulse (PGS) Constraint Solver
 * ============================================================================ */

AthenaPhysicsWorld *athena_physics_world_create(void) {
    AthenaPhysicsWorld *w = (AthenaPhysicsWorld *)phys_calloc_aligned(sizeof(AthenaPhysicsWorld));
    if (!w) return NULL;
    w->gravity[0] = 0.0f;
    w->gravity[1] = -9.81f;
    w->gravity[2] = 0.0f;
    w->gravity[3] = 0.0f;
    w->cfm = 1e-5f;
    w->erp = 0.2f;
    w->solver_iterations = 20;
    return w;
}

void athena_physics_world_destroy(AthenaPhysicsWorld *world) {
    if (!world) return;
    while (world->bodies) {
        athena_physics_body_destroy(world->bodies);
    }
    free(world);
}

void athena_physics_world_set_gravity(AthenaPhysicsWorld *world, float x, float y, float z) {
    if (!world) return;
    world->gravity[0] = x;
    world->gravity[1] = y;
    world->gravity[2] = z;
    world->gravity[3] = 0.0f;
}

void athena_physics_world_get_gravity(const AthenaPhysicsWorld *world, VECTOR out) {
    if (world && out) vu0_vec_copy(out, world->gravity);
}

void athena_physics_world_set_cfm(AthenaPhysicsWorld *world, float cfm) {
    if (world) world->cfm = cfm;
}

float athena_physics_world_get_cfm(const AthenaPhysicsWorld *world) {
    return world ? world->cfm : 0.0f;
}

void athena_physics_world_set_erp(AthenaPhysicsWorld *world, float erp) {
    if (world) world->erp = erp;
}

float athena_physics_world_get_erp(const AthenaPhysicsWorld *world) {
    return world ? world->erp : 0.0f;
}

void athena_physics_world_set_quick_step_iterations(AthenaPhysicsWorld *world, int iters) {
    if (world) world->solver_iterations = (iters > 0) ? iters : 1;
}

/* 16-byte aligned 1D Constraint Row for Projected Gauss-Seidel (PGS) Solver */
typedef struct AthenaSolverRow {
    VECTOR j_v1;     /* Linear Jacobian for body 1 */
    VECTOR j_w1;     /* Angular Jacobian for body 1 */
    VECTOR j_v2;     /* Linear Jacobian for body 2 */
    VECTOR j_w2;     /* Angular Jacobian for body 2 */
    VECTOR i_jw1;    /* Precomputed I1^-1 * j_w1 */
    VECTOR i_jw2;    /* Precomputed I2^-1 * j_w2 */
    AthenaPhysicsBody *b1;
    AthenaPhysicsBody *b2;
    float inv_m1;
    float inv_m2;
    float eff_mass;  /* 1.0f / (J * M^-1 * J^T + cfm) */
    float rhs;       /* Velocity bias (Baumgarte ERP + restitution) */
    float cfm;
    float lo;
    float hi;
    float lambda;
    float mu;
    int normal_row;  /* >= 0 if friction row bounded by [-mu*lambda_n, +mu*lambda_n] */
} __attribute__((aligned(16))) AthenaSolverRow;

/* Build an orthonormal tangent basis (t1, t2) perpendicular to unit normal n */
static void build_tangent_basis(const VECTOR n, VECTOR t1, VECTOR t2) {
    if (fabsf(n[0]) >= 0.57735f) {
        t1[0] =  n[1];
        t1[1] = -n[0];
        t1[2] =  0.0f;
    } else {
        t1[0] =  0.0f;
        t1[1] =  n[2];
        t1[2] = -n[1];
    }
    t1[3] = 0.0f;
    vu0_vec_normalize_len(t1, t1);
    vu0_vec_cross(t2, n, t1);
}

/* Initialize precomputed inertia terms and effective mass for a constraint row */
static int finalize_solver_row(AthenaSolverRow *row, AthenaPhysicsBody *b1, AthenaPhysicsBody *b2,
                               float cfm_per_dt, float rhs, float lo, float hi) {
    row->b1 = (b1 && b1->enabled && b1->inv_mass > 0.0f) ? b1 : NULL;
    row->b2 = (b2 && b2->enabled && b2->inv_mass > 0.0f) ? b2 : NULL;
    if (!row->b1 && !row->b2)
        return 0;

    float k = cfm_per_dt;
    if (row->b1) {
        row->inv_m1 = row->b1->inv_mass;
        vu0_mat3_mul_vec(row->i_jw1, row->b1->inv_inertia_world, row->j_w1);
        k += row->inv_m1 * vu0_vec_dot(row->j_v1, row->j_v1) + vu0_vec_dot(row->j_w1, row->i_jw1);
    } else {
        row->inv_m1 = 0.0f;
        vu0_vec_zero(row->i_jw1);
    }

    if (row->b2) {
        row->inv_m2 = row->b2->inv_mass;
        vu0_mat3_mul_vec(row->i_jw2, row->b2->inv_inertia_world, row->j_w2);
        k += row->inv_m2 * vu0_vec_dot(row->j_v2, row->j_v2) + vu0_vec_dot(row->j_w2, row->i_jw2);
    } else {
        row->inv_m2 = 0.0f;
        vu0_vec_zero(row->i_jw2);
    }

    if (k <= 1e-10f)
        return 0;

    row->eff_mass = 1.0f / k;
    row->rhs = rhs;
    row->cfm = cfm_per_dt;
    row->lo = lo;
    row->hi = hi;
    row->lambda = 0.0f;
    row->mu = 0.0f;
    row->normal_row = -1;
    return 1;
}

/* Add a point-to-point linear row along direction dir at anchor points r1, r2 */
static int add_linear_row(AthenaSolverRow *rows, int count, int max_rows,
                          AthenaPhysicsBody *b1, AthenaPhysicsBody *b2,
                          const VECTOR r1, const VECTOR r2, const VECTOR dir,
                          float pos_err, float erp_inv_dt, float cfm_inv_dt,
                          float lo, float hi) {
    if (count >= max_rows) return count;
    AthenaSolverRow *row = &rows[count];
    vu0_vec_copy(row->j_v1, dir);
    vu0_vec_cross(row->j_w1, r1, dir);
    vu0_vec_scale(row->j_v2, dir, -1.0f);
    vu0_vec_cross(row->j_w2, r2, row->j_v2);

    if (finalize_solver_row(row, b1, b2, cfm_inv_dt, pos_err * erp_inv_dt, lo, hi))
        return count + 1;
    return count;
}

/* Add an angular constraint row around world axis */
static int add_angular_row(AthenaSolverRow *rows, int count, int max_rows,
                           AthenaPhysicsBody *b1, AthenaPhysicsBody *b2,
                           const VECTOR axis, float ang_err, float erp_inv_dt, float cfm_inv_dt) {
    if (count >= max_rows) return count;
    AthenaSolverRow *row = &rows[count];
    vu0_vec_zero(row->j_v1);
    vu0_vec_copy(row->j_w1, axis);
    vu0_vec_zero(row->j_v2);
    vu0_vec_scale(row->j_w2, axis, -1.0f);

    if (finalize_solver_row(row, b1, b2, cfm_inv_dt, ang_err * erp_inv_dt, -1e20f, 1e20f))
        return count + 1;
    return count;
}

void athena_physics_world_step(AthenaPhysicsWorld *world, float step_size) {
    if (!world || step_size <= 1e-7f) return;

    float dt = step_size;
    float inv_dt = 1.0f / dt;
    float erp_inv_dt = world->erp * inv_dt;
    float world_cfm_inv_dt = world->cfm * inv_dt;

    /* 1. Integrate external forces and gravity into velocities */
    for (AthenaPhysicsBody *b = world->bodies; b; b = b->next) {
        if (!b->enabled || b->inv_mass <= 0.0f)
            continue;

        float lin_damp = fmaxf(0.0f, 1.0f - b->linear_damping * dt);
        float ang_damp = fmaxf(0.0f, 1.0f - b->angular_damping * dt);

        b->linear_vel[0] = (b->linear_vel[0] + (world->gravity[0] + b->force_acc[0] * b->inv_mass) * dt) * lin_damp;
        b->linear_vel[1] = (b->linear_vel[1] + (world->gravity[1] + b->force_acc[1] * b->inv_mass) * dt) * lin_damp;
        b->linear_vel[2] = (b->linear_vel[2] + (world->gravity[2] + b->force_acc[2] * b->inv_mass) * dt) * lin_damp;
        b->linear_vel[3] = 0.0f;

        VECTOR dw;
        vu0_mat3_mul_vec(dw, b->inv_inertia_world, b->torque_acc);
        b->angular_vel[0] = (b->angular_vel[0] + dw[0] * dt) * ang_damp;
        b->angular_vel[1] = (b->angular_vel[1] + dw[1] * dt) * ang_damp;
        b->angular_vel[2] = (b->angular_vel[2] + dw[2] * dt) * ang_damp;
        b->angular_vel[3] = 0.0f;
    }

    /* 2. Count active joints & allocate 16-byte aligned solver rows */
    int joint_count = 0;
    for (AthenaPhysicsJoint *j = world->joints; j; j = j->next) {
        joint_count++;
    }

    if (joint_count > 0) {
        int max_rows = joint_count * 6;
        AthenaSolverRow *rows = (AthenaSolverRow *)memalign(16, max_rows * sizeof(AthenaSolverRow));
        int row_count = 0;

        if (rows) {
            for (AthenaPhysicsJoint *j = world->joints; j; j = j->next) {
                AthenaPhysicsBody *b1 = j->body1;
                AthenaPhysicsBody *b2 = j->body2;

                /* If a contact joint was created from (g1, g2) and attach wasn't called,
                 * or if b1 is static (NULL) and b2 is dynamic, handle both seamlessly. */
                if (j->type == ATH_JOINT_CONTACT) {
                    if (!b1 && !b2) {
                        if (j->contact.g1) b1 = j->contact.g1->body;
                        if (j->contact.g2) b2 = j->contact.g2->body;
                    }
                    if (!b1 && !b2) continue;

                    const AthenaContact *c = &j->contact;
                    VECTOR r1 = {0,0,0,0}, r2 = {0,0,0,0};
                    if (b1) vu0_vec_sub(r1, c->pos, b1->position);
                    if (b2) vu0_vec_sub(r2, c->pos, b2->position);

                    /* Compute relative normal velocity at contact before solving for restitution */
                    VECTOR v1 = {0,0,0,0}, v2 = {0,0,0,0}, wxr;
                    if (b1) {
                        vu0_vec_cross(wxr, b1->angular_vel, r1);
                        vu0_vec_add(v1, b1->linear_vel, wxr);
                    }
                    if (b2) {
                        vu0_vec_cross(wxr, b2->angular_vel, r2);
                        vu0_vec_add(v2, b2->linear_vel, wxr);
                    }
                    VECTOR vrel;
                    vu0_vec_sub(vrel, v1, v2);
                    float vn = vu0_vec_dot(vrel, c->normal);

                    float cfm_dt = ((c->soft_cfm > 0.0f) ? c->soft_cfm : world->cfm) * inv_dt;
                    float slop = 0.005f;
                    float pen_bias = fmaxf(0.0f, c->depth - slop) * erp_inv_dt;
                    float bounce_bias = (vn < -0.5f && c->bounce > 0.0f) ? (-c->bounce * vn) : 0.0f;
                    float rhs = fmaxf(pen_bias, bounce_bias);

                    int n_idx = row_count;
                    if (row_count < max_rows) {
                        AthenaSolverRow *nrow = &rows[row_count];
                        vu0_vec_copy(nrow->j_v1, c->normal);
                        vu0_vec_cross(nrow->j_w1, r1, c->normal);
                        vu0_vec_scale(nrow->j_v2, c->normal, -1.0f);
                        vu0_vec_cross(nrow->j_w2, r2, nrow->j_v2);
                        if (finalize_solver_row(nrow, b1, b2, cfm_dt, rhs, 0.0f, 1e20f)) {
                            row_count++;

                            /* Add 2 Coulomb friction pyramid rows if mu > 0 */
                            if (c->mu > 0.0f && row_count + 2 <= max_rows) {
                                VECTOR t1, t2;
                                build_tangent_basis(c->normal, t1, t2);
                                const float *tangents[2] = { t1, t2 };
                                for (int t = 0; t < 2; t++) {
                                    AthenaSolverRow *frow = &rows[row_count];
                                    vu0_vec_copy(frow->j_v1, tangents[t]);
                                    vu0_vec_cross(frow->j_w1, r1, tangents[t]);
                                    vu0_vec_scale(frow->j_v2, tangents[t], -1.0f);
                                    vu0_vec_cross(frow->j_w2, r2, frow->j_v2);
                                    if (finalize_solver_row(frow, b1, b2, world_cfm_inv_dt, 0.0f, 0.0f, 0.0f)) {
                                        frow->mu = c->mu;
                                        frow->normal_row = n_idx;
                                        row_count++;
                                    }
                                }
                            }
                        }
                    }
                    continue;
                }

                /* General bilateral joints */
                if (!b1 && !b2) continue;

                VECTOR a1_w, a2_w, r1 = {0,0,0,0}, r2 = {0,0,0,0}, p_err;
                body_local_pt_to_world(b1, j->anchor1_local, a1_w);
                body_local_pt_to_world(b2, j->anchor2_local, a2_w);
                if (b1) vu0_vec_sub(r1, a1_w, b1->position);
                if (b2) vu0_vec_sub(r2, a2_w, b2->position);
                vu0_vec_sub(p_err, a2_w, a1_w);

                static const VECTOR world_axes[3] = {
                    { 1.0f, 0.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f, 0.0f }
                };

                switch (j->type) {
                    case ATH_JOINT_BALL: {
                        for (int k = 0; k < 3; k++) {
                            row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                       world_axes[k], p_err[k], erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                        }
                        break;
                    }
                    case ATH_JOINT_HINGE: {
                        for (int k = 0; k < 3; k++) {
                            row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                       world_axes[k], p_err[k], erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                        }
                        VECTOR h1, h2, t1, t2, herr;
                        body_local_dir_to_world(b1, j->axis1_local, h1);
                        body_local_dir_to_world(b2, j->axis2_local, h2);
                        build_tangent_basis(h1, t1, t2);
                        vu0_vec_cross(herr, h1, h2);
                        row_count = add_angular_row(rows, row_count, max_rows, b1, b2, t1, vu0_vec_dot(herr, t1), erp_inv_dt, world_cfm_inv_dt);
                        row_count = add_angular_row(rows, row_count, max_rows, b1, b2, t2, vu0_vec_dot(herr, t2), erp_inv_dt, world_cfm_inv_dt);
                        break;
                    }
                    case ATH_JOINT_SLIDER: {
                        VECTOR s_axis, t1, t2;
                        body_local_dir_to_world(b1, j->axis1_local, s_axis);
                        build_tangent_basis(s_axis, t1, t2);
                        row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                   t1, vu0_vec_dot(p_err, t1), erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                        row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                   t2, vu0_vec_dot(p_err, t2), erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                        for (int k = 0; k < 3; k++) {
                            row_count = add_angular_row(rows, row_count, max_rows, b1, b2, world_axes[k], 0.0f, erp_inv_dt, world_cfm_inv_dt);
                        }
                        break;
                    }
                    case ATH_JOINT_HINGE2:
                    case ATH_JOINT_UNIVERSAL: {
                        for (int k = 0; k < 3; k++) {
                            row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                       world_axes[k], p_err[k], erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                        }
                        VECTOR ax1, ax2, perp;
                        body_local_dir_to_world(b1, j->axis1_local, ax1);
                        body_local_dir_to_world(b2, j->axis2_local, ax2);
                        vu0_vec_cross(perp, ax1, ax2);
                        if (vu0_vec_normalize_len(perp, perp) > 1e-5f) {
                            float err = -vu0_vec_dot(ax1, ax2);
                            row_count = add_angular_row(rows, row_count, max_rows, b1, b2, perp, err, erp_inv_dt, world_cfm_inv_dt);
                        }
                        break;
                    }
                    case ATH_JOINT_FIXED: {
                        for (int k = 0; k < 3; k++) {
                            row_count = add_linear_row(rows, row_count, max_rows, b1, b2, r1, r2,
                                                       world_axes[k], p_err[k], erp_inv_dt, world_cfm_inv_dt, -1e20f, 1e20f);
                            row_count = add_angular_row(rows, row_count, max_rows, b1, b2, world_axes[k], 0.0f, erp_inv_dt, world_cfm_inv_dt);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            /* 3. Iterative Projected Gauss-Seidel (PGS) Velocity Solver using VU0 */
            int iters = (world->solver_iterations > 0) ? world->solver_iterations : 10;
            for (int iter = 0; iter < iters; iter++) {
                for (int r = 0; r < row_count; r++) {
                    AthenaSolverRow *row = &rows[r];
                    AthenaPhysicsBody *b1 = row->b1;
                    AthenaPhysicsBody *b2 = row->b2;

                    float jv = 0.0f;
                    if (b1) {
                        jv += vu0_vec_dot(row->j_v1, b1->linear_vel) +
                              vu0_vec_dot(row->j_w1, b1->angular_vel);
                    }
                    if (b2) {
                        jv += vu0_vec_dot(row->j_v2, b2->linear_vel) +
                              vu0_vec_dot(row->j_w2, b2->angular_vel);
                    }

                    float d_lambda = (row->rhs - jv - row->cfm * row->lambda) * row->eff_mass;
                    float old_lambda = row->lambda;
                    float new_lambda = old_lambda + d_lambda;

                    float lo = row->lo;
                    float hi = row->hi;
                    if (row->normal_row >= 0) {
                        hi = row->mu * rows[row->normal_row].lambda;
                        lo = -hi;
                    }

                    if (new_lambda < lo) new_lambda = lo;
                    if (new_lambda > hi) new_lambda = hi;
                    row->lambda = new_lambda;
                    d_lambda = new_lambda - old_lambda;

                    if (fabsf(d_lambda) > 1e-9f) {
                        if (b1) {
                            vu0_vec_madd(b1->linear_vel,  b1->linear_vel,  row->j_v1,  d_lambda * row->inv_m1);
                            vu0_vec_madd(b1->angular_vel, b1->angular_vel, row->i_jw1, d_lambda);
                        }
                        if (b2) {
                            vu0_vec_madd(b2->linear_vel,  b2->linear_vel,  row->j_v2,  d_lambda * row->inv_m2);
                            vu0_vec_madd(b2->angular_vel, b2->angular_vel, row->i_jw2, d_lambda);
                        }
                    }
                }
            }

            free(rows);
        }
    }

    /* 4. Semi-Implicit Euler Position & Quaternion Integration */
    for (AthenaPhysicsBody *b = world->bodies; b; b = b->next) {
        if (!b->enabled || b->inv_mass <= 0.0f) {
            vu0_vec_zero(b->force_acc);
            vu0_vec_zero(b->torque_acc);
            continue;
        }

        vu0_vec_madd(b->position, b->position, b->linear_vel, dt);

        /* Integrate orientation quaternion: dq/dt = 0.5 * omega * q */
        VECTOR wq = { b->angular_vel[0], b->angular_vel[1], b->angular_vel[2], 0.0f };
        VECTOR dq;
        quat_mul(dq, wq, b->quat);
        float half_dt = 0.5f * dt;
        b->quat[0] += dq[0] * half_dt;
        b->quat[1] += dq[1] * half_dt;
        b->quat[2] += dq[2] * half_dt;
        b->quat[3] += dq[3] * half_dt;

        float qlen = sqrtf(b->quat[0]*b->quat[0] + b->quat[1]*b->quat[1] +
                           b->quat[2]*b->quat[2] + b->quat[3]*b->quat[3]);
        if (qlen > 1e-8f) {
            float inv_q = 1.0f / qlen;
            b->quat[0] *= inv_q;
            b->quat[1] *= inv_q;
            b->quat[2] *= inv_q;
            b->quat[3] *= inv_q;
        } else {
            b->quat[0] = 0.0f; b->quat[1] = 0.0f; b->quat[2] = 0.0f; b->quat[3] = 1.0f;
        }

        quat_to_rot_matrix(b->rotation, b->quat);
        update_body_inertia_world(b);

        vu0_vec_zero(b->force_acc);
        vu0_vec_zero(b->torque_acc);
    }
}

void athena_physics_world_quick_step(AthenaPhysicsWorld *world, float step_size) {
    athena_physics_world_step(world, step_size);
}

/* ============================================================================
 * RenderObject Integration Hooks
 * ============================================================================ */

void athena_physics_update_geom_pos_rot(athena_object_data *obj) {
    if (!obj || !obj->collision)
        return;

    AthenaPhysicsGeom *geom = (AthenaPhysicsGeom *)obj->collision;
    athena_physics_geom_set_position(geom, obj->position[0], obj->position[1], obj->position[2]);

    MATRIX r;
    euler_to_rot_matrix(obj->rotation, r);
    float r9[9] = {
        r[0], r[1], r[2],
        r[4], r[5], r[6],
        r[8], r[9], r[10]
    };
    athena_physics_geom_set_rotation(geom, r9);

    if (fabsf(obj->scale[0]) > 1e-6f && fabsf(obj->scale[1]) > 1e-6f && fabsf(obj->scale[2]) > 1e-6f) {
        geom->scale[0] = obj->scale[0];
        geom->scale[1] = obj->scale[1];
        geom->scale[2] = obj->scale[2];
        geom->scale[3] = 0.0f;
    }
    geom->aabb_dirty = 1;
}

static void rot_matrix_to_euler(const MATRIX R, VECTOR out_euler) {
    float sy = fmaxf(-1.0f, fminf(1.0f, R[2]));
    float ry = asinf(sy);
    float rx, rz;
    if (fabsf(sy) < 0.9999f) {
        rx = atan2f(-R[6], R[10]);
        rz = atan2f(-R[1], R[0]);
    } else {
        rx = atan2f(R[9], R[5]);
        rz = 0.0f;
    }
    out_euler[0] = rx;
    out_euler[1] = ry;
    out_euler[2] = rz;
}

void athena_physics_update_body_pos_rot(athena_object_data *obj) {
    if (!obj || !obj->physics)
        return;

    AthenaPhysicsBody *body = (AthenaPhysicsBody *)obj->physics;
    const float *R = body->rotation;
    float sx = obj->scale[0], sy = obj->scale[1], sz = obj->scale[2];

    obj->position[0] = body->position[0];
    obj->position[1] = body->position[1];
    obj->position[2] = body->position[2];

    rot_matrix_to_euler(R, obj->rotation);

    obj->transform[0]  = R[0] * sx;
    obj->transform[1]  = R[4] * sx;
    obj->transform[2]  = R[8] * sx;
    obj->transform[3]  = 0.0f;

    obj->transform[4]  = R[1] * sy;
    obj->transform[5]  = R[5] * sy;
    obj->transform[6]  = R[9] * sy;
    obj->transform[7]  = 0.0f;

    obj->transform[8]  = R[2] * sz;
    obj->transform[9]  = R[6] * sz;
    obj->transform[10] = R[10] * sz;
    obj->transform[11] = 0.0f;

    obj->transform[12] = body->position[0];
    obj->transform[13] = body->position[1];
    obj->transform[14] = body->position[2];
    obj->transform[15] = 1.0f;
}

void updateGeomPosRot(athena_object_data *obj) {
    athena_physics_update_geom_pos_rot(obj);
}

void updateBodyPosRot(athena_object_data *obj) {
    athena_physics_update_body_pos_rot(obj);
}

void athena_physics_cleanup(void) {
    /* Stateless VU0/MMI allocator — resources are freed per world/space/geom/body */
}


