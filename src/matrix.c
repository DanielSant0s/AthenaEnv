#include <math.h>
#include <matrix.h>
#include <vif.h>
#include <mpg_manager.h>

// Purely MMI accelerated functions, they can discard VU0 alternative since they're 128bit wide

void mmi_matrix_transpose(MATRIX m0, MATRIX m1) {
    __asm__ __volatile__(
	"lq $8,  0x00(%1)\n"
	"lq $9,  0x10(%1)\n"
	"lq $10, 0x20(%1)\n"
	"lq $11, 0x30(%1)\n"

	"pextlw     $12,$9,$8\n"
	"pextuw     $13,$9,$8\n"
	"pextlw     $14,$11,$10\n"
	"pextuw     $15,$11,$10\n"

	"pcpyld     $8,$14,$12\n"
	"pcpyud     $9,$12,$14\n"
	"pcpyld     $10,$15,$13\n"
	"pcpyud     $11,$13,$15\n"

	"sq $8,  0x00(%0)\n"
	"sq $9,  0x10(%0)\n"
	"sq $10, 0x20(%0)\n"
	"sq $11, 0x30(%0)\n"
	: : "r" (m0) , "r" (m1) : "memory");
}

void mmi_matrix_copy(MATRIX m0, MATRIX m1) {
    __asm__ __volatile__(
        "lq       $2, 0x00(%1)\n"
		"lq       $3, 0x10(%1)\n"
		"lq       $4, 0x20(%1)\n"
		"lq       $5, 0x30(%1)\n"

        "sq       $2, 0x00(%0)\n"
		"sq       $3, 0x10(%0)\n"
		"sq       $4, 0x20(%0)\n"
		"sq       $5, 0x30(%0)\n"
        
        : : "r" (m0), "r" (m1) : "memory", "$2", "$3", "$4", "$5"
    );
}

// VU0 (as COP2) accelerated functions

__asm__ (
    ".data\n"
    ".align 4\n"
    "S5432:\n"
    ".word	0x362e9c14, 0xb94fb21f, 0x3c08873e, 0xbe2aaaa4\n"
    ".text\n"
);

#define FLT_MAX 3.402823466e+38f

static const VECTOR ANGLE_CONSTANTS __attribute__((aligned(16))) = {
    M_PI*2,    // 2π 
    M_PI,    // π
    1.0f/(M_PI*2),    // 1/(2π)
    FLT_MAX                    
};

static void normalize_angle(VECTOR input, VECTOR output) {
    __asm__ __volatile__(
        "lqc2    $vf31,0x0(%2)\n"
        "lqc2    $vf1,0x0(%1)\n"

        "vaddy.xyzw $vf2, $vf1, $vf31 \n" // $vf2 = input + pi
        "vmulz.xyzw $vf3, $vf2, $vf31 \n" // $vf3 = $vf2 / (pi*2)

        // $vf4 = trunc($vf3)
        "vftoi0.xyzw $vf4, $vf3 \n" 
        "vitof0.xyzw $vf5, $vf4 \n" 

        "vmulx.xyzw $vf6, $vf5, $vf31 \n" // $vf5 = $vf4 * (pi*2)
        "vsub.xyzw $vf7, $vf2, $vf6 \n"

        // $vf11 = $vf7 + (($vf7 < 0) * 2*pi)
        "vmulw.xyzw $vf8, $vf7, $vf31  \n"
        "vmini.xyzw $vf8, $vf8, $vf0  \n"
        "vsub.xyzw $vf8, $vf0, $vf8 \n"
        "vminiw.xyzw $vf8, $vf8, $vf0  \n"
        "vmax.xyzw $vf8, $vf8, $vf0  \n"
        "vmulx.xyzw $vf9, $vf8, $vf31 \n"
        "vadd.xyz $vf11, $vf9, $vf7 \n"

        "vsuby.xyzw $vf12, $vf11, $vf31 \n"   // $vf12 = $vf11 - pi

        "sqc2         $vf12, 0x0(%0)          \n"
        : : "r" (output) , "r" (input), "r" (ANGLE_CONSTANTS) : "memory");
}

