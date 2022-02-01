#include "include/render.h"

int clip_bounding_box(MATRIX local_clip, VECTOR *bounding_box)
{
    int ret;
    __asm__  __volatile__(
    "lqc2    $vf4, 0x00(%1)			\n"
    "lqc2    $vf5, 0x10(%1)			\n"
    "lqc2    $vf6, 0x20(%1)			\n"
    "lqc2    $vf7, 0x30(%1)			\n"
    "li      %0, 0x3f				\n"
    "li      $8, 8					\n"

	"loop_clip_all:					\n"
    "lqc2    $vf8, 0x00(%2)			\n"
	"addi	 %2, 0x10				\n"
    "vmulax.xyzw     $ACC,$vf4,$vf8	\n"
    "vmadday.xyzw    $ACC,$vf5,$vf8	\n"
    "vmaddaz.xyzw    $ACC,$vf6,$vf8	\n"
    "vmaddw.xyzw     $vf9,$vf7,$vf8	\n"
	"vclipw.xyz $vf9xyz,$vf9w		\n"
	"vnop							\n"
	"vnop							\n"
	"vnop							\n"
	"vnop							\n"
	"vnop							\n"
	"cfc2    $3,$vi18               \n"	//READ CLIP flag
	"and	%0,%0,$3				\n"
    "beqz    %0,end_clip_all		\n"
    "addi    $8,$8,-1				\n"
    "bne     $0,$8,loop_clip_all	\n"
    "addi    %0,$0,1				\n"
	"end_clip_all:					\n"
    :"=&r"(ret): "r" (local_clip),"r" (bounding_box)  : "$2","$3","$8");

	return ret;
}

void RotTransPersClipGsColN(vertex_f_t *output, MATRIX local_screen, VECTOR *verts, VECTOR *normals,
VECTOR *texels, VECTOR *colours, MATRIX local_light, MATRIX light_color, int count)
{
	__asm__ __volatile__(
	"lqc2	$vf4,0x00(%1)				\n"	//set local_world matrix[0]	
	"lqc2	$vf5,0x10(%1)				\n"	//set local_world matrix[1]	
	"lqc2	$vf6,0x20(%1)				\n"	//set local_world matrix[2]	
	"lqc2	$vf7,0x30(%1)				\n"	//set local_world matrix[3]	

	"lqc2	$vf17,0x00(%6)				\n"	//set local_light matrix[0]	
	"lqc2	$vf18,0x10(%6)				\n"	//set local_light matrix[1]	
	"lqc2	$vf19,0x20(%6)				\n"	//set local_light matrix[2]	

	"lqc2	$vf21,0x00(%7)				\n"	//set light_color matrix[0]	
	"lqc2	$vf22,0x10(%7)				\n"	//set light_color matrix[1]	
	"lqc2	$vf23,0x20(%7)				\n"	//set light_color matrix[2]	
	"lqc2	$vf20,0x30(%7)				\n"	//set light_color matrix[3]	

	"li      $2,0x4580					\n"
	"dsll    $2, 16						\n"
	"ori     $2, 0x4580					\n"
	"dsll    $2, 16						\n"
	"qmtc2   $2, $vf29					\n"
	"li      $2, 0x8000					\n"
	"ctc2    $2, $vi1					\n"
	"li      $9, 0x00					\n"

	"_rotTPNCGC_loop:					\n"
	"lqc2	$vf8,0x00(%2)				\n"	//load XYZ	
	"lqc2	$vf24,0x00(%4)				\n"	//load NORMAL
	"lqc2	$vf25,0x00(%5)				\n"	//load COLOR	
	"lqc2	$vf27,0x00(%8)				\n"	//load ST	

	"vmulax.xyzw    $ACC, $vf4,$vf8		\n"
	"vmadday.xyzw   $ACC, $vf5,$vf8		\n"
	"vmaddaz.xyzw   $ACC, $vf6,$vf8		\n"
	"vmaddw.xyzw    $vf12,$vf7,$vf8		\n"
	"vdiv    		$Q,$vf0w,$vf12w		\n"
	"vwaitq								\n"
	"vmulq.xyz		$vf12,$vf12,$Q		\n"
	"vftoi4.xyzw	$vf13,$vf12			\n"

	"#GS CLIP							\n"
	"vnop								\n"
	"vnop								\n"
	"ctc2    		$0,$vi16           	\n"	//clear status flag
	"vsub.xyw  		$vf0,$vf12,$vf0 	\n"	//(z,ZBz,PPy,PPx)-(1,0,0,0);	
	"vsub.xy  		$vf0,$vf29,$vf12  	\n"	//(Zmax,0,Ymax,Xmax) -(z,ZBz,PPy,PPx)
	"vnop                            	\n"
	"vnop                            	\n"
	"vnop								\n"
	"vnop								\n"
	"sll     		$9,1				\n"
	"andi    		$9,$9,6				\n"
	"cfc2    		$2,$vi16            \n"	//read status flag
	"andi    		$2,$2,0xc0			\n"
	"beqz    		$2,_rotTPNCGC_skip1	\n"
	"ori     		$9,$9,1				\n"
	"_rotTPNCGC_skip1:					\n"
	"beqz    		$9,_rotTPNCGC_skip2	\n"
	"vmfir.w 		$vf13,$vi1			\n"
	"_rotTPNCGC_skip2:					\n"


	//"vmulax.xyzw    $ACC, $vf17,$vf24	\n"
	//"vmadday.xyzw   $ACC, $vf18,$vf24	\n"
	//"vmaddz.xyzw    $vf24,$vf19,$vf24	\n"

	//"vmaxx.xyz	  $vf24,$vf24,$vf0	\n"

	//"vmulax.xyzw    $ACC,$vf21,$vf24	\n"
	//"vmadday.xyzw   $ACC,$vf22,$vf24	\n"
	//"vmaddaz.xyzw   $ACC,$vf23,$vf24	\n"
	//"vmaddw.xyzw    $vf24,$vf20,$vf0	\n"

	//"vmul.xyzw	  $vf26,$vf24,$vf25	\n"

	"vmulq.xyz		$vf28,$vf27,$Q		\n"

	//"vmaxx.xyz	  $vf26,$vf26,$vf0	\n"
	//"lui			  $2,0x437f			\n"
	//"ctc2			  $2,$vi21			\n"
	//"vnop								\n"
	//"vnop								\n"
	//"vminii.xyz	  $vf26,$vf26,$I	\n"
	//"vftoi0.xyzw	  $vf26,$vf26		\n"

	//"sqc2	$vf26,0x04(%0)				\n"	//store RGBA 
	//"addi	%0,0x10						\n"				
	"sqc2	$vf12,0x00(%0)				\n"	//store XYZ 	
	"addi	%0,0x10						\n"
	"sqc2	$vf28,0x00(%0)				\n"	//store STQ 	
	"addi	%0,0x10						\n"				

	"addi	%2,0x10						\n"
	//"addi	%4,0x10						\n"
	//"addi	%5,0x10						\n"
	"addi	%8,0x10						\n"
	"addi	%3,-1						\n"
	"bne	$0,%3,_rotTPNCGC_loop		\n"
	: : "r" (output), "r" (local_screen) ,"r" (verts), "r" (count) ,"r" (normals) ,"r" (colours), "r" (local_light) ,"r" (light_color),"r"(texels) :"$2","$9", "memory");
}

