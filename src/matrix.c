#include <matrix.h>
#include <vif.h>
#include <mpg_manager.h>

register_vu_program(VU0MatrixMul);

vu_mpg *vu0_mat_mul = NULL;

void init_vu0_matrix() {
    vu0_mat_mul = vu_mpg_load_buffer(embed_vu_code_ptr(VU0MatrixMul), embed_vu_code_size(VU0MatrixMul), VECTOR_UNIT_0, false); 
}

void vu0_matrix_multiply(MATRIX output, MATRIX input0, MATRIX input1) {
    int mpg_addr = vu_mpg_preload(vu0_mat_mul, false);

    asm __volatile__ (
        "ctc2       %3,	  $vi27	    \n"
        "lqc2		$vf1, 0x00(%1)	\n"
        "lqc2		$vf2, 0x10(%1)	\n"
        "lqc2		$vf3, 0x20(%1)	\n"
        "lqc2		$vf4, 0x30(%1)	\n"
        "lqc2		$vf5, 0x00(%2)	\n"
        "lqc2		$vf6, 0x10(%2)	\n"
        "lqc2		$vf7, 0x20(%2)	\n"
        "lqc2		$vf8, 0x30(%2)	\n"
        "vcallmsr   $vi27           \n"

        "qmfc2.i    $2,   $vf10	    \n" // trick to wait execution
        "sq 		$2,    0x00(%0)	\n"
        "sqc2		$vf11, 0x10(%0)	\n"
        "sqc2		$vf12, 0x20(%0)	\n"
        "sqc2		$vf13, 0x30(%0)	\n"
        : : "r" (output), "r" (input0), "r" (input1), "r" (mpg_addr)
        : "memory", "$2", "$3", "$4", "$5"
    );
 }