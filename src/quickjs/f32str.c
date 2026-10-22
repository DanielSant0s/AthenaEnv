/*
 * float32 -> decimal string without doubles or printf.
 *
 * On the EE doubles are emulated in software and newlib's printf formats
 * floats through them, so a single "%g" costs tens of microseconds. These
 * routines only use 32/64-bit integer arithmetic:
 *
 *  - js_f32_to_string: the shortest digits that read back as the same
 *    float32 (Ryu f2s by Ulf Adams, Apache-2.0 / Boost-1.0), laid out with
 *    the Number.prototype.toString rules.
 *  - js_f32_to_fixed: Number.prototype.toFixed for float32 values, exact
 *    (the float is m * 2^e, so the scaled value is an exact integer ratio).
 */

#include <stdint.h>
#include <string.h>

#include "f32str.h"

#define FLOAT_MANTISSA_BITS 23
#define FLOAT_BIAS 127
#define FLOAT_POW5_INV_BITCOUNT 59
#define FLOAT_POW5_BITCOUNT 61

/* INV[i] = 2^(bitlen(5^i) - 1 + 59) / 5^i + 1; SPLIT[i] = 5^i scaled to
   61 significant bits (generated with exact integer arithmetic) */
static const uint64_t FLOAT_POW5_INV_SPLIT[31] = {
    UINT64_C(576460752303423489), UINT64_C(461168601842738791), UINT64_C(368934881474191033),
    UINT64_C(295147905179352826), UINT64_C(472236648286964522), UINT64_C(377789318629571618),
    UINT64_C(302231454903657294), UINT64_C(483570327845851670), UINT64_C(386856262276681336),
    UINT64_C(309485009821345069), UINT64_C(495176015714152110), UINT64_C(396140812571321688),
    UINT64_C(316912650057057351), UINT64_C(507060240091291761), UINT64_C(405648192073033409),
    UINT64_C(324518553658426727), UINT64_C(519229685853482763), UINT64_C(415383748682786211),
    UINT64_C(332306998946228969), UINT64_C(531691198313966350), UINT64_C(425352958651173080),
    UINT64_C(340282366920938464), UINT64_C(544451787073501542), UINT64_C(435561429658801234),
    UINT64_C(348449143727040987), UINT64_C(557518629963265579), UINT64_C(446014903970612463),
    UINT64_C(356811923176489971), UINT64_C(570899077082383953), UINT64_C(456719261665907162),
    UINT64_C(365375409332725730)
};

static const uint64_t FLOAT_POW5_SPLIT[47] = {
    UINT64_C(1152921504606846976), UINT64_C(1441151880758558720), UINT64_C(1801439850948198400),
    UINT64_C(2251799813685248000), UINT64_C(1407374883553280000), UINT64_C(1759218604441600000),
    UINT64_C(2199023255552000000), UINT64_C(1374389534720000000), UINT64_C(1717986918400000000),
    UINT64_C(2147483648000000000), UINT64_C(1342177280000000000), UINT64_C(1677721600000000000),
    UINT64_C(2097152000000000000), UINT64_C(1310720000000000000), UINT64_C(1638400000000000000),
    UINT64_C(2048000000000000000), UINT64_C(1280000000000000000), UINT64_C(1600000000000000000),
    UINT64_C(2000000000000000000), UINT64_C(1250000000000000000), UINT64_C(1562500000000000000),
    UINT64_C(1953125000000000000), UINT64_C(1220703125000000000), UINT64_C(1525878906250000000),
    UINT64_C(1907348632812500000), UINT64_C(1192092895507812500), UINT64_C(1490116119384765625),
    UINT64_C(1862645149230957031), UINT64_C(1164153218269348144), UINT64_C(1455191522836685180),
    UINT64_C(1818989403545856475), UINT64_C(2273736754432320594), UINT64_C(1421085471520200371),
    UINT64_C(1776356839400250464), UINT64_C(2220446049250313080), UINT64_C(1387778780781445675),
    UINT64_C(1734723475976807094), UINT64_C(2168404344971008868), UINT64_C(1355252715606880542),
    UINT64_C(1694065894508600678), UINT64_C(2117582368135750847), UINT64_C(1323488980084844279),
    UINT64_C(1654361225106055349), UINT64_C(2067951531382569187), UINT64_C(1292469707114105741),
    UINT64_C(1615587133892632177), UINT64_C(2019483917365790221)
};

static inline int32_t pow5bits(int32_t e) { return (int32_t)(((uint32_t)e * 1217359) >> 19) + 1; }
static inline uint32_t log10Pow2(int32_t e) { return ((uint32_t)e * 78913) >> 18; }
static inline uint32_t log10Pow5(int32_t e) { return ((uint32_t)e * 732923) >> 20; }

static inline uint32_t pow5factor_32(uint32_t value)
{
    uint32_t count = 0;
    for (;;) {
        uint32_t q = value / 5;
        if (value - 5 * q != 0)
            break;
        value = q;
        ++count;
    }
    return count;
}