static void  __attribute__((used)) _ecossin(float t) {
    __asm__ __volatile__(
        "la	$8,S5432	\n"
        "lqc2	$vf5 ,0x0($8)	\n"
        "vmr32.w $vf6 ,$vf6 	\n"
        "vaddx.x $vf4 ,$vf0 ,$vf6 		\n"
        "vmul.x $vf6 ,$vf6 ,$vf6 		\n"
        "vmulx.yzw $vf4 ,$vf4 ,$vf0 			\n"
        "vmulw.xyzw $vf8 ,$vf5 ,$vf6 	\n"
        "vsub.xyzw $vf5 ,$vf0 ,$vf0  		\n"
        "vmulx.xyzw $vf8 ,$vf8 ,$vf6 		\n"
        "vmulx.xyz $vf8 ,$vf8 ,$vf6 		\n"
        "vaddw.x $vf4 ,$vf4 ,$vf8 				\n"
        "vmulx.xy $vf8 ,$vf8 ,$vf6 			\n"
        "vaddz.x $vf4 ,$vf4 ,$vf8 				\n"
        "vmulx.x $vf8 ,$vf8 ,$vf6 			\n"
        "vaddy.x $vf4 ,$vf4 ,$vf8 					\n"
        "vaddx.x $vf4 ,$vf4 ,$vf8 	 	\n"

        "vaddx.xy $vf4 ,$vf5 ,$vf4 	 			\n"

        "vmul.x $vf7 ,$vf4 ,$vf4 			\n"
        "vsubx.w $vf7 ,$vf0 ,$vf7 			\n"

        "vsqrt $Q, $vf7w			\n"
        "vwaitq							\n"

        "vaddq.x $vf7 ,$vf0 ,$Q     			\n"

        "bne	$7,$0,_ecossin_01				\n"
        "vaddx.x $vf4 ,$vf5 ,$vf7 				\n"
        "b	_ecossin_02					\n"
    "_ecossin_01:							\n"
        "vsubx.x $vf4 ,$vf5 ,$vf7 				\n"
    "_ecossin_02:							\n"
	: : :"$7","$8");
}

void matrix_rotate_z(MATRIX m0, MATRIX m1, float rz) {
    __asm__ __volatile__(
        "move   $9, %0\n"
        "move   $10, %1\n"
        "mtc1	$0,$f0\n"
        "c.lt.s %2,$f0\n"
        "li.s    $f0,1.57079637050628662109e0	\n"
        "bc1f    _matrix_rotate_z_01\n"
        "add.s   %2,$f0,%2			#rx=rx+π/2\n"
        "li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
        "j	_matrix_rotate_z_02\n"
    "_matrix_rotate_z_01:\n"
        "sub.s   %2,$f0,%2			#rx=π/2-rx\n"
        "move	$7,$0\n"
    "_matrix_rotate_z_02:\n"
        "mfc1    $8,%2\n"
        "qmtc2    $8,$vf6\n"
        "move	$6,$31	\n"
        
        "jal	_ecossin	# sin(roll), cos(roll)\n"
        "move	$31,$6	\n"
        "		#$vf5 :0,0,0,0 |x,y,z,w\n"
        "vmove.xyzw $vf6 ,$vf5 \n"
        "vmove.xyzw $vf7 ,$vf5 \n"
        "vmove.xyzw $vf9 ,$vf0 \n"
        "vsub.xyz $vf9 ,$vf9 ,$vf9  	#0,0,0,1 |x,y,z,w\n"
        "vmr32.xyzw $vf8 ,$vf9 	#0,0,1,0 |x,y,z,w\n"
        
        "vsub.zw $vf4 ,$vf4 ,$vf4  #$vf4 .zw=0 s,c,0,0 |x,y,z,w\n"
        "vaddx.y $vf6 ,$vf5 ,$vf4  #$vf6  0,s,0,0 |x,y,z,w\n"
        "vaddy.x $vf6 ,$vf5 ,$vf4  #$vf6  c,s,0,0 |x,y,z,w\n"
        "vsubx.x $vf7 ,$vf5 ,$vf4  #$vf7  -s,0,0,0 |x,y,z,w\n"
        "vaddy.y $vf7 ,$vf5 ,$vf4  #$vf7  -s,c,0,0 |x,y,z,w\n"
        
        "li    $7,4\n"
        "_loop_matrix_rotate_z:\n"
        "lqc2    $vf4,0x0($10)\n"
        "vmulax.xyzw	$ACC,   $vf6,$vf4\n"
        "vmadday.xyzw	$ACC,   $vf7,$vf4\n"
        "vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
        "vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
        "sqc2    $vf5,0x0($9)\n"
        "addi    $7,-1\n"
        "addiu    $10, 0x10\n"
        "addiu    $9, 0x10\n"
        "bne    $0,$7,_loop_matrix_rotate_z\n"
	: : "r" (m0) , "r" (m1), "f" (rz):"$6","$7","$8", "$9", "$10", "$f0", "memory");
}

void matrix_rotate_x(MATRIX m0, MATRIX m1, float rx) {
    __asm__ __volatile__(
        "move   $9, %0\n"
        "move   $10, %1\n"
        "mtc1	$0,$f0\n"
        "c.lt.s %2,$f0\n"
        "li.s    $f0,1.57079637050628662109e0	\n"
        "bc1f    _matrix_rotate_x_01\n"
        "add.s   %2,$f0,%2			#rx=rx+π/2\n"
        "li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
        "j	_matrix_rotate_x_02\n"
    "_matrix_rotate_x_01:\n"
        "sub.s   %2,$f0,%2			#rx=π/2-rx\n"
        "move	$7,$0\n"
    "_matrix_rotate_x_02:\n"

        "mfc1    $8,%2\n"
        "qmtc2    $8,$vf6\n"
        "move	$6,$31	\n"
        "jal	_ecossin	# sin(roll), cos(roll)\n"
        "move	$31,$6	\n"
        "		#$vf5 :0,0,0,0 |x,y,z,w\n"
        "vmove.xyzw $vf6 ,$vf5 \n"
        "vmove.xyzw $vf7 ,$vf5 \n"
        "vmove.xyzw $vf8 ,$vf5 \n"
        "vmove.xyzw $vf9 ,$vf5 \n"
        "vaddw.x $vf6 ,$vf5 ,$vf0  #$vf6  1,0,0,0 |x,y,z,w\n"
        "vaddw.w $vf9 ,$vf5 ,$vf0  #$vf9  0,0,0,1 |x,y,z,w\n"

        "vsub.zw $vf4 ,$vf4 ,$vf4  #$vf4 .zw=0 s,c,0,0 |x,y,z,w\n"
        "vaddx.z $vf7 ,$vf5 ,$vf4  #$vf7 .zw=0 0,0,s,0 |x,y,z,w\n"
        "vaddy.y $vf7 ,$vf5 ,$vf4  #$vf7 .zw=0 0,c,s,0 |x,y,z,w\n"
        "vsubx.y $vf8 ,$vf5 ,$vf4  #$vf8 .zw=0 0,-s,0,0 |x,y,z,w\n"
        "vaddy.z $vf8 ,$vf5 ,$vf4  #$vf8 .zw=0 0,-s,c,0 |x,y,z,w\n"

        "li    $7, 4 \n"

        "_loop_matrix_rotate_x: \n"
            "lqc2    $vf4, 0x0($10)\n"
            "vmulax.xyzw	$ACC,   $vf6,$vf4\n"
            "vmadday.xyzw	$ACC,   $vf7,$vf4\n"
            "vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
            "vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
            "sqc2    $vf5, 0x0($9)\n"
            "addi    $7,-1\n"
            "addiu  $10, 0x10 \n"
            "addiu  $9, 0x10 \n"
        "bne    $0,$7,_loop_matrix_rotate_x\n"
	: : "r" (m0) , "r" (m1), "f" (rx):"$6","$7","$8", "$9", "$10", "$f0","memory");
}

