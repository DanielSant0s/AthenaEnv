#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>
#include <dbgprintf.h>

#include <jpeglib.h>
#include <png.h>

#include <graphics.h>


struct gsBitMapFileHeader
{
	u16	Type;
	u32	Size;
	u16 Reserved1;
	u16 Reserved2;
	u32 Offset;
} __attribute__ ((packed));
typedef struct gsBitMapFileHeader GSBMFHDR;

struct gsBitMapInfoHeader
{
	u32	Size;
	u32	Width;
	u32	Height;
	u16	Planes;
	u16 BitCount;
	u32 Compression;
	u32 SizeImage;
	u32 XPelsPerMeter;
	u32 YPelsPerMeter;
	u32 ColorUsed;
	u32 ColorImportant;
} __attribute__ ((packed));
typedef struct gsBitMapInfoHeader GSBMIHDR;

struct gsBitMapClut
{
	u8 Blue;
	u8 Green;
	u8 Red;
	u8 Alpha;
} __attribute__ ((packed));
typedef struct gsBitMapClut GSBMCLUT;

struct gsBitmap
{
	GSBMFHDR FileHeader;
	GSBMIHDR InfoHeader;
	char *Texture;
	GSBMCLUT *Clut;
};
typedef struct gsBitmap GSBITMAP;


//2D drawing functions
int athena_load_png(GSTEXTURE* tex, FILE* File, bool delayed)
{
	tex->Delayed = true;

	if (File == NULL)
	{
		dbgprintf("Failed to load PNG file\n");
		return -1;
	}

	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	png_bytep *row_pointers;

	u32 sig_read = 0;
        int row, i, k=0, j, bit_depth, color_type, interlace_type;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) NULL, NULL, NULL);

	if(!png_ptr)
	{
		dbgprintf("PNG Read Struct Init Failed\n");
		fclose(File);
		return -1;
	}

	info_ptr = png_create_info_struct(png_ptr);

	if(!info_ptr)
	{
		dbgprintf("PNG Info Struct Init Failed\n");
		fclose(File);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return -1;
	}

	if(setjmp(png_jmpbuf(png_ptr)))
	{
		dbgprintf("Got PNG Error!\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(File);
		return -1;
	}

	png_init_io(png_ptr, File);

	png_set_sig_bytes(png_ptr, sig_read);

	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,&interlace_type, NULL, NULL);

	if (bit_depth == 16) 
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA || bit_depth < 4) 
		png_set_expand(png_ptr);

	png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

	png_read_update_info(png_ptr, info_ptr);

	tex->Width = width;
	tex->Height = height;

    tex->VramClut = 0;
    tex->Clut = NULL;
	tex->ClutStorageMode = GS_CLUT_STORAGE_CSM1;

	color_type = png_get_color_type(png_ptr, info_ptr);

	if(color_type == PNG_COLOR_TYPE_RGB_ALPHA)
	{
		int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
		tex->PSM = GS_PSM_CT32;
		tex->Mem = (u32*)memalign(128, gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM));

		row_pointers = (png_byte**)calloc(height, sizeof(png_bytep));

		for (row = 0; row < height; row++) row_pointers[row] = (png_bytep)malloc(row_bytes);

		png_read_image(png_ptr, row_pointers);

		struct pixel { u8 r,g,b,a; };
		struct pixel *Pixels = (struct pixel *) tex->Mem;

		for (i = 0; i < tex->Height; i++) {
			for (j = 0; j < tex->Width; j++) {
				memcpy(&Pixels[k], &row_pointers[i][4 * j], 3);
				Pixels[k++].a = row_pointers[i][4 * j + 3] >> 1;
			}
		}

		for(row = 0; row < height; row++) free(row_pointers[row]);

		free(row_pointers);
	}
	else if(color_type == PNG_COLOR_TYPE_RGB)
	{
		int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
		tex->PSM = GS_PSM_CT24;
		tex->Mem = (u32*)memalign(128, gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM));

		row_pointers = (png_byte**)calloc(height, sizeof(png_bytep));

		for(row = 0; row < height; row++) row_pointers[row] = (png_bytep)malloc(row_bytes);

		png_read_image(png_ptr, row_pointers);

		struct pixel3 { u8 r,g,b; };
		struct pixel3 *Pixels = (struct pixel3 *) tex->Mem;

		for (i = 0; i < tex->Height; i++) {
			for (j = 0; j < tex->Width; j++) {
				memcpy(&Pixels[k++], &row_pointers[i][4 * j], 3);
			}
		}

		for(row = 0; row < height; row++) free(row_pointers[row]);

		free(row_pointers);
	}
	else if(color_type == PNG_COLOR_TYPE_PALETTE){

		struct png_clut { u8 r, g, b, a; };

		png_colorp palette = NULL;
		int num_pallete = 0;
		png_bytep trans = NULL;
		int num_trans = 0;

        png_get_PLTE(png_ptr, info_ptr, &palette, &num_pallete);
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
        tex->ClutPSM = GS_PSM_CT32;

		if (bit_depth == 4) {

			int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
			tex->PSM = GS_PSM_T4;
			tex->Mem = (u32*)memalign(128, gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM));

			row_pointers = (png_byte**)calloc(height, sizeof(png_bytep));

			for(row = 0; row < height; row++) row_pointers[row] = (png_bytep)malloc(row_bytes);

			png_read_image(png_ptr, row_pointers);

            tex->Clut = memalign(128, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
            memset(tex->Clut, 0, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));

            unsigned char *pixel = (unsigned char *)tex->Mem;
    		struct png_clut *clut = (struct png_clut *)tex->Clut;

    		int i, j, k = 0;

    		for (i = num_pallete; i < 16; i++) {
    		    memset(&clut[i], 0, sizeof(clut[i]));
    		}

    		for (i = 0; i < num_pallete; i++) {
    		    clut[i].r = palette[i].red;
    		    clut[i].g = palette[i].green;
    		    clut[i].b = palette[i].blue;
    		    clut[i].a = 0x80;
    		}

    		for (i = 0; i < num_trans; i++)
    		    clut[i].a = trans[i] >> 1;

    		for (i = 0; i < tex->Height; i++) {
    		    for (j = 0; j < tex->Width / 2; j++)
    		        memcpy(&pixel[k++], &row_pointers[i][1 * j], 1);
    		}

    		int byte;
    		unsigned char *tmpdst = (unsigned char *)tex->Mem;
    		unsigned char *tmpsrc = (unsigned char *)pixel;

    		for (byte = 0; byte < gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM); byte++) tmpdst[byte] = (tmpsrc[byte] << 4) | (tmpsrc[byte] >> 4);

			for(row = 0; row < height; row++) free(row_pointers[row]);

			free(row_pointers);

        } else if (bit_depth == 8) {
			int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
			tex->PSM = GS_PSM_T8;
			tex->Mem = (u32*)memalign(128, gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM));

			row_pointers = (png_byte**)calloc(height, sizeof(png_bytep));

			for(row = 0; row < height; row++) row_pointers[row] = (png_bytep)malloc(row_bytes);

			png_read_image(png_ptr, row_pointers);

            tex->Clut = memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
            memset(tex->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));

            unsigned char *pixel = (unsigned char *)tex->Mem;
    		struct png_clut *clut = (struct png_clut *)tex->Clut;

    		int i, j, k = 0;

    		for (i = num_pallete; i < 256; i++) {
    		    memset(&clut[i], 0, sizeof(clut[i]));
    		}

    		for (i = 0; i < num_pallete; i++) {
    		    clut[i].r = palette[i].red;
    		    clut[i].g = palette[i].green;
    		    clut[i].b = palette[i].blue;
    		    clut[i].a = 0x80;
    		}

    		for (i = 0; i < num_trans; i++)
    		    clut[i].a = trans[i] >> 1;

    		// rotate clut
    		for (i = 0; i < num_pallete; i++) {
    		    if ((i & 0x18) == 8) {
    		        struct png_clut tmp = clut[i];
    		        clut[i] = clut[i + 8];
    		        clut[i + 8] = tmp;
    		    }
    		}

    		for (i = 0; i < tex->Height; i++) {
    		    for (j = 0; j < tex->Width; j++) {
    		        memcpy(&pixel[k++], &row_pointers[i][1 * j], 1);
    		    }
    		}

			for(row = 0; row < height; row++) free(row_pointers[row]);

			free(row_pointers);
        }
	}
	else
	{
		dbgprintf("This texture depth is not supported yet!\n");
		return -1;
	}

	tex->Filter = GS_FILTER_NEAREST;
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
	fclose(File);

	gsKit_setup_tbw(tex);

	return 0;

}

int athena_load_bmp(GSTEXTURE* tex, FILE* File, bool delayed)
{
	GSBITMAP Bitmap;
	int x, y;
	int cy;
	u32 FTexSize;
	u8  *image;
	u8  *p;

	tex->Delayed = true;

	if (File == NULL)
	{
		dbgprintf("BMP: Failed to load bitmap\n");
		return -1;
	}
	if (fread(&Bitmap.FileHeader, sizeof(Bitmap.FileHeader), 1, File) <= 0)
	{
		dbgprintf("BMP: Could not load bitmap\n");
		fclose(File);
		return -1;
	}

	if (fread(&Bitmap.InfoHeader, sizeof(Bitmap.InfoHeader), 1, File) <= 0)
	{
		dbgprintf("BMP: Could not load bitmap\n");
		fclose(File);
		return -1;
	}

	tex->Width = Bitmap.InfoHeader.Width;
	tex->Height = Bitmap.InfoHeader.Height;
	tex->Filter = GS_FILTER_NEAREST; 

    tex->VramClut = 0;
    tex->Clut = NULL;
	tex->ClutStorageMode = GS_CLUT_STORAGE_CSM1;

	if(Bitmap.InfoHeader.BitCount == 4)
	{
		tex->PSM = GS_PSM_T4;
		tex->Clut = (u32*)memalign(128, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
		tex->ClutPSM = GS_PSM_CT32;

		memset(tex->Clut, 0, gsKit_texture_size_ee(8, 2, GS_PSM_CT32));
		fseek(File, 54, SEEK_SET);
		if (fread(tex->Clut, Bitmap.InfoHeader.ColorUsed*sizeof(u32), 1, File) <= 0)
		{
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			dbgprintf("BMP: Could not load bitmap\n");
			fclose(File);
			return -1;
		}

		GSBMCLUT *clut = (GSBMCLUT *)tex->Clut;
		int i;
		for (i = Bitmap.InfoHeader.ColorUsed; i < 16; i++)
		{
			memset(&clut[i], 0, sizeof(clut[i]));
		}

		for (i = 0; i < 16; i++)
		{
			u8 tmp = clut[i].Blue;
			clut[i].Blue = clut[i].Red;
			clut[i].Red = tmp;
			clut[i].Alpha = 0x80;
		}

	}
	else if(Bitmap.InfoHeader.BitCount == 8)
	{
		tex->PSM = GS_PSM_T8;
		tex->Clut = (u32*)memalign(128, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
		tex->ClutPSM = GS_PSM_CT32;

		memset(tex->Clut, 0, gsKit_texture_size_ee(16, 16, GS_PSM_CT32));
		fseek(File, 54, SEEK_SET);
		if (fread(tex->Clut, Bitmap.InfoHeader.ColorUsed*sizeof(u32), 1, File) <= 0)
		{
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			dbgprintf("BMP: Could not load bitmap\n");
			fclose(File);
			return -1;
		}

		GSBMCLUT *clut = (GSBMCLUT *)tex->Clut;
		int i;
		for (i = Bitmap.InfoHeader.ColorUsed; i < 256; i++)
		{
			memset(&clut[i], 0, sizeof(clut[i]));
		}

		for (i = 0; i < 256; i++)
		{
			u8 tmp = clut[i].Blue;
			clut[i].Blue = clut[i].Red;
			clut[i].Red = tmp;
			clut[i].Alpha = 0x80;
		}

		// rotate clut
		for (i = 0; i < 256; i++)
		{
			if ((i&0x18) == 8)
			{
				GSBMCLUT tmp = clut[i];
				clut[i] = clut[i+8];
				clut[i+8] = tmp;
			}
		}
	}
	else if(Bitmap.InfoHeader.BitCount == 16)
	{
		tex->PSM = GS_PSM_CT16;
		tex->VramClut = 0;
		tex->Clut = NULL;
	}
	else if(Bitmap.InfoHeader.BitCount == 24)
	{
		tex->PSM = GS_PSM_CT24;
		tex->VramClut = 0;
		tex->Clut = NULL;
	}

	fseek(File, 0, SEEK_END);
	FTexSize = ftell(File);
	FTexSize -= Bitmap.FileHeader.Offset;

	fseek(File, Bitmap.FileHeader.Offset, SEEK_SET);

	u32 TextureSize = gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM);

	tex->Mem = (u32*)memalign(128,TextureSize);

	if(Bitmap.InfoHeader.BitCount == 24)
	{
		image = (u8*)memalign(128, FTexSize);
		if (image == NULL) {
			dbgprintf("BMP: Failed to allocate memory\n");
			if (tex->Mem) {
				free(tex->Mem);
				tex->Mem = NULL;
			}
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			fclose(File);
			return -1;
		}

		fread(image, FTexSize, 1, File);
		p = (u8*)((u32)tex->Mem);
		for (y = tex->Height - 1, cy = 0; y >= 0; y--, cy++) {
			for (x = 0; x < tex->Width; x++) {
				p[(y * tex->Width + x) * 3 + 2] = image[(cy * tex->Width + x) * 3 + 0];
				p[(y * tex->Width + x) * 3 + 1] = image[(cy * tex->Width + x) * 3 + 1];
				p[(y * tex->Width + x) * 3 + 0] = image[(cy * tex->Width + x) * 3 + 2];
			}
		}
		free(image);
		image = NULL;
	}
	else if(Bitmap.InfoHeader.BitCount == 16)
	{
		image = (u8*)memalign(128, FTexSize);
		if (image == NULL) {
			dbgprintf("BMP: Failed to allocate memory\n");
			if (tex->Mem) {
				free(tex->Mem);
				tex->Mem = NULL;
			}
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			fclose(File);
			return -1;
		}

		fread(image, FTexSize, 1, File);

		p = (u8*)((u32*)tex->Mem);
		for (y = tex->Height - 1, cy = 0; y >= 0; y--, cy++) {
			for (x = 0; x < tex->Width; x++) {
				u16 value;
				value = *(u16*)&image[(cy * tex->Width + x) * 2];
				value = (value & 0x8000) | value << 10 | (value & 0x3E0) | (value & 0x7C00) >> 10;	//ARGB -> ABGR

				*(u16*)&p[(y * tex->Width + x) * 2] = value;
			}
		}
		free(image);
		image = NULL;
	}
	else if(Bitmap.InfoHeader.BitCount == 8 || Bitmap.InfoHeader.BitCount == 4)
	{
		char *text = (char *)((u32)tex->Mem);
		image = (u8*)memalign(128,FTexSize);
		if (image == NULL) {
			dbgprintf("BMP: Failed to allocate memory\n");
			if (tex->Mem) {
				free(tex->Mem);
				tex->Mem = NULL;
			}
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			fclose(File);
			return -1;
		}

		if (fread(image, FTexSize, 1, File) != 1)
		{
			if (tex->Mem) {
				free(tex->Mem);
				tex->Mem = NULL;
			}
			if (tex->Clut) {
				free(tex->Clut);
				tex->Clut = NULL;
			}
			dbgprintf("BMP: Read failed!, Size %d\n", FTexSize);
			free(image);
			image = NULL;
			fclose(File);
			return -1;
		}
		for (y = tex->Height - 1; y >= 0; y--)
		{
			if(Bitmap.InfoHeader.BitCount == 8)
				memcpy(&text[y * tex->Width], &image[(tex->Height - y - 1) * tex->Width], tex->Width);
			else
				memcpy(&text[y * (tex->Width / 2)], &image[(tex->Height - y - 1) * (tex->Width / 2)], tex->Width / 2);
		}
		free(image);
		image = NULL;

		if(Bitmap.InfoHeader.BitCount == 4)
		{
			int byte;
			u8 *tmpdst = (u8 *)((u32)tex->Mem);
			u8 *tmpsrc = (u8 *)text;

			for(byte = 0; byte < FTexSize; byte++)
			{
				tmpdst[byte] = (tmpsrc[byte] << 4) | (tmpsrc[byte] >> 4);
			}
		}
	}
	else
	{
		dbgprintf("BMP: Unknown bit depth format %d\n", Bitmap.InfoHeader.BitCount);
	}

	fclose(File);

	gsKit_setup_tbw(tex);

	return 0;

}

struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr)cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