static inline int multipleOfPowerOf5_32(uint32_t value, uint32_t p) { return pow5factor_32(value) >= p; }
static inline int multipleOfPowerOf2_32(uint32_t value, uint32_t p) { return (value & ((1u << p) - 1)) == 0; }

static inline uint32_t mulShift32(uint32_t m, uint64_t factor, int32_t shift)
{
    uint64_t bits0 = (uint64_t)m * (uint32_t)factor;
    uint64_t bits1 = (uint64_t)m * (uint32_t)(factor >> 32);
    uint64_t sum = (bits0 >> 32) + bits1;
    return (uint32_t)(sum >> (shift - 32));
}

/* shortest decimal: value = *out * 10^(*exponent) */
static void f2d(uint32_t ieeeMantissa, uint32_t ieeeExponent, uint32_t *out, int32_t *exponent)
{
    int32_t e2, e10, removed = 0;
    uint32_t m2, mv, mp, mm, mmShift, vr, vp, vm, output;
    int vmIsTrailingZeros = 0, vrIsTrailingZeros = 0, acceptBounds;
    uint8_t lastRemovedDigit = 0;

    if (ieeeExponent == 0) {
        e2 = 1 - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
        m2 = ieeeMantissa;
    } else {
        e2 = (int32_t)ieeeExponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
        m2 = (1u << FLOAT_MANTISSA_BITS) | ieeeMantissa;
    }
    acceptBounds = (m2 & 1) == 0;

    mv = 4 * m2;
    mp = 4 * m2 + 2;
    mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;
    mm = 4 * m2 - 1 - mmShift;

    if (e2 >= 0) {
        uint32_t q = log10Pow2(e2);
        int32_t k = FLOAT_POW5_INV_BITCOUNT + pow5bits((int32_t)q) - 1;
        int32_t i = -e2 + (int32_t)q + k;
        e10 = (int32_t)q;
        vr = mulShift32(mv, FLOAT_POW5_INV_SPLIT[q], i);
        vp = mulShift32(mp, FLOAT_POW5_INV_SPLIT[q], i);
        vm = mulShift32(mm, FLOAT_POW5_INV_SPLIT[q], i);
        if (q != 0 && (vp - 1) / 10 <= vm / 10) {
            int32_t l = FLOAT_POW5_INV_BITCOUNT + pow5bits((int32_t)q - 1) - 1;
            lastRemovedDigit = (uint8_t)(mulShift32(mv, FLOAT_POW5_INV_SPLIT[q - 1], -e2 + (int32_t)q - 1 + l) % 10);
        }
        if (q <= 9) {
            if (mv % 5 == 0)
                vrIsTrailingZeros = multipleOfPowerOf5_32(mv, q);
            else if (acceptBounds)
                vmIsTrailingZeros = multipleOfPowerOf5_32(mm, q);
            else
                vp -= multipleOfPowerOf5_32(mp, q);
        }
    } else {
        uint32_t q = log10Pow5(-e2);
        int32_t i = -e2 - (int32_t)q;
        int32_t k = pow5bits(i) - FLOAT_POW5_BITCOUNT;
        int32_t j = (int32_t)q - k;
        e10 = (int32_t)q + e2;
        vr = mulShift32(mv, FLOAT_POW5_SPLIT[i], j);
        vp = mulShift32(mp, FLOAT_POW5_SPLIT[i], j);
        vm = mulShift32(mm, FLOAT_POW5_SPLIT[i], j);
        if (q != 0 && (vp - 1) / 10 <= vm / 10) {
            j = (int32_t)q - 1 - (pow5bits(i + 1) - FLOAT_POW5_BITCOUNT);
            lastRemovedDigit = (uint8_t)(mulShift32(mv, FLOAT_POW5_SPLIT[i + 1], j) % 10);
        }
        if (q <= 1) {
            vrIsTrailingZeros = 1;
            if (acceptBounds)
                vmIsTrailingZeros = mmShift == 1;
            else
                --vp;
        } else if (q < 31) {
            vrIsTrailingZeros = multipleOfPowerOf2_32(mv, q - 1);
        }
    }

    if (vmIsTrailingZeros || vrIsTrailingZeros) {
        while (vp / 10 > vm / 10) {
            vmIsTrailingZeros &= vm % 10 == 0;
            vrIsTrailingZeros &= lastRemovedDigit == 0;
            lastRemovedDigit = (uint8_t)(vr % 10);
            vr /= 10; vp /= 10; vm /= 10;
            ++removed;
        }
        if (vmIsTrailingZeros) {
            while (vm % 10 == 0) {
                vrIsTrailingZeros &= lastRemovedDigit == 0;
                lastRemovedDigit = (uint8_t)(vr % 10);
                vr /= 10; vp /= 10; vm /= 10;
                ++removed;
            }
        }
        if (vrIsTrailingZeros && lastRemovedDigit == 5 && vr % 2 == 0)
            lastRemovedDigit = 4;   /* round to even */
        output = vr + ((vr == vm && (!acceptBounds || !vmIsTrailingZeros)) || lastRemovedDigit >= 5);
    } else {
        while (vp / 10 > vm / 10) {
            lastRemovedDigit = (uint8_t)(vr % 10);
            vr /= 10; vp /= 10; vm /= 10;
            ++removed;
        }
        output = vr + (vr == vm || lastRemovedDigit >= 5);
    }
    *out = output;
    *exponent = e10 + removed;
}