void calculate_vertices_no_clip(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen) {
	asm __volatile__ (
	"lqc2		$vf1, 0x00(%3)	\n"	//set local_screen matrix[0]
	"lqc2		$vf2, 0x10(%3)	\n"	//set local_screen matrix[1]
	"lqc2		$vf3, 0x20(%3)	\n"	//set local_screen matrix[2]
	"lqc2		$vf4, 0x30(%3)	\n"	//set local_screen matrix[3]

	"calcvertnoclip_loop:	\n"
	"lqc2		$vf6, 0x00(%2)	\n" //load XYZ
	"vmulaw		$ACC, $vf4, $vf0	\n"
	"vmaddax		$ACC, $vf1, $vf6	\n"
	"vmadday		$ACC, $vf2, $vf6	\n"
	"vmaddz		$vf7, $vf3, $vf6	\n"
	"vdiv    	$Q,	$vf0w,	$vf7w		\n"
	"vwaitq								\n"
	"vmulq.xyz	$vf7,	$vf7, $Q		\n"
	"vnop								\n"
	"sqc2		$vf7, 0x00(%0)	\n" //Store XYZ
	"addi		%0, 0x10	\n"
	"addi		%2, 0x10	\n"
	"addi		%1, -1		\n"
	"bne		$0, %1, calcvertnoclip_loop	\n"
	: : "r" (output), "r" (count), "r" (vertices), "r" (local_screen) : "$10", "memory"
	);
}