// Following official documentation max width or height of the texture is 1024
#define MAX_TEXTURE 1024
static void  _ps2_load_JPEG_generic(GSTEXTURE *Texture, struct jpeg_decompress_struct *cinfo, struct my_error_mgr *jerr, bool scale_down)
{
	int textureSize = 0;
	if (scale_down) {
		unsigned int longer = cinfo->image_width > cinfo->image_height ? cinfo->image_width : cinfo->image_height;
		float downScale = (float)longer / (float)MAX_TEXTURE;
		cinfo->scale_denom = ceil(downScale);
	}

	jpeg_start_decompress(cinfo);

	int psm = cinfo->out_color_components == 3 ? GS_PSM_CT24 : GS_PSM_CT32;

	Texture->Width =  cinfo->output_width;
	Texture->Height = cinfo->output_height;
	Texture->PSM = psm;
	Texture->Filter = GS_FILTER_NEAREST;
	Texture->VramClut = 0;
	Texture->Clut = NULL;
	Texture->ClutStorageMode = GS_CLUT_STORAGE_CSM1;

	textureSize = cinfo->output_width*cinfo->output_height*cinfo->out_color_components;
	#ifdef DEBUG
	dbgprintf("Texture Size = %i\n",textureSize);
	#endif
	Texture->Mem = (u32*)memalign(128, textureSize);

	unsigned int row_stride = textureSize/Texture->Height;
	unsigned char *row_pointer = (unsigned char *)Texture->Mem;
	while (cinfo->output_scanline < cinfo->output_height) {
		jpeg_read_scanlines(cinfo, (JSAMPARRAY)&row_pointer, 1);
		row_pointer += row_stride;
	}

	jpeg_finish_decompress(cinfo);
}

