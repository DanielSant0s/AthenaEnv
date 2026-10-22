#ifndef F32STR_H
#define F32STR_H

#include <stdint.h>

/* Number.prototype.toString(10) of a float32 value; buf needs 24 bytes. */
int js_f32_to_string(float f, char *buf);

/* Number.prototype.toFixed(digits) of a float32 value; buf needs 24 bytes.
   Returns -1 outside the exact fast path (|f| >= 2^31, digits > 6, NaN,
   Infinity): the caller uses the generic path then. */
int js_f32_to_fixed(float f, int frac_digits, char *buf);

/* decimal of an int32; buf needs 12 bytes */
int js_i32_to_string(int32_t v, char *buf);

#endif