void calculate_vertices_clipped(VECTOR *output,  int count, VECTOR *vertices, MATRIX local_screen) {
	asm __volatile__ (
	"lqc2		$vf1, 0x00(%3)	    \n"	//set local_screen matrix[0]
	"lqc2		$vf2, 0x10(%3)	    \n"	//set local_screen matrix[1]
	"lqc2		$vf3, 0x20(%3)	    \n"	//set local_screen matrix[2]
	"lqc2		$vf4, 0x30(%3)	    \n"	//set local_screen matrix[3]
	// GS CLIP
	"li      $2,0x4580				\n"
	"dsll    $2, 16					\n"
	"ori     $2, 0x4580				\n"
	"dsll    $2, 16					\n"
	"qmtc2   $2,	$vf29			\n"
	"li      $2,	0x8000			\n"
	"ctc2    $2,	$vi1			\n"
	"li      $9,	0x00			\n"

	"calcvertices_loop:	            \n"
	"lqc2		$vf6, 0x00(%2)	    \n" //load XYZ
	"vmulaw		$ACC, $vf4, $vf0	\n"
	"vmaddax	$ACC, $vf1, $vf6	\n"
	"vmadday	$ACC, $vf2, $vf6	\n"
	"vmaddz		$vf7, $vf3, $vf6	\n"
	"vdiv    	$Q,	$vf0w,	$vf7w		\n"
	"vwaitq								\n"
	"vmulq.xyz		$vf7,	$vf7, $Q	\n"
	"vftoi4.xyzw	$vf8,	$vf7		\n"
	//GS CLIP
	"vnop							 \n"
	"vnop							 \n"
	"ctc2    $0,	$vi16            \n"	//clear status flag
	"vsub.xyw  $vf0, $vf7,	$vf0     \n"	//(z,ZBz,PPy,PPx)-(1,0,0,0);
	"vsub.xy   $vf0, $vf29,	$vf7  	 \n"	//(Zmax,0,Ymax,Xmax) -(z,ZBz,PPy,PPx)
	"vnop                            \n"
	"vnop                            \n"
	"vnop							 \n"
	"vnop							 \n"
	"sll     $9,	1				 \n"
	"andi    $9,	$9, 6			 \n"
	"cfc2    $2,	$vi16            \n"	//read status flag
	"andi    $2,	$2,0xc0			 \n"
	"beqz    $2,	calcvertices_skip1\n"
	"ori     $9,	$9,1			 \n"

	"calcvertices_skip1:			 \n"
	"beqz    $9,calcvertices_skip2	 \n"
	"vmfir.w $vf8,	$vi1			 \n"

	"calcvertices_skip2:		     \n"
	"sqc2		$vf7, 0x00(%0)	     \n" //Store XYZ
	"addi		%0, 0x10	         \n"
	"addi		%2, 0x10	         \n"
	"addi		%1, -1		         \n"
	"bne		$0, %1, calcvertices_loop	\n"
	: : "r" (output), "r" (count), "r" (vertices), "r" (local_screen) : "$10", "memory"
	);
}

int draw_convert_rgbq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours, unsigned char alpha)
{

	int i;
	float q = 1.00f;

	// For each colour...
	for (i=0;i<count;i++)
	{

		// Calculate the Q value.
		if (vertices[i].w != 0)
		{

			q = 1 / vertices[i].w;

		}

		// Calculate the RGBA values.
		output[i].r = (int)(colours[i].r * 128.0f);
		output[i].g = (int)(colours[i].g * 128.0f);
		output[i].b = (int)(colours[i].b * 128.0f);
		output[i].a = alpha;
		output[i].q = q;

	}

	// End function.
	return 0;

}

int draw_convert_rgbaq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours)
{

	int i;
	float q = 1.00f;

	// For each colour...
	for (i=0;i<count;i++)
	{

		// Calculate the Q value.
		if (vertices[i].w != 0)
		{

			q = 1 / vertices[i].w;

		}

		// Calculate the RGBA values.
		output[i].r = (int)(colours[i].r * 128.0f);
		output[i].g = (int)(colours[i].g * 128.0f);
		output[i].b = (int)(colours[i].b * 128.0f);
		output[i].a = (int)(colours[i].a * 128.0f);
		output[i].q = q;

	}

	// End function.
	return 0;

}

int draw_convert_st(texel_t *output, int count, vertex_f_t *vertices, texel_f_t *coords)
{

	int i = 0;
	float q = 1.00f;

	// For each coordinate...
	for (i=0;i<count;i++)
	{

		// Calculate the Q value.
		if (vertices[i].w != 0)
		{
			q = 1 / vertices[i].w;
		}

		// Calculate the S and T values.
		output[i].s = coords[i].s * q;
		output[i].t = coords[i].t * q;

	}

	// End function.
	return 0;
}

int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices)
{

	int i;

	int center_x;
	int center_y;

	unsigned int max_z;

	center_x = ftoi4(x);
	center_y = ftoi4(y);

	max_z = 1 << (z - 1);

	// For each colour...
	for (i=0;i<count;i++)
	{

		// Calculate the XYZ values.
		output[i].x = (short)((vertices[i].x + 1.0f) * center_x);
		output[i].y = (short)((vertices[i].y + 1.0f) * -center_y);
		output[i].z = (unsigned int)((vertices[i].z + 1.0f) * max_z);

	}

	// End function.
	return 0;

}