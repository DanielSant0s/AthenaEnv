#include <math.h>
#include <vector.h>
#include <vif.h>
#include <mpg_manager.h>

void gen_vector_div(VECTOR out, VECTOR v0, VECTOR v1) {
    out[0] = v0[0] / v1[0];
    out[1] = v0[1] / v1[1];
    out[2] = v0[2] / v1[2];
    out[3] = v0[3] / v1[3];
}

void gen_vector_copy(VECTOR out, VECTOR v0) {
    __asm__ __volatile__(
        "lq    $2, 0x0(%1)\n"
        "sq    $2, 0x0(%0)\n"
        : : "r" (out) , "r" (v0) : "memory", "$2");
}

float vu0_vector_dot(VECTOR v0, VECTOR v1) {
    float ret;

    __asm__ __volatile__(
		"lqc2     $vf4, 0(%1)\n"
		"lqc2     $vf5, 0(%2)\n"
		"vmul.xyz $vf5, $vf4,  $vf5\n"
		"vaddy.x  $vf5, $vf5,  $vf5\n"
		"vaddz.x  $vf5, $vf5,  $vf5\n"
		"qmfc2    $2,   $vf5\n"
		"mtc1     $2,   %0\n"
	    : "=f" (ret) :"r" (v0) ,"r" (v1) :"$2", "memory" );

    return ret;
}

void vu0_vector_cross(VECTOR v0, VECTOR v1, VECTOR v2) {
    __asm__ __volatile__(
	    "lqc2   $vf4,0x0(%1)\n"
	    "lqc2   $vf5,0x0(%2)\n"
	    "vopmula.xyz	$ACC,$vf4,$vf5\n"
	    "vopmsub.xyz	$vf6,$vf5,$vf4\n"
	    "vsub.w $vf6,$vf6,$vf6		#vf6.xyz=0;\n"
	    "sqc2    $vf6,0x0(%0)\n"
	    : : "r" (v0) , "r" (v1) ,"r" (v2) : "memory");
}

void vu0_vector_normalize(VECTOR v0, VECTOR v1)
{
    __asm__ __volatile__(
        "lqc2    $vf4,0x0(%1)\n"
        "vmul.xyz $vf5,$vf4,$vf4\n"
        "vaddy.x $vf5,$vf5,$vf5\n"
        "vaddz.x $vf5,$vf5,$vf5\n"

        "vsqrt $Q,$vf5x\n"
        "vwaitq\n"
        "vaddq.x $vf5x,$vf0x,$Q\n"
        "vnop\n"
        "vnop\n"
        "vdiv    $Q,$vf0w,$vf5x\n"
        "vsub.xyzw $vf6,$vf0,$vf0           #vf6.xyzw=0;\n"
        "vwaitq\n"

        "vmulq.xyz  $vf6,$vf4,$Q\n"
        "sqc2    $vf6,0x0(%0)\n"
        : : "r" (v0) , "r" (v1) : "memory");
}

void vu0_vector_clamp(VECTOR v0, VECTOR v1, float min, float max) {
    __asm__ __volatile__(
        "mfc1    $8, %2\n"
        "mfc1    $9, %3\n"
	    "lqc2    $vf6, 0x0(%1)\n"
        "qmtc2    $8, $vf4\n"
        "qmtc2    $9, $vf5\n"
	    "vmaxx.xyzw $vf6, $vf6, $vf4\n"
	    "vminix.xyzw $vf6, $vf6, $vf5\n"
	    "sqc2    $vf6, 0x0(%0)\n"
	    : : "r" (v0) , "r" (v1), "f" (min), "f" (max):"$8","$9","memory");
}

void vu0_vector_add(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__(
	    "lqc2    $vf4,0x0(%1)\n"
	    "lqc2    $vf5,0x0(%2)\n"
	    "vadd.xyzw	$vf6,$vf4,$vf5\n"
	    "sqc2    $vf6,0x0(%0)\n"
	    : : "r" (v0) , "r" (v1), "r" (v2) : "memory");
}