int athena_load_jpeg(GSTEXTURE* tex, FILE* fp, bool scale_down, bool delayed)
{
	tex->Delayed = true;

	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;

	if (tex == NULL) {
		dbgprintf("jpeg: error Texture is NULL\n");
		return -1;
	}

	if (fp == NULL)
	{
		dbgprintf("jpeg: Failed to load file\n");
		return -1;
	}

	/* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		* We need to clean up the JPEG object, close the input file, and return.
		*/
		jpeg_destroy_decompress(&cinfo);
		fclose(fp);
		if (tex->Mem)
			free(tex->Mem);
		dbgprintf("jpeg: error during processing file\n");
		return -1;
	}
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	_ps2_load_JPEG_generic(tex, &cinfo, &jerr, scale_down);
	
	jpeg_destroy_decompress(&cinfo);
	fclose(fp);

	gsKit_setup_tbw(tex);

	return 0;

}

int load_image(GSTEXTURE* image, const char* path, bool delayed){
	FILE* file = fopen(path, "rb");
	uint16_t magic;
	fread(&magic, 1, 2, file);
	fseek(file, 0, SEEK_SET);
	if (magic == 0x4D42) athena_load_bmp(image, file, delayed);
	else if (magic == 0xD8FF) athena_load_jpeg(image, file, false, delayed);
	else if (magic == 0x5089) athena_load_png(image, file, delayed);

	return 0;
}