static int put_uint(char *p, uint32_t v)
{
    char tmp[12];
    int n = 0, i;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (i = 0; i < n; i++)
        p[i] = tmp[n - 1 - i];
    return n;
}

int js_f32_to_string(float f, char *buf)
{
    union { float f; uint32_t u; } bits;
    char digits[12];
    uint32_t mantissa, exponent, out;
    int32_t e10;
    int k, n, i, len = 0;

    bits.f = f;
    mantissa = bits.u & ((1u << FLOAT_MANTISSA_BITS) - 1);
    exponent = (bits.u >> FLOAT_MANTISSA_BITS) & 0xff;

    if (exponent == 0xff) {
        strcpy(buf, mantissa ? "NaN" : (bits.u >> 31) ? "-Infinity" : "Infinity");
        return (int)strlen(buf);
    }
    if (exponent == 0 && mantissa == 0) {
        strcpy(buf, "0");           /* -0 prints as 0 too */
        return 1;
    }
    if (bits.u >> 31)
        buf[len++] = '-';

    f2d(mantissa, exponent, &out, &e10);
    k = put_uint(digits, out);      /* value = digits * 10^e10 */
    n = k + e10;                    /* position of the decimal point */

    if (k <= n && n <= 21) {
        memcpy(buf + len, digits, k);
        len += k;
        for (i = k; i < n; i++)
            buf[len++] = '0';
    } else if (0 < n && n <= 21) {
        memcpy(buf + len, digits, n);
        len += n;
        buf[len++] = '.';
        memcpy(buf + len, digits + n, k - n);
        len += k - n;
    } else if (-6 < n && n <= 0) {
        buf[len++] = '0';
        buf[len++] = '.';
        for (i = 0; i < -n; i++)
            buf[len++] = '0';
        memcpy(buf + len, digits, k);
        len += k;
    } else {
        int e = n - 1;
        buf[len++] = digits[0];
        if (k > 1) {
            buf[len++] = '.';
            memcpy(buf + len, digits + 1, k - 1);
            len += k - 1;
        }
        buf[len++] = 'e';
        buf[len++] = e < 0 ? '-' : '+';
        len += put_uint(buf + len, (uint32_t)(e < 0 ? -e : e));
    }
    buf[len] = '\0';
    return len;
}

int js_f32_to_fixed(float f, int frac_digits, char *buf)
{
    static const uint32_t pow10[7] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };
    union { float f; uint32_t u; } bits;
    uint32_t mantissa, exponent;
    int32_t e2;
    uint64_t m, n, ip, fp;
    int len = 0, i;

    bits.f = f;
    mantissa = bits.u & ((1u << FLOAT_MANTISSA_BITS) - 1);
    exponent = (bits.u >> FLOAT_MANTISSA_BITS) & 0xff;

    /* finite, |f| < 2^31, up to 6 digits: exact in 64-bit integers */
    if (frac_digits < 0 || frac_digits > 6 || exponent >= FLOAT_BIAS + 31)
        return -1;

    if (exponent == 0) {
        m = mantissa;
        e2 = 1 - FLOAT_BIAS - FLOAT_MANTISSA_BITS;
    } else {
        m = (1u << FLOAT_MANTISSA_BITS) | mantissa;
        e2 = (int32_t)exponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS;
    }
    /* n = round(|f| * 10^d) with ties going up, as the spec picks the
       larger n */
    m *= pow10[frac_digits];                    /* < 2^24 * 10^6 < 2^44 */
    if (e2 >= 0)
        n = m << e2;                            /* e2 <= 7: < 2^51 */
    else if (-e2 >= 63)
        n = 0;                                  /* tiny: rounds to 0 */
    else
        n = (m + ((uint64_t)1 << (-e2 - 1))) >> -e2;

    ip = n / pow10[frac_digits];
    fp = n - ip * pow10[frac_digits];
    /* x < 0 gets "-" even when it rounds to zero; -0 does not */
    if ((bits.u >> 31) && (mantissa | exponent))
        buf[len++] = '-';
    len += put_uint(buf + len, (uint32_t)ip);
    if (frac_digits) {
        buf[len++] = '.';
        for (i = frac_digits - 1; i >= 0; i--) {
            buf[len + i] = (char)('0' + fp % 10);
            fp /= 10;
        }
        len += frac_digits;
    }
    buf[len] = '\0';
    return len;
}

int js_i32_to_string(int32_t v, char *buf)
{
    int len = 0;
    uint32_t u = (uint32_t)v;
    if (v < 0) {
        buf[len++] = '-';
        u = 0u - u;
    }
    len += put_uint(buf + len, u);
    buf[len] = '\0';
    return len;
}