void matrix_rotate_y(MATRIX m0, MATRIX m1, float ry) {
    __asm__ __volatile__(
        "move   $9, %0\n"
        "move   $10, %1\n"
        "mtc1	$0,$f0\n"
        "c.lt.s %2,$f0\n"
        "li.s    $f0,1.57079637050628662109e0	\n"
        "bc1f    _matrix_rotate_y_01\n"
        "add.s   %2,$f0,%2			#rx=rx+π/2\n"
        "li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
        "j	_matrix_rotate_y_02\n"
    "_matrix_rotate_y_01:\n"
        "sub.s   %2,$f0,%2			#rx=π/2-rx\n"
        "move	$7,$0\n"
    "_matrix_rotate_y_02:\n"

        "mfc1    $8,%2\n"
        "qmtc2    $8,$vf6\n"
        "move	$6,$31	\n"
        "jal	_ecossin	# sin(roll), cos(roll)\n"
        "move	$31,$6	\n"
        "		#$vf5 :0,0,0,0 |x,y,z,w\n"
        "vmove.xyzw $vf6 ,$vf5 \n"
        "vmove.xyzw $vf7 ,$vf5 \n"
        "vmove.xyzw $vf8 ,$vf5 \n"
        "vmove.xyzw $vf9 ,$vf5 \n"
        "vaddw.y $vf7 ,$vf5 ,$vf0  #$vf7  0,1,0,0 |x,y,z,w\n"
        "vaddw.w $vf9 ,$vf5 ,$vf0  #$vf9  0,0,0,1 |x,y,z,w\n"

        "vsub.zw $vf4 ,$vf4 ,$vf4  #$vf4 .zw=0 s,c,0,0 |x,y,z,w\n"
        "vsubx.z $vf6 ,$vf5 ,$vf4  #$vf6  0,0,-s,0 |x,y,z,w\n"
        "vaddy.x $vf6 ,$vf5 ,$vf4  #$vf6  c,0,-s,0 |x,y,z,w\n"
        "vaddx.x $vf8 ,$vf5 ,$vf4  #$vf8  s,0,0,0 |x,y,z,w\n"
        "vaddy.z $vf8 ,$vf5 ,$vf4  #$vf8  s,0,c,0 |x,y,z,w\n"

        "li    $7, 4 \n"

        "_loop_matrix_rotate_y: \n"
            "lqc2    $vf4,0x0($10)\n"
            "vmulax.xyzw	$ACC,   $vf6,$vf4\n"
            "vmadday.xyzw	$ACC,   $vf7,$vf4\n"
            "vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
            "vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
            "sqc2    $vf5,0x0($9)\n"
            "addi    $7, -1\n"
            "addiu    $10, 0x10\n"
            "addiu    $9, 0x10\n"
        "bne    $0,$7,_loop_matrix_rotate_y\n"
	: : "r" (m0) , "r" (m1), "f" (ry):"$6","$7","$8", "$9", "$10", "$f0","memory");
}

void vu0_matrix_rotate(MATRIX m0, MATRIX m1, VECTOR rot) {
    normalize_angle(rot, rot);

    matrix_rotate_z(m0, m1, rot[2]);
    matrix_rotate_y(m0, m0, rot[1]);
    matrix_rotate_x(m0, m0, rot[0]);
}

void vu0_matrix_translate(MATRIX m0, MATRIX m1, VECTOR tv) {
    __asm__ __volatile__(
	"lqc2    $vf4,0x0(%2)\n"
	"lqc2    $vf5,0x30(%1)\n"

	"lq    $7,0x0(%1)\n"
	"lq    $8,0x10(%1)\n"
	"lq    $9,0x20(%1)\n"
	"vadd.xyz	$vf5,$vf5,$vf4\n"
	"sq    $7,0x0(%0)\n"
	"sq    $8,0x10(%0)\n"
	"sq    $9,0x20(%0)\n"
	"sqc2    $vf5,0x30(%0)\n"
	: : "r" (m0) , "r" (m1), "r" (tv):"$7","$8","$9","memory");
}

void vu0_matrix_scale(MATRIX output, MATRIX input0, VECTOR input1) {
    asm volatile (
        "lqc2       $vf1, 0x0(%2)        \n" 

        "lqc2       $vf4, 0x0(%1)        \n"   
        "lqc2       $vf5, 0x10(%1)       \n"   
        "lqc2       $vf6, 0x20(%1)       \n"   
        "lqc2       $vf7, 0x30(%1)       \n"   

        "vmulx.xyzw $vf8, $vf4, $vf1x   \n"   
        "vmuly.xyzw $vf9, $vf5, $vf1y   \n"   
        "vmulz.xyzw $vf10, $vf6, $vf1z  \n"   
        "vmove.xyzw $vf11, $vf7         \n"   

        "sqc2       $vf8,  0x0(%0)        \n"   
        "sqc2       $vf9,  0x10(%0)       \n"   
        "sqc2       $vf10, 0x20(%0)       \n"   
        "sqc2       $vf11, 0x30(%0)       \n"   
        :
        : "r" (output), "r" (input0), "r" (input1)  
        : 
    );
}

