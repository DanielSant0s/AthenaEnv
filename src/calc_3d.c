#include "include/render.h"
#include <math.h>

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

unsigned int get_max_z(GSGLOBAL* gsGlobal)
{

	int z;
	unsigned int max_z;

	switch(gsGlobal->PSMZ){
		case GS_PSMZ_32:
			z = 32;
			break;

		case GS_PSMZ_24:
			z = 24;
			break;

		case GS_PSMZ_16:
		case GS_PSMZ_16S:
			z = 16;
			break;
		
		default:
			return -1;
	}

	max_z = 1 << (z - 1);

	// End function.
	return max_z;

}

/*
int athena_process_xyz_rgbaq(GSPRIMPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices)
{
	float q = 1.00f;

	int center_x = gsGlobal->Width/2;
	int center_y = gsGlobal->Height/2;
	unsigned int max_z = get_max_z(gsGlobal);

	for (int i = 0; i < count; i++)
	{
		// Calculate the Q value.
		if (vertices[i].w != 0)
		{
			q = 1 / vertices[i].w;
		}

		output[i].rgbaq.color.r = (int)(colours[i].r * 128.0f);
		output[i].rgbaq.color.g = (int)(colours[i].g * 128.0f);
		output[i].rgbaq.color.b = (int)(colours[i].b * 128.0f);
		output[i].rgbaq.color.a = 0x80;
		output[i].rgbaq.color.q = q;
		output[i].rgbaq.tag = GS_RGBAQ;

		output[i].xyz2.xyz.x = gsKit_float_to_int_x(gsGlobal, (vertices[i].x + 1.0f) * center_x);
		output[i].xyz2.xyz.y = gsKit_float_to_int_y(gsGlobal, (vertices[i].y + 1.0f) * center_y);
		output[i].xyz2.xyz.z = (unsigned int)((vertices[i].z + 1.0f) * max_z);
		output[i].xyz2.tag = GS_XYZ2;

	}

	// End function.
	return 0;

}

int athena_process_xyz_rgbaq_st(GSPRIMSTQPOINT *output, GSGLOBAL* gsGlobal, int count, color_f_t *colours, vertex_f_t *vertices, texel_f_t *coords)
{
	float q = 1.00f;

	int center_x = 2048.0f+gsGlobal->Width/2;
	int center_y = 2048.0f+gsGlobal->Height/2;
	unsigned int max_z = get_max_z(gsGlobal);

	for (int i = 0; i < count; i++)
	{
		// Calculate the Q value.
		if (vertices[i].w != 0)
		{
			q = 1 / vertices[i].w;
		}

		output[i].rgbaq.color.r = (int)(colours[i].r * 128.0f);
		output[i].rgbaq.color.g = (int)(colours[i].g * 128.0f);
		output[i].rgbaq.color.b = (int)(colours[i].b * 128.0f);
		output[i].rgbaq.color.a = 0x80;
		output[i].rgbaq.color.q = q;
		output[i].rgbaq.tag = GS_RGBAQ;

		output[i].stq.st.s = coords[i].s * q;
		output[i].stq.st.t = coords[i].t * q;
		output[i].stq.tag = GS_ST;

		output[i].xyz2.xyz.x = (int)(((vertices[i].x + 1.0f) * center_x) * 16.0f) ;
		output[i].xyz2.xyz.y = (int)(((vertices[i].y + 1.0f) * center_y) * 16.0f) ;
		output[i].xyz2.xyz.z = (unsigned int)((vertices[i].z + 1.0f) * max_z);
		output[i].xyz2.tag = GS_XYZ2;

	}

	// End function.
	return 0;

}*/