void vu0_vector_sub(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__(
	    "lqc2    $vf4,0x0(%1)\n"
	    "lqc2    $vf5,0x0(%2)\n"
	    "vsub.xyzw	$vf6,$vf4,$vf5\n"
	    "sqc2    $vf6,0x0(%0)\n"
	    : : "r" (v0) , "r" (v1), "r" (v2) : "memory");
}

void vu0_vector_mul(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__(
	    "lqc2    $vf4,0x0(%1)\n"
	    "lqc2    $vf5,0x0(%2)\n"
	    "vmul.xyzw	$vf6,$vf4,$vf5\n"
	    "sqc2    $vf6,0x0(%0)\n"
	    : : "r" (v0) , "r" (v1), "r" (v2) : "memory");
}

float vu0_vector_get_length(VECTOR v) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void vu0_vector_set_length(VECTOR v, float newLength) {
    vu0_vector_normalize(v, v);
    v[0] *= newLength;
    v[1] *= newLength;
    v[2] *= newLength;
}

void vu0_vector_scale(VECTOR res, VECTOR v, float size) {
    __asm__ __volatile__("\n\
	    lqc2    $vf4, 0x0(%1)\n\
	    mfc1    $8,%2\n\
	    qmtc2   $8, $vf5\n\
	    vmulx.xyzw	$vf6, $vf4, $vf5\n\
	    sqc2    $vf6, 0x0(%0)\n\
	": : "r" (res) , "r" (v), "f" (size): "$8", "memory");
}

int vu0_vector_equals(VECTOR m0, VECTOR m1)
{
    int result;
    
    __asm__ __volatile__(
        "vsub.xyzw  $vf10, $vf0, $vf0\n"      

        "lqc2       $vf1, 0x0(%1)\n"          
        "lqc2       $vf2, 0x0(%2)\n"          
        "vsub.xyzw  $vf3, $vf1, $vf2\n"       
        "vabs.xyzw  $vf3, $vf3\n"             
        "vadd.xyzw  $vf10, $vf10, $vf3\n"     

        "vaddy.x    $vf11, $vf10, $vf10\n"    
        "vaddz.x    $vf11, $vf11, $vf10\n"    
        "vaddw.x    $vf11, $vf11, $vf10\n"    

        "qmfc2      %0, $vf11\n"
        "sltiu      %0, %0, 1\n" // result = (sum == 0) ? 1 : 0
        
        : "=r" (result)
        : "r" (m0), "r" (m1)
        : "memory"
    );
    
    return result;
}

void vu0_vector_interpolate(VECTOR v0, VECTOR v1, VECTOR v2, float t) {
    __asm__ __volatile__(
	    "lqc2    $vf4, 0x0(%1)\n"
	    "lqc2    $vf5, 0x0(%2)\n"
	    "mfc1    $8, %3\n"
	    "qmtc2    $8, $vf6\n"

        "vaddw.x    $vf7, $vf0, $vf0	#vf7.x=1;\n"
        "vsub.x     $vf8, $vf7, $vf6	#vf8.x=1-t;\n"
	    "vmulax.xyzw	$ACC, $vf4, $vf6\n"
	    "vmaddx.xyzw	$vf9, $vf5, $vf8\n"

	    "sqc2    $vf9, 0x0(%0)\n"
	: : "r" (v0) , "r" (v1), "r" (v2), "f" (t) : "$8", "memory");
}

static vector_ops vu0_functions = {
    .add=vu0_vector_add,
    .sub=vu0_vector_sub,
    .mul=vu0_vector_mul,
    .div=gen_vector_div,

    .dot=vu0_vector_dot,
    .cross=vu0_vector_cross,

    .get_length=vu0_vector_get_length,
    .set_length=vu0_vector_set_length,

    .normalize=vu0_vector_normalize,
    .scale=vu0_vector_scale,
    .clamp=vu0_vector_clamp,
    .interpolate=vu0_vector_interpolate,

    .equals=vu0_vector_equals,

    .copy=gen_vector_copy
};

vector_ops *vector_functions = &vu0_functions;