void vu0_matrix_multiply(MATRIX output, MATRIX input0, MATRIX input1) {
    asm __volatile__ (
        "lqc2		$vf1, 0x00(%1)	\n"
        "lqc2		$vf2, 0x10(%1)	\n"
        "lqc2		$vf3, 0x20(%1)	\n"
        "lqc2		$vf4, 0x30(%1)	\n"
        "lqc2		$vf5, 0x00(%2)	\n"
        "lqc2		$vf6, 0x10(%2)	\n"
        "lqc2		$vf7, 0x20(%2)	\n"
        "lqc2		$vf8, 0x30(%2)	\n"
        "vmulax.xyzw		$ACC, $vf5, $vf1\n"
        "vmadday.xyzw	$ACC, $vf6, $vf1\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf1\n"
        "vmaddw.xyzw		$vf1, $vf8, $vf1\n"
        "vmulax.xyzw		$ACC, $vf5, $vf2\n"
        "vmadday.xyzw	$ACC, $vf6, $vf2\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf2\n"
        "vmaddw.xyzw		$vf2, $vf8, $vf2\n"
        "vmulax.xyzw		$ACC, $vf5, $vf3\n"
        "vmadday.xyzw	$ACC, $vf6, $vf3\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf3\n"
        "vmaddw.xyzw		$vf3, $vf8, $vf3\n"
        "vmulax.xyzw		$ACC, $vf5, $vf4\n"
        "vmadday.xyzw	$ACC, $vf6, $vf4\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf4\n"
        "vmaddw.xyzw		$vf4, $vf8, $vf4\n"
        "sqc2		$vf1, 0x00(%0)	\n"
        "sqc2		$vf2, 0x10(%0)	\n"
        "sqc2		$vf3, 0x20(%0)	\n"
        "sqc2		$vf4, 0x30(%0)	\n"
        : : "r" (output), "r" (input0), "r" (input1)
        : "memory"
    );
}

void vu0_matrix_identity(MATRIX m0) {
    __asm__ __volatile__(
	"vsub.xyzw	$vf4,$vf0,$vf0 #vf4.xyzw=0;\n"
	"vadd.w	$vf4,$vf4,$vf0\n"
	"vmr32.xyzw	$vf5,$vf4\n"
	"vmr32.xyzw	$vf6,$vf5\n"
	"vmr32.xyzw	$vf7,$vf6\n"
	"sqc2   $vf4,0x30(%0)\n"
	"sqc2   $vf5,0x20(%0)\n"
	"sqc2   $vf6,0x10(%0)\n"
	"sqc2   $vf7,0x0(%0)\n"
	: : "r" (m0) : "memory");
}

void vu0_matrix_inverse(MATRIX m0, MATRIX m1)
{
    __asm__ __volatile__(
	"lq $8,0x0000(%1)\n"
	"lq $9,0x0010(%1)\n"
	"lq $10,0x0020(%1)\n"
	"lqc2 $vf4,0x0030(%1)\n"

	"vmove.xyzw $vf5,$vf4\n"
	"vsub.xyz $vf4,$vf4,$vf4		#vf4.xyz=0;\n"
	"vmove.xyzw $vf9,$vf4\n"
	"qmfc2    $11,$vf4\n"

	"pextlw     $12,$9,$8\n"
	"pextuw     $13,$9,$8\n"
	"pextlw     $14,$11,$10\n"
	"pextuw     $15,$11,$10\n"
	"pcpyld     $8,$14,$12\n"
	"pcpyud     $9,$12,$14\n"
	"pcpyld     $10,$15,$13\n"

	"qmtc2    $8,$vf6\n"
	"qmtc2    $9,$vf7\n"
	"qmtc2    $10,$vf8\n"

	"vmulax.xyz	$ACC,   $vf6,$vf5\n"
	"vmadday.xyz	$ACC,   $vf7,$vf5\n"
	"vmaddz.xyz	$vf4,$vf8,$vf5\n"
	"vsub.xyz	$vf4,$vf9,$vf4\n"
	"\n"
	"sq $8,0x0000(%0)\n"
	"sq $9,0x0010(%0)\n"
	"sq $10,0x0020(%0)\n"
	"sqc2 $vf4,0x0030(%0)\n"
	: : "r" (m0) , "r" (m1):"$8","$9","$10","$11","$12","$13","$14","$15", "memory");
}

void vu0_matrix_apply(VECTOR v0, MATRIX m0, VECTOR v1)
{
    __asm__ __volatile__(
	"lqc2   $vf4,0x0(%1)\n"
	"lqc2   $vf5,0x10(%1)\n"
	"lqc2   $vf6,0x20(%1)\n"
	"lqc2   $vf7,0x30(%1)\n"
	"lqc2   $vf8,0x0(%2)\n"
	"vmulax.xyzw	$ACC,  $vf4,$vf8\n"
	"vmadday.xyzw	$ACC,  $vf5,$vf8\n"
	"vmaddaz.xyzw	$ACC,  $vf6,$vf8\n"
	"vmaddw.xyzw	$vf9,$vf7,$vf8\n"
	"sqc2   $vf9,0x0(%0)\n"
	: : "r" (v0) , "r" (m0) ,"r" (v1): "memory");
}

int vu0_matrix_equals(MATRIX m0, MATRIX m1)
{
    int result;
    
    __asm__ __volatile__(
        "vsub.xyzw  $vf10, $vf0, $vf0\n"      

        "lqc2       $vf1, 0x0(%1)\n"          
        "lqc2       $vf2, 0x0(%2)\n"          
        "vsub.xyzw  $vf3, $vf1, $vf2\n"       
        "vabs.xyzw  $vf3, $vf3\n"             
        "vadd.xyzw  $vf10, $vf10, $vf3\n"     

        "lqc2       $vf1, 0x10(%1)\n"
        "lqc2       $vf2, 0x10(%2)\n"
        "vsub.xyzw  $vf3, $vf1, $vf2\n"
        "vabs.xyzw  $vf3, $vf3\n"
        "vadd.xyzw  $vf10, $vf10, $vf3\n"

        "lqc2       $vf1, 0x20(%1)\n"
        "lqc2       $vf2, 0x20(%2)\n"
        "vsub.xyzw  $vf3, $vf1, $vf2\n"
        "vabs.xyzw  $vf3, $vf3\n"
        "vadd.xyzw  $vf10, $vf10, $vf3\n"

        "lqc2       $vf1, 0x30(%1)\n"
        "lqc2       $vf2, 0x30(%2)\n"
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
        : "memory", "$f0"
    );
    
    return result;
}

static matrix_ops vu0_functions = {
    .identity=vu0_matrix_identity,
    .multiply=vu0_matrix_multiply,
    .inverse=vu0_matrix_inverse,
    .apply=vu0_matrix_apply,
    .transpose=mmi_matrix_transpose,
    .copy=mmi_matrix_copy,

    .translate=vu0_matrix_translate,
    .rotate=vu0_matrix_rotate,
    .scale=vu0_matrix_scale,

    .equals=vu0_matrix_equals
};

// EE core functions, w/o hardware acceleration

void core_matrix_identity(MATRIX output) {
    memset(output, 0, sizeof(MATRIX));
    output[0x00] = 1.00f;
    output[0x05] = 1.00f;
    output[0x0A] = 1.00f;
    output[0x0F] = 1.00f;
}

void core_matrix_multiply(MATRIX output, MATRIX input0, MATRIX input1) {
    output[0]  = input0[0] * input1[0] + input0[1] * input1[4] + input0[2] * input1[8]  + input0[3] * input1[12];
    output[1]  = input0[0] * input1[1] + input0[1] * input1[5] + input0[2] * input1[9]  + input0[3] * input1[13];
    output[2]  = input0[0] * input1[2] + input0[1] * input1[6] + input0[2] * input1[10] + input0[3] * input1[14];
    output[3]  = input0[0] * input1[3] + input0[1] * input1[7] + input0[2] * input1[11] + input0[3] * input1[15];

    output[4]  = input0[4] * input1[0] + input0[5] * input1[4] + input0[6] * input1[8]  + input0[7] * input1[12];
    output[5]  = input0[4] * input1[1] + input0[5] * input1[5] + input0[6] * input1[9]  + input0[7] * input1[13];
    output[6]  = input0[4] * input1[2] + input0[5] * input1[6] + input0[6] * input1[10] + input0[7] * input1[14];
    output[7]  = input0[4] * input1[3] + input0[5] * input1[7] + input0[6] * input1[11] + input0[7] * input1[15];

    output[8]  = input0[8] * input1[0] + input0[9] * input1[4] + input0[10] * input1[8]  + input0[11] * input1[12];
    output[9]  = input0[8] * input1[1] + input0[9] * input1[5] + input0[10] * input1[9]  + input0[11] * input1[13];
    output[10] = input0[8] * input1[2] + input0[9] * input1[6] + input0[10] * input1[10] + input0[11] * input1[14];
    output[11] = input0[8] * input1[3] + input0[9] * input1[7] + input0[10] * input1[11] + input0[11] * input1[15];

    output[12] = input0[12] * input1[0] + input0[13] * input1[4] + input0[14] * input1[8]  + input0[15] * input1[12];
    output[13] = input0[12] * input1[1] + input0[13] * input1[5] + input0[14] * input1[9]  + input0[15] * input1[13];
    output[14] = input0[12] * input1[2] + input0[13] * input1[6] + input0[14] * input1[10] + input0[15] * input1[14];
    output[15] = input0[12] * input1[3] + input0[13] * input1[7] + input0[14] * input1[11] + input0[15] * input1[15];
}