void athena_line_goraud_3d(GSGLOBAL *gsGlobal, float x1, float y1, int iz1, float x2, float y2, int iz2, u64 color1, u64 color2)
{
	u64* p_store;
	u64* p_data;
	int qsize = 3;
	int bsize = 48;

	int center_x = 2048.0f+gsGlobal->Width/2;
	int center_y = 2048.0f+gsGlobal->Height/2;
	unsigned int max_z = get_max_z(gsGlobal);

	int ix1 = (int)(((x1 + 1.0f) * center_x) * 16.0f);
	int iy1 = (int)(((y1 + 1.0f) * center_y) * 16.0f);

	int ix2 = (int)(((x2 + 1.0f) * center_x) * 16.0f);
	int iy2 = (int)(((y2 + 1.0f) * center_y) * 16.0f);

	p_store = p_data = gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_LINE);

	if(p_store == gsGlobal->CurQueue->last_tag)
	{
		*p_data++ = GIF_TAG_LINE_GORAUD(0);
		*p_data++ = GIF_TAG_LINE_GORAUD_REGS;
	}

	*p_data++ = GS_SETREG_PRIM( GS_PRIM_PRIM_LINE, 1, 0, gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
				0, gsGlobal->PrimContext, 0) ;

	*p_data++ = color1;
	*p_data++ = GS_SETREG_XYZ2( ix1, iy1, (unsigned int)((iz1 + 1.0f) * max_z) );

	*p_data++ = color2;
	*p_data++ = GS_SETREG_XYZ2( ix2, iy2, (unsigned int)((iz2 + 1.0f) * max_z) );
}

