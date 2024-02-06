
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "include/athena_math.h"

// Took from Tyra

extern volatile const uint32_t ATHENA_MATH_ATAN_TABLE[9] = {
    0x3f7ffff5, 0xbeaaa61c, 0x3e4c40a6, 0xbe0e6c63, 0x3dc577df,
    0xbd6501c4, 0x3cb31652, 0xbb84d7e7, 0x3f490fdb,
};

extern volatile const float ATHENA_MATH_ATAN_TABLE2[8] = {
    0.0f, M_PI_2, M_PI_2, M_PI, -M_PI, -M_PI_2, -M_PI_2, 0.0f,
};

float athena_cosf(float x) {
  float r;
  asm volatile(
      "lui     $9,  0x3f00        \n\t"
      ".set noreorder             \n\t"
      ".align 3                   \n\t"
      "abs.s   %0,  %1            \n\t"
      "lui     $8,  0xbe22        \n\t"
      "mtc1    $9,  $f1           \n\t"
      "ori     $8,  $8,    0xf983 \n\t"
      "mtc1    $8,  $f8           \n\t"
      "lui     $9,  0x4b00        \n\t"
      "mtc1    $9,  $f3           \n\t"
      "lui     $8,  0x3f80        \n\t"
      "mtc1    $8,  $f2           \n\t"
      "mula.s  %0,  $f8           \n\t"
      "msuba.s $f3, $f2           \n\t"
      "madda.s $f3, $f2           \n\t"
      "lui     $8,  0x40c9        \n\t"
      "msuba.s %0,  $f8           \n\t"
      "ori     $8,  0x0fdb        \n\t"
      "msub.s  %0,  $f1,   $f2    \n\t"
      "lui     $9,  0xc225        \n\t"
      "abs.s   %0,  %0            \n\t"
      "lui     $10, 0x3e80        \n\t"
      "mtc1    $10, $f7           \n\t"
      "ori     $9,  0x5de1        \n\t"
      "sub.s   %0,  %0,    $f7    \n\t"
      "lui     $10, 0x42a3        \n\t"
      "mtc1    $8,  $f3           \n\t"
      "ori     $10, 0x3458        \n\t"
      "mtc1    $9,  $f4           \n\t"
      "lui     $8,  0xc299        \n\t"
      "mtc1    $10, $f5           \n\t"
      "ori     $8,  0x2663        \n\t"
      "mul.s   $f8, %0,    %0     \n\t"
      "lui     $9,  0x421e        \n\t"
      "mtc1    $8,  $f6           \n\t"
      "ori     $9,  0xd7bb        \n\t"
      "mtc1    $9,  $f7           \n\t"
      "nop                        \n\t"
      "mul.s   $f1, %0,    $f8    \n\t"
      "mul.s   $f9, $f8,   $f8    \n\t"
      "mula.s  $f3, %0            \n\t"
      "mul.s   $f2, $f1,   $f8    \n\t"
      "madda.s $f4, $f1           \n\t"
      "mul.s   $f1, $f1,   $f9    \n\t"
      "mul.s   %0,  $f2,   $f9    \n\t"
      "madda.s $f5, $f2           \n\t"
      "madda.s $f6, $f1           \n\t"
      "madd.s  %0,  $f7,   %0     \n\t"
      ".set reorder               \n\t"
      : "=&f"(r)
      : "f"(x)
      : "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7", "$f8", "$f9", "$8",
        "$9", "$10");
  return r;
}