void core_matrix_inverse(MATRIX output, MATRIX input0) {
    MATRIX work;

    mmi_matrix_transpose(work, input0);
    work[0x03] = 0.00f;
    work[0x07] = 0.00f;
    work[0x0B] = 0.00f;
    work[0x0C] = -(input0[0x0C] * work[0x00] + input0[0x0D] * work[0x04] + input0[0x0E] * work[0x08]);
    work[0x0D] = -(input0[0x0C] * work[0x01] + input0[0x0D] * work[0x05] + input0[0x0E] * work[0x09]);
    work[0x0E] = -(input0[0x0C] * work[0x02] + input0[0x0D] * work[0x06] + input0[0x0E] * work[0x0A]);
    work[0x0F] = 1.00f;

    mmi_matrix_copy(output, work);
}

void core_matrix_apply(VECTOR output, MATRIX matrix, VECTOR input) {
    output[0] = matrix[0] * input[0] + matrix[1] * input[1] + matrix[2] * input[2] + matrix[3] * input[3];
    output[1] = matrix[4] * input[0] + matrix[5] * input[1] + matrix[6] * input[2] + matrix[7] * input[3];
    output[2] = matrix[8] * input[0] + matrix[9] * input[1] + matrix[10] * input[2] + matrix[11] * input[3];
    output[3] = matrix[12] * input[0] + matrix[13] * input[1] + matrix[14] * input[2] + matrix[15] * input[3];
}

void core_matrix_rotate(MATRIX output, MATRIX input0, VECTOR input1) {
    MATRIX work;

    core_matrix_identity(work);
    work[0x00] =  cosf(input1[2]);
    work[0x01] =  sinf(input1[2]);
    work[0x04] = -sinf(input1[2]);
    work[0x05] =  cosf(input1[2]);
    core_matrix_multiply(output, input0, work);

    core_matrix_identity(work);
    work[0x00] =  cosf(input1[1]);
    work[0x02] = -sinf(input1[1]);
    work[0x08] =  sinf(input1[1]);
    work[0x0A] =  cosf(input1[1]);
    core_matrix_multiply(output, output, work);

    core_matrix_identity(work);
    work[0x05] =  cosf(input1[0]);
    work[0x06] =  sinf(input1[0]);
    work[0x09] = -sinf(input1[0]);
    work[0x0A] =  cosf(input1[0]);
    core_matrix_multiply(output, output, work);
}

void core_matrix_scale(MATRIX output, MATRIX input0, VECTOR input1) {
    MATRIX work;

    core_matrix_identity(work);
    work[0x00] = input1[0];
    work[0x05] = input1[1];
    work[0x0A] = input1[2];
    core_matrix_multiply(output, input0, work);
}

void core_matrix_translate(MATRIX output, MATRIX input0, VECTOR input1) {
    MATRIX work;

    core_matrix_identity(work);
    work[0x0C] = input1[0];
    work[0x0D] = input1[1];
    work[0x0E] = input1[2];
    core_matrix_multiply(output, input0, work);
}

int core_matrix_equals(MATRIX m0, MATRIX m1) {
    float sum = 0.0f;
    
    for (int i = 0; i < 16; i++) {
        sum += fabsf(m0[i] - m1[i]);
    }
    
    return (sum == 0.0f) ? 1 : 0;
}

static matrix_ops core_functions = {
    .identity=core_matrix_identity,
    .multiply=core_matrix_multiply,
    .inverse=core_matrix_inverse,
    .apply=core_matrix_apply,
    .transpose=mmi_matrix_transpose,
    .copy=mmi_matrix_copy,

    .translate=core_matrix_translate,
    .rotate=core_matrix_rotate,
    .scale=core_matrix_scale,

    .equals=core_matrix_equals
};

matrix_ops *matrix_functions = &vu0_functions;