static inline u32 lzw(u32 val)
{
	u32 res;
	__asm__ __volatile__ ("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
	return(res);
}

void athena_set_tw_th(const GSTEXTURE *Texture, int *tw, int *th)
{
	*tw = 31 - (lzw(Texture->Width) + 1);
	if(Texture->Width > (1<<*tw))
		(*tw)++;

	*th = 31 - (lzw(Texture->Height) + 1);
	if(Texture->Height > (1<<*th))
		(*th)++;
}

void SetRow(MATRIX output, int row, VECTOR vec) {
    for (int i = 0; i < 4; i++) {
        output[row * 4 + i] = vec[i];
    }
}

void SetColumn(MATRIX output, int col, VECTOR vec) {
    for (int i = 0; i < 4; i++) {
        output[i * 4 + col] = vec[i];
    }
}

float vu0_innerproduct(VECTOR v0, VECTOR v1)
{
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
/*
void vu0_build_lights(VECTOR *output, int count, VECTOR *normals, LightData* lights) {	
	float intensity;

	for (int i = 0; i < count; i++) {
		output[i][0] = 0.0f;
		output[i][1] = 0.0f;
		output[i][2] = 0.0f;
		output[i][3] = 0.0f;
		
		for (int j = 0; j < 4; j++) {
    		output[i][0] += lights->ambient[j][0];
    		output[i][1] += lights->ambient[j][1];
    		output[i][2] += lights->ambient[j][2];
    		output[i][3] = 1.00f;

    		intensity = -vu0_innerproduct(normals[i], lights->direction[j]);
    		// Clamp the minimum intensity.
    		if (intensity < 0.00f) { intensity = 0.00f; }

   			// If the light has intensity...
   			if (intensity > 0.00f) {
    			// Add the light value.
    			output[i][0] += (lights->diffuse[j][0] * intensity);
    			output[i][1] += (lights->diffuse[j][1] * intensity);
    			output[i][2] += (lights->diffuse[j][2] * intensity);
    			output[i][3] = 1.00f;
   			}
  		}
 	}
}

void vu0_calculate_lights(VECTOR *output, int count, VECTOR *normals, VECTOR *light_direction, VECTOR *light_colour, const int *light_type, int light_count) {	
	float intensity;

	for (int i = 0; i < count; i++) {
		output[i][0] = 0.0f;
		output[i][1] = 0.0f;
		output[i][2] = 0.0f;
		output[i][3] = 0.0f;
		for (int j = 0; j < light_count; j++) {
   			if (light_type[j] == LIGHT_AMBIENT)  {
    			intensity = 1.00f;

   			} else if (light_type[j] == LIGHT_DIRECTIONAL)  {
    			intensity = -vu0_innerproduct(normals[i], light_direction[j]);
    			// Clamp the minimum intensity.
    			if (intensity < 0.00f) { intensity = 0.00f; }
   				// Else, this is an invalid light type.

   			} else { 
				intensity = 0.00f; 
			}

   			// If the light has intensity...
   			if (intensity > 0.00f) {
    			// Add the light value.
    			output[i][0] += (light_colour[j][0] * intensity);
    			output[i][1] += (light_colour[j][1] * intensity);
    			output[i][2] += (light_colour[j][2] * intensity);
    			output[i][3] = 1.00f;
   			}
  		}
 	}
}*/

void vu0_vector_clamp(VECTOR v0, VECTOR v1, float min, float max)
{
    __asm__ __volatile__(
        "mfc1         $8,    %2\n"
        "mfc1         $9,    %3\n"
		"lqc2         $vf6,  0(%1)\n"
        "qmtc2        $8,    $vf4\n"
        "qmtc2        $9,    $vf5\n"
		"vmaxx.xyzw   $vf6, $vf6, $vf4\n"
		"vminix.xyzw  $vf6, $vf6, $vf5\n"
		"sqc2         $vf6,  0(%0)\n"
	: : "r" (v0) , "r" (v1), "f" (min), "f" (max):"$8","$9","memory");
}

/*void vu0_calculate_colours(VECTOR *output, int count, VECTOR *colours, VECTOR *lights) {
  	for (int i = 0; i < count; i++) {
   		// Apply the light value to the colour.
		__asm__ __volatile__(
			"lqc2     $vf4, 0(%1)\n"
			"lqc2     $vf5, 0(%2)\n"
			"vmul.xyz $vf6, $vf4,  $vf5\n"
			"sqc2     $vf6, 0(%0)\n"
		: :"r" (output[i]) , "r" (colours[i]) ,"r" (lights[i]) : "memory" );

   		vu0_vector_clamp(output[i], output[i], 0.00f, 1.99f);
	}

}*/

void UnitMatrix(MATRIX m0)
{
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

void OuterProduct(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__
	(
	"lqc2   $vf4,0x0(%1)\n"
	"lqc2   $vf5,0x0(%2)\n"
	"vopmula.xyz	$ACC,$vf4,$vf5\n"
	"vopmsub.xyz	$vf6,$vf5,$vf4\n"
	"vsub.w $vf6,$vf6,$vf6		#vf6.xyz=0;\n"
	"sqc2    $vf6,0x0(%0)\n"
	: : "r" (v0) , "r" (v1) ,"r" (v2) : "memory");
}

void Normalize(VECTOR v0, VECTOR v1)
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

void TransMatrix(MATRIX m0, MATRIX m1, VECTOR tv)
{
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

void InversMatrix(MATRIX m0, MATRIX m1)
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

void ApplyMatrix(VECTOR v0, MATRIX m0,VECTOR v1)
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

/*void RotMatrixZ(MATRIX m0, MATRIX m1, float rz)
{
    __asm__ __volatile__(
"	mtc1	$0,$f0\n"
"	c.olt.s %2,$f0\n"
"	li.s    $f0,1.57079637050628662109e0	\n"
"	bc1f    _RotMatrixZ_01\n"
"	add.s   %2,$f0,%2			#rx=rx+π/2\n"
"	li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
"	j	_RotMatrixZ_02\n"
"_RotMatrixZ_01:\n"
"	sub.s   %2,$f0,%2			#rx=π/2-rx\n"
"	move	$7,$0\n"
"_RotMatrixZ_02:\n"

"        mfc1    $8,%2\n"
"        qmtc2    $8,$vf6\n"
"	move	$6,$31	# ra 保存 (本当はスタックを使うべき)\n"

"	jal	_ecossin	# sin(roll), cos(roll)\n"
"	move	$31,$6	# ra 回復\n"
"			#vf05:0,0,0,0 |x,y,z,w\n"
"	vmove.xyzw $vf06,$vf05\n"
"	vmove.xyzw $vf07,$vf05\n"
"	vmove.xyzw $vf09,$vf00\n"
"	vsub.xyz $vf09,$vf09,$vf09 	#0,0,0,1 |x,y,z,w\n"
"	vmr32.xyzw $vf08,$vf09	#0,0,1,0 |x,y,z,w\n"

"	vsub.zw $vf04,$vf04,$vf04 #vf04.zw=0 s,c,0,0 |x,y,z,w\n"
"	vaddx.y $vf06,$vf05,$vf04 #vf06 0,s,0,0 |x,y,z,w\n"
"	vaddy.x $vf06,$vf05,$vf04 #vf06 c,s,0,0 |x,y,z,w\n"
"	vsubx.x $vf07,$vf05,$vf04 #vf07 -s,0,0,0 |x,y,z,w\n"
"	vaddy.y $vf07,$vf05,$vf04 #vf07 -s,c,0,0 |x,y,z,w\n"

"	li    $7,4\n"
"	_loopRotMatrixZ:\n"
"	lqc2    $vf4,0x0(%1)\n"
"	vmulax.xyzw	$ACC,   $vf6,$vf4\n"
"	vmadday.xyzw	$ACC,   $vf7,$vf4\n"
"	vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
"	vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
"	sqc2    $vf5,0x0(%0)\n"
"	addi    $7,-1\n"
"	addi    %1,0x10\n"
"	addi    %0,0x10\n"
"	bne    $0,$7,_loopRotMatrixZ\n"
	: : "r" (m0) , "r" (m1), "f" (rz):"$6","$7","$8","$f0", "memory");
}

void RotMatrixX(MATRIX m0, MATRIX m1, float rx)
{
    __asm__ __volatile__(
	"mtc1	$0,$f0\n"
	"c.olt.s %2,$f0\n"
	"li.s    $f0,1.57079637050628662109e0	\n"
	"bc1f    _RotMatrixX_01\n"
	"add.s   %2,$f0,%2			#rx=rx+π/2\n"
	"li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
	"j	_RotMatrixX_02\n"
"_RotMatrixX_01:\n"
"	sub.s   %2,$f0,%2			#rx=π/2-rx\n"
"	move	$7,$0\n"
"_RotMatrixX_02:\n"

	"mfc1    $8,%2\n"
	"qmtc2    $8,$vf6\n"
	"move	$6,$31	# ra 保存 (本当はスタックを使うべき)\n"
	"jal	_ecossin	# sin(roll), cos(roll)\n"
	"move	$31,$6	# ra 回復\n"
	"		#vf05:0,0,0,0 |x,y,z,w\n"
	"vmove.xyzw $vf06,$vf05\n"
	"vmove.xyzw $vf07,$vf05\n"
	"vmove.xyzw $vf08,$vf05\n"
	"vmove.xyzw $vf09,$vf05\n"
	"vaddw.x $vf06,$vf05,$vf00 #vf06 1,0,0,0 |x,y,z,w\n"
	"vaddw.w $vf09,$vf05,$vf00 #vf09 0,0,0,1 |x,y,z,w\n"

	"vsub.zw $vf04,$vf04,$vf04 #vf04.zw=0 s,c,0,0 |x,y,z,w\n"
	"vaddx.z $vf07,$vf05,$vf04 #vf07.zw=0 0,0,s,0 |x,y,z,w\n"
	"vaddy.y $vf07,$vf05,$vf04 #vf07.zw=0 0,c,s,0 |x,y,z,w\n"
	"vsubx.y $vf08,$vf05,$vf04 #vf08.zw=0 0,-s,0,0 |x,y,z,w\n"
	"vaddy.z $vf08,$vf05,$vf04 #vf08.zw=0 0,-s,c,0 |x,y,z,w\n"

	"li    $7,4\n"
	"_loopRotMatrixX:\n"
	"lqc2    $vf4,0x0(%1)\n"
	"vmulax.xyzw	$ACC,   $vf6,$vf4\n"
	"vmadday.xyzw	$ACC,   $vf7,$vf4\n"
	"vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
	"vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
	"sqc2    $vf5,0x0(%0)\n"
	"addi    $7,-1\n"
	"addi    %1,0x10\n"
	"addi    %0,0x10\n"
	"bne    $0,$7,_loopRotMatrixX\n"
	: : "r" (m0) , "r" (m1), "f" (rx):"$6","$7","$8","$f0","memory");
}
	
void RotMatrixY(MATRIX m0, MATRIX m1, float ry)
{
    __asm__ __volatile__(
	"mtc1	$0,$f0\n"
	"c.olt.s %2,$f0\n"
	"li.s    $f0,1.57079637050628662109e0	\n"
	"bc1f    _RotMatrixY_01\n"
	"add.s   %2,$f0,%2			#rx=rx+π/2\n"
	"li 	$7,1				#cos(rx)=sin(rx+π/2)\n"
	"j	_RotMatrixY_02\n"
"_RotMatrixY_01:\n"
	"sub.s   %2,$f0,%2			#rx=π/2-rx\n"
	"move	$7,$0\n"
"_RotMatrixY_02:\n"

	"mfc1    $8,%2\n"
	"qmtc2    $8,$vf6\n"
	"move	$6,$31	# ra 保存 (本当はスタックを使うべき)\n"
	"jal	_ecossin	# sin(roll), cos(roll)\n"
	"move	$31,$6	# ra 回復\n"
	"		#vf05:0,0,0,0 |x,y,z,w\n"
	"vmove.xyzw $vf06,$vf05\n"
	"vmove.xyzw $vf07,$vf05\n"
	"vmove.xyzw $vf08,$vf05\n"
	"vmove.xyzw $vf09,$vf05\n"
	"vaddw.y $vf07,$vf05,$vf00 #vf07 0,1,0,0 |x,y,z,w\n"
	"vaddw.w $vf09,$vf05,$vf00 #vf09 0,0,0,1 |x,y,z,w\n"

	"vsub.zw $vf04,$vf04,$vf04 #vf04.zw=0 s,c,0,0 |x,y,z,w\n"
	"vsubx.z $vf06,$vf05,$vf04 #vf06 0,0,-s,0 |x,y,z,w\n"
	"vaddy.x $vf06,$vf05,$vf04 #vf06 c,0,-s,0 |x,y,z,w\n"
	"vaddx.x $vf08,$vf05,$vf04 #vf08 s,0,0,0 |x,y,z,w\n"
	"vaddy.z $vf08,$vf05,$vf04 #vf08 s,0,c,0 |x,y,z,w\n"

	"li    $7,4\n"
	"_loopRotMatrixY:\n"
	"lqc2    $vf4,0x0(%1)\n"
	"vmulax.xyzw	$ACC,   $vf6,$vf4\n"
	"vmadday.xyzw	$ACC,   $vf7,$vf4\n"
	"vmaddaz.xyzw	$ACC,   $vf8,$vf4\n"
	"vmaddw.xyzw	$vf5,  $vf9,$vf4\n"
	"sqc2    $vf5,0x0(%0)\n"
	"addi    $7,-1\n"
	"addi    %1,0x10\n"
	"addi    %0,0x10\n"
	"bne    $0,$7,_loopRotMatrixY\n"
	: : "r" (m0) , "r" (m1), "f" (ry):"$6","$7","$8","$f0","memory");
}*/

void SubVector(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__(
	"lqc2    $vf4,0x0(%1)\n"
	"lqc2    $vf5,0x0(%2)\n"
	"vsub.xyzw	$vf6,$vf4,$vf5\n"
	"sqc2    $vf6,0x0(%0)\n"
	: : "r" (v0) , "r" (v1), "r" (v2) : "memory");
}

void AddVector(VECTOR v0, VECTOR v1, VECTOR v2)
{
    __asm__ __volatile__(
	"lqc2    $vf4,0x0(%1)\n"
	"lqc2    $vf5,0x0(%2)\n"
	"vadd.xyzw	$vf6,$vf4,$vf5\n"
	"sqc2    $vf6,0x0(%0)\n"
	: : "r" (v0) , "r" (v1), "r" (v2) : "memory");
}

float LenVector(VECTOR v) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void SetLenVector(VECTOR v, float newLength) {
    Normalize(v, v);
    v[0] *= newLength;
    v[1] *= newLength;
    v[2] *= newLength;
}

void ScaleVector(VECTOR res, VECTOR v, float size) {
    res[0] = v[0] * size;
    res[1] = v[1] * size;
    res[2] = v[2] * size;
	res[3] = 0.0f;
}

void CameraMatrix(MATRIX m, VECTOR position, VECTOR target, VECTOR up)
{
	MATRIX	m0;
	VECTOR	xd;

	UnitMatrix(m0);	

	OuterProduct(xd, up, target);

	VECTOR left;
	Normalize(left, xd);

	VECTOR forward;
	Normalize(forward, target);

	VECTOR nup;
	OuterProduct(nup, forward, left);

	m0[0] = left[0];
	m0[1] = left[1];
	m0[2] = left[2];
	m0[3] = left[3];

	m0[4] = nup[0];
	m0[5] = nup[1];
	m0[6] = nup[2];
	m0[7] = nup[3];

	m0[8] = forward[0];
	m0[9] = forward[1];
	m0[10] = forward[2];
	m0[11] = forward[3];

	m0[12] = position[0];
	m0[13] = position[1];
	m0[14] = position[2];
	m0[15] = 1.0f;

	//TransMatrix(m0, m0, p);
	InversMatrix(m, m0);
}

void RotCameraMatrix(MATRIX m, VECTOR p, VECTOR zd, VECTOR yd, VECTOR rot)
{
    MATRIX work;
    VECTOR direction, vertical, position;
    UnitMatrix(work);
	matrix_rotate(work, work, rot);
    //RotMatrixX(work,work,rot[0]);
    //RotMatrixY(work,work,rot[1]);
    //RotMatrixZ(work,work,rot[2]);
    ApplyMatrix(direction, work, zd);
    ApplyMatrix(vertical, work, yd);
    ApplyMatrix(position, work, p);
    CameraMatrix(m, position, direction, vertical);
}

void LookAtCameraMatrix(MATRIX m, VECTOR position, VECTOR target, VECTOR up)
{
	MATRIX	m0;
	VECTOR	work;

	VECTOR left, forward, nup;

	UnitMatrix(m0);	

	SubVector(work, target, position);
	Normalize(forward, work);

	OuterProduct(work, up, forward);
	Normalize(left, work);

	OuterProduct(nup, forward, left);

	m0[0] = left[0];
	m0[1] = left[1];
	m0[2] = left[2];
	m0[3] = left[3];

	m0[4] = nup[0];
	m0[5] = nup[1];
	m0[6] = nup[2];
	m0[7] = nup[3];

	m0[8] =  -forward[0];
	m0[9] =  -forward[1];
	m0[10] = -forward[2];
	m0[11] = forward[3];

	m0[12] = position[0];
	m0[13] = position[1];
	m0[14] = position[2];
	m0[15] = 1.0f;

	//TransMatrix(m0, m0, p);
	InversMatrix(m, m0);
}