float athena_atan2f(float y, float x) {
  float r;
  asm volatile(
      "mtc1		$0, %0      \n\t"
      "abs.s		$f1, %1   \n\t"
      "abs.s		$f2, %2   \n\t"
      "c.lt.s		%2, %0    \n\t"
      "move		$9, $0      \n\t"
      "bc1f		_atan_00    \n\t"
      "addiu		$9, $9, 4   \n\t"
      "_atan_00:            \n\t"
      "c.eq.s		%2, %0      \n\t"
      "bc1f		_atan_00_1    \n\t"
      "c.eq.s		%1, %2      \n\t"
      "bc1t		_atan_06      \n\t"
      "c.lt.s		%1, %0      \n\t"
      "bc1f		_atan_00_1    \n\t"
      "addiu		$9, $0, 3	    \n\t"
      "_atan_00_1:		    \n\t"
      "mul.s		%1, %1, %2    \n\t"
      "c.lt.s		%1, %0	    \n\t"
      "bc1f		_atan_01    \n\t"
      "addiu		$9, $9, 2			    \n\t"
      "c.lt.s		$f2, $f1	    \n\t"
      "bc1f		_atan_02    \n\t"
      "addiu		$9, $9, 1	    \n\t"
      "b			_atan_02	    \n\t"
      "_atan_01:			    \n\t"
      "c.lt.s		$f1, $f2    \n\t"
      "bc1f		_atan_02    \n\t"
      "addiu		$9, $9, 1	    \n\t"
      "_atan_02:			    \n\t"
      "c.lt.s		$f1, $f2    \n\t"
      "bc1f		_atan_03    \n\t"
      "mov.s		%1, $f2    \n\t"
      "mov.s		%2, $f1    \n\t"
      "b			_atan_04    \n\t"
      "_atan_03:    \n\t"
      "mov.s		%1, $f1    \n\t"
      "mov.s		%2, $f2    \n\t"
      "_atan_04:    \n\t"
      "mfc1		$6, %1    \n\t"
      "mfc1		$7, %2    \n\t"
      "la			$8, ATHENA_MATH_ATAN_TABLE    \n\t"
      "lqc2		$vf4, 0x0($8)    \n\t"
      "lqc2		$vf5, 0x10($8)    \n\t"
      "lqc2		$vf6, 0x20($8)    \n\t"
      "qmtc2		$6, $vf21    \n\t"
      "qmtc2		$7, $vf22    \n\t"
      "vadd.x		$vf23, $vf21, $vf22    \n\t"
      "vsub.x		$vf22, $vf22, $vf21    \n\t"
      "vdiv     $Q, $vf22x, $vf23x    \n\t"
      "vwaitq    \n\t"
      "vaddq.x  $vf21, $vf0, $Q		    \n\t"
      "vmul.x		$vf22, $vf21, $vf21    \n\t"
      "vmulax.x	$ACC, $vf21, $vf4	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmadday.x	$ACC, $vf21, $vf4	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmaddaz.x	$ACC, $vf21, $vf4	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmaddaw.x	$ACC, $vf21, $vf4	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmaddax.x	$ACC, $vf21, $vf5	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmadday.x	$ACC, $vf21, $vf5	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmaddaz.x	$ACC, $vf21, $vf5	    \n\t"
      "vmul.x		$vf21, $vf21, $vf22    \n\t"
      "vmaddaw.x	$ACC, $vf21, $vf5	    \n\t"
      "vmaddw.x	$vf21, $vf6, $vf0    \n\t"
      "qmfc2		$6, $vf21    \n\t"
      "mtc1		$6, %0    \n\t"
      "andi		$8, $9, 1    \n\t"
      "sll			$9, $9, 2    \n\t"
      "la			$7, ATHENA_MATH_ATAN_TABLE2    \n\t"
      "add			$9, $9, $7    \n\t"
      "lw			$7, 0x0($9)    \n\t"
      "mtc1		$7, %1    \n\t"
      "beq			$8, $0, _atan_05    \n\t"
      "sub.s		%0, %1, %0    \n\t"
      "b			_atan_06    \n\t"
      "_atan_05:    \n\t"
      "add.s		%0, %1, %0    \n\t"
      "_atan_06:    \n\t"
      : "=&f"(r)
      : "f"(x), "f"(y)
      : "$6", "$7", "$8", "$9", "$f0", "$f1", "$f2");

  return r;
}

float athena_randomf(float min, float max) {
  float random = ((float)rand()) / (float)RAND_MAX;
  float diff = max - min;
  float r = random * diff;
  return min + r;
}

int athena_randomi(int min, int max) {
  return rand() % (max - min + 1) + min;
}

float athena_asinf(float x) {
  float r;
  asm volatile(
      "lui     $9,  0x3f00        \n\t"
      ".set noreorder             \n\t"
      ".align 3                   \n\t"
      "abs.s   %0,  %1            \n\t"
      "lui     $8,  0xbe22        \n\t"
      "mtc1    $9,  $f1           \n\t"
      "ori     $8,  $8,    0xf983 \n\t"
      "mtc1    $8,  $f8           \n\t"
      "lui     $9,  0x4b00        \n\t"
      "mtc1    $9,  $f3           \n\t"
      "lui     $8,  0x3f80        \n\t"
      "mtc1    $8,  $f2           \n\t"
      "mula.s  %0,  $f8           \n\t"
      "msuba.s $f3, $f2           \n\t"
      "madda.s $f3, $f2           \n\t"
      "lui     $8,  0x40c9        \n\t"
      "msuba.s %0,  $f8           \n\t"
      "ori     $8,  0x0fdb        \n\t"
      "msub.s  %0,  $f1,   $f2    \n\t"
      "lui     $9,  0xc225        \n\t"
      "abs.s   %0,  %0            \n\t"
      "lui     $10, 0x3e80        \n\t"
      "mtc1    $10, $f7           \n\t"
      "ori     $9,  0x5de1        \n\t"
      "sub.s   %0,  %0,    $f7    \n\t"
      "lui     $10, 0x42a3        \n\t"
      "mtc1    $8,  $f3           \n\t"
      "ori     $10, 0x3458        \n\t"
      "mtc1    $9,  $f4           \n\t"
      "lui     $8,  0xc299        \n\t"
      "mtc1    $10, $f5           \n\t"
      "ori     $8,  0x2663        \n\t"
      "mul.s   $f8, %0,    %0     \n\t"
      "lui     $9,  0x421e        \n\t"
      "mtc1    $8,  $f6           \n\t"
      "ori     $9,  0xd7bb        \n\t"
      "mtc1    $9,  $f7           \n\t"
      "nop                        \n\t"
      "mul.s   $f1, %0,    $f8    \n\t"
      "mul.s   $f9, $f8,   $f8    \n\t"
      "mula.s  $f3, %0            \n\t"
      "mul.s   $f2, $f1,   $f8    \n\t"
      "madda.s $f4, $f1           \n\t"
      "mul.s   $f1, $f1,   $f9    \n\t"
      "mul.s   %0,  $f2,   $f9    \n\t"
      "madda.s $f5, $f2           \n\t"
      "madda.s $f6, $f1           \n\t"
      "madd.s  %0,  $f7,   %0     \n\t"
      ".set reorder               \n\t"
      : "=&f"(r)
      : "f"(x)
      : "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7", "$f8", "$f9", "$8",
        "$9", "$10");
  return r;
}

float athena_acosf(float x) {
  float y = sqrtf(1.0f - x * x);
  float t = athena_atan2f(y, x);
  return t;
}

float athena_sinf(float x) { return athena_cosf(x - M_PI_2); }

float athena_tanf(float x) { return athena_sinf(x) / athena_cosf(x); }



