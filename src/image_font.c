
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <time.h>

#include <graphics.h>
#include <athena_math.h>
#include <dbgprintf.h>
#include <fntsys.h>
#include <texture_manager.h>
#include <render.h>

#include <owl_packet.h>

int athena_font_upload(GSCONTEXT *gsGlobal, GSFONT *gsFont)
{
	int i;

	if( gsFont->Type == FONT_TYPE_FNT )
	{
		for (i=0; i<256; i++) {
            gsFont->Additional[i] = gsFont->CharWidth;
        }
		return 0;
	}
	else if( (gsFont->Type == FONT_TYPE_PNG_DAT) || 
	         (gsFont->Type == FONT_TYPE_BMP_DAT) || 
	         (gsFont->Type == FONT_TYPE_JPEG_DAT)) 
	{
		if( load_image(gsFont->Texture, gsFont->Path, true) == -1) {
			dbgprintf("Error uploading font texture: %s\n", gsFont->Path);
			return -1;
		}

		gsFont->HChars=16;
		gsFont->VChars=16;
		gsFont->CharWidth = gsFont->Texture->Width / 16;
		gsFont->CharHeight = gsFont->Texture->Height / 16;

		// Try to load .dat file with character widths
		if(gsFont->Path_DAT) {
			FILE* File = fopen(gsFont->Path_DAT, "rb");
			if (File)
			{
				fseek(File, 0, SEEK_SET);
				for(i=0; i<256; i++) {
					if(fread(&gsFont->Additional[i], 1, 2, File) != 2)
					{
						dbgprintf("Problem reading font sizes from %s\n", gsFont->Path_DAT);
						fclose(File);
						// Fallback to default width
						for (i=0; i<256; i++) {
							gsFont->Additional[i] = (short)gsFont->CharWidth;
						}
						return 0;
					}
				}
				fclose(File);
				return 0;
			}
		}

		for (i=0; i<256; i++) {
			gsFont->Additional[i] = (short)gsFont->CharWidth;
		}
		return 0;
	}

	dbgprintf("Error uploading font: Unknown type %d\n", gsFont->Type);
	return -1;
}

GSFONT *athena_init_font(u8 type, char *path)
{
	char *tmp = NULL;
	size_t path_len = strlen(path) + 1;

	GSFONT *gsFont = calloc(1, sizeof(GSFONT));
	if (!gsFont) return NULL;
	
	gsFont->Texture = calloc(1, sizeof(GSSURFACE));
	gsFont->Path = calloc(1, path_len);
	gsFont->Additional = calloc(1, sizeof(short)*256);

	if (!gsFont->Texture || !gsFont->Path || !gsFont->Additional) {
		if (gsFont->Texture) free(gsFont->Texture);
		if (gsFont->Path) free(gsFont->Path);
		if (gsFont->Additional) free(gsFont->Additional);
		free(gsFont);
		return NULL;
	}

	gsFont->Type = type;
	strcpy(gsFont->Path, path);

	if(gsFont->Type == FONT_TYPE_BMP_DAT || 
	   gsFont->Type == FONT_TYPE_PNG_DAT || 
	   gsFont->Type == FONT_TYPE_JPEG_DAT)
	{
		gsFont->Path_DAT = calloc(1, path_len);
		if (!gsFont->Path_DAT) {
			free(gsFont->Additional);
			free(gsFont->Path);
			free(gsFont->Texture);
			free(gsFont);
			return NULL;
		}
		strcpy(gsFont->Path_DAT, path);

		if(gsFont->Type == FONT_TYPE_BMP_DAT) {
			tmp = strstr(gsFont->Path_DAT, ".bmp");
		} else if(gsFont->Type == FONT_TYPE_PNG_DAT) {
			tmp = strstr(gsFont->Path_DAT, ".png");
		} else if(gsFont->Type == FONT_TYPE_JPEG_DAT) {
			tmp = strstr(gsFont->Path_DAT, ".jpg");
			if (!tmp) tmp = strstr(gsFont->Path_DAT, ".jpeg");
		}

		if (tmp == NULL)
		{
			free(gsFont->Additional);
			free(gsFont->Path);
			free(gsFont->Path_DAT);
			free(gsFont->Texture);
			free(gsFont);
			dbgprintf("Error initializing .dat path for font\n");
			return NULL;
		}
		else
		{
			strcpy(tmp, ".dat");
		}
	}
	else
	{
		gsFont->Path_DAT = NULL;
	}

	return gsFont;
}

static void render_font_char_glyph(GSFONT *gsFont, unsigned char c, owl_packet *packet, int pen_x, int pen_y, float scale)
{
	int px, py, charsiz;
	float x1, y1, x2, y2;
	float u1, v1, u2, v2;

	px = c % 16;
	py = (c - px) / 16;

	charsiz = gsFont->Additional[(u8)c];

	x1 = (float)pen_x - 0.5f;
	
	if (GetInterlacedFrameMode()) {
		y1 = ((float)pen_y / 2.0f) - 0.5f;
		y2 = (y1 + ((float)gsFont->CharHeight / 2.0f) * scale) - 0.5f;
	} else {
		y1 = (float)pen_y - 0.5f;
		y2 = y1 + ((float)gsFont->CharHeight * scale) - 0.5f;
	}
	
	x2 = x1 + ((float)charsiz * scale) - 0.5f;

	u1 = (float)(px * gsFont->CharWidth) + (0.5f * scale);
	v1 = (float)(py * gsFont->CharHeight) + (0.5f * scale);
	u2 = u1 + (float)charsiz;
	v2 = v1 + (float)gsFont->CharHeight;

	owl_add_tag(packet, 0, GS_SETREG_STQ( owl_uv_transform(u1, 1024), owl_uv_transform(v1, 1024) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)(owl_coord_transform(x1, gsGlobal->OffsetX)) | ((uint64_t)(owl_coord_transform(y1, gsGlobal->OffsetY)) << 32));

	owl_add_tag(packet, 0, GS_SETREG_STQ( owl_uv_transform(u2, 1024), owl_uv_transform(v2, 1024) << 4 ));

	owl_add_tag(packet, 1, (uint64_t)(owl_coord_transform(x2, gsGlobal->OffsetX)) | ((uint64_t)(owl_coord_transform(y2, gsGlobal->OffsetY)) << 32));
}

void athena_font_print_scaled(GSCONTEXT *gsGlobal, GSFONT *gsFont, float X, float Y, int Z,
                      float scale, unsigned long color, const char *String)
{

	if(gsFont->Type == FONT_TYPE_PNG_DAT || 
	   gsFont->Type == FONT_TYPE_BMP_DAT || 
	   gsFont->Type == FONT_TYPE_JPEG_DAT ||
	   gsFont->Type == FONT_TYPE_FNT)
	{
		int pen_x = (int)X;
		int pen_y = (int)Y;
		const char *text_to_render = String;
		
		owl_packet *packet = NULL;
		owl_qword *last_cnt, *last_direct, *last_prim, *before_first_draw, *after_draw;
		int texture_id = -1;
		bool started_rendering = false;

		int printable_chars = 0;
		for (const char *p = String; *p; p++) {
			if (*p != '\n') printable_chars++;
		}

		texture_id = texture_manager_bind(gsGlobal, gsFont->Texture, true);
		
		int text_vert_size = printable_chars * 2;
		packet = owl_query_packet(CHANNEL_VIF1, (texture_id != -1 ? 12 : 8) + text_vert_size);

		last_cnt = packet->ptr;
		owl_add_cnt_tag(packet, (texture_id != -1 ? 11 : 7) + text_vert_size, 0);

		if (texture_id != -1) {
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSH, 0));
			owl_add_uint(packet, VIF_CODE(2, 0, VIF_DIRECT, 0));

			owl_add_tag(packet, GIF_AD, GIFTAG(1, 1, 0, 0, 0, 1));
			owl_add_tag(packet, GIF_NOP, 0);

			owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
			owl_add_uint(packet, VIF_CODE(texture_id, 0, VIF_MARK, 0));
			owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 1));
		}

		last_direct = packet->ptr;
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_NOP, 0));
		owl_add_uint(packet, VIF_CODE(0, 0, VIF_FLUSHA, 0));
		owl_add_uint(packet, VIF_CODE(6 + text_vert_size, 0, VIF_DIRECT, 0));

		owl_add_tag(packet, GIF_AD, GIFTAG(4, 1, 0, 0, 0, 1));

		int tw, th;
		athena_set_tw_th(gsFont->Texture, &tw, &th);

		owl_add_tag(packet,
			GS_TEX0_1 + gsGlobal->PrimContext,
			GS_SETREG_TEX0((gsFont->Texture->Vram & ~TRANSFER_REQUEST_MASK) / 256,
				gsFont->Texture->TBW,
				gsFont->Texture->PSM,
				tw, th,
				gsGlobal->PrimAlphaEnable,
				COLOR_MODULATE,
				(gsFont->Texture->VramClut & ~TRANSFER_REQUEST_MASK) / 256,
				gsFont->Texture->ClutPSM,
				0, 0,
				gsFont->Texture->VramClut ? GS_CLUT_STOREMODE_LOAD : GS_CLUT_STOREMODE_NOLOAD)
		);

		owl_add_tag(packet, GS_TEX1_1 + gsGlobal->PrimContext, 
			GS_SETREG_TEX1(1, 0, gsFont->Texture->Filter, gsFont->Texture->Filter, 0, 0, 0));

		owl_add_tag(packet, GS_PRIM,
			VU_GS_PRIM(GS_PRIM_PRIM_SPRITE,
				0,
				1,
				gsGlobal->PrimFogEnable,
				gsGlobal->PrimAlphaEnable,
				gsGlobal->PrimAAEnable,
				1,
				gsGlobal->PrimContext,
				0)
		);

		owl_add_tag(packet, GS_RGBAQ, color);

		last_prim = packet->ptr;
		owl_add_tag(packet,
			((uint64_t)(GS_UV) << 0 | (uint64_t)(GS_XYZ2) << 4),
			VU_GS_GIFTAG(text_vert_size,
				1, NO_CUSTOM_DATA, 0,
				0,
				1, 2)
		);

		before_first_draw = packet->ptr;

		for (; *text_to_render; ++text_to_render) {
			unsigned char c = (unsigned char)*text_to_render;
			
			if (c == '\n') {
				pen_x = (int)X;
				pen_y += (int)((gsFont->CharHeight * scale) + 1);
				continue;
			}

			render_font_char_glyph(gsFont, c, packet, pen_x, pen_y, scale);

			pen_x += (int)((gsFont->Additional[(u8)c] * scale) + 1);
		}
	}
}

GSFONT* loadFont(const char* path)
{
	int file = open(path, O_RDONLY, 0777);
	if (file < 0) {
		dbgprintf("Cannot open font file: %s\n", path);
		return NULL;
	}

	uint16_t magic;
	if (read(file, &magic, 2) != 2) {
		close(file);
		dbgprintf("Cannot read font file header: %s\n", path);
		return NULL;
	}
	close(file);

	GSFONT* font = NULL;

	if (magic == 0x4D42) {
		font = athena_init_font(FONT_TYPE_BMP_DAT, (char*)path);
		if (font && athena_font_upload(gsGlobal, font) != 0) {
			unloadFont(font);
			font = NULL;
		}
	}

	else if (magic == 0x4246) {
		font = athena_init_font(FONT_TYPE_FNT, (char*)path);
		if (font && athena_font_upload(gsGlobal, font) != 0) {
			unloadFont(font);
			font = NULL;
		}
	}

	else if (magic == 0x5089) {
		font = athena_init_font(FONT_TYPE_PNG_DAT, (char*)path);
		if (font && athena_font_upload(gsGlobal, font) != 0) {
			unloadFont(font);
			font = NULL;
		}
	}

	else if (magic == 0xFFD8) {
		font = athena_init_font(FONT_TYPE_JPEG_DAT, (char*)path);
		if (font && athena_font_upload(gsGlobal, font) != 0) {
			unloadFont(font);
			font = NULL;
		}
	}
	else {
		const char *ext = strrchr(path, '.');
		if (ext) {
			if (strcasecmp(ext, ".bmp") == 0) {
				font = athena_init_font(FONT_TYPE_BMP_DAT, (char*)path);
				if (font && athena_font_upload(gsGlobal, font) != 0) {
					unloadFont(font);
					font = NULL;
				}
			}
			else if (strcasecmp(ext, ".png") == 0) {
				font = athena_init_font(FONT_TYPE_PNG_DAT, (char*)path);
				if (font && athena_font_upload(gsGlobal, font) != 0) {
					unloadFont(font);
					font = NULL;
				}
			}
			else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
				font = athena_init_font(FONT_TYPE_JPEG_DAT, (char*)path);
				if (font && athena_font_upload(gsGlobal, font) != 0) {
					unloadFont(font);
					font = NULL;
				}
			}
		}
		
		if (!font) {
			dbgprintf("Unknown font format: %s (magic: 0x%04X)\n", path, magic);
		}
	}

	return font;
}

Coords athena_font_calc_dimensions(GSFONT *gsFont, float scale, const char *str)
{
	Coords size = {0, 0};
	
	if (!gsFont || !str) {
		return size;
	}

	int width = 0;
	int max_width = 0;
	int lines = 1;

	for (const char *p = str; *p; ++p) {
		unsigned char c = (unsigned char)*p;
		
		if (c == '\n') {
			if (width > max_width) {
				max_width = width;
			}
			width = 0;
			lines++;
			continue;
		}

		width += (int)((gsFont->Additional[(u8)c] * scale) + 1);
	}

	if (width > max_width) {
		max_width = width;
	}

	size.width = max_width;
	size.height = (int)(gsFont->CharHeight * scale * lines);

	return size;
}

void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color)
{
	if (!font || !text) {
		return;
	}
	
	athena_font_print_scaled(gsGlobal, font, x - 0.5f, y - 0.5f, 1, scale, color, text);
}

void printFontTextPlus(GSFONT* font, const char* text, float x, float y, float scale, Color color,
                       short aligned, size_t width, size_t height,
                       float outline, Color outline_color,
                       float dropshadow, Color dropshadow_color)
{
	if (!font || !text) {
		return;
	}

	float draw_x = x;
	float draw_y = y;

	Coords text_size = athena_font_calc_dimensions(font, scale, text);

	if (aligned & ALIGN_HCENTER) {
		draw_x -= text_size.width >> 1;
	} else if (aligned & ALIGN_RIGHT) {
		draw_x -= text_size.width;
	}

	if (aligned & ALIGN_VCENTER) {
		draw_y += (height - text_size.height) >> 1;
	} else if (aligned & ALIGN_BOTTOM) {
		draw_y += height - text_size.height;
	}

	if (outline > 0.0f) {
		float offsets[][2] = { {outline, outline}, {outline, -outline}, {-outline, outline}, {-outline, -outline} };
		for (int i = 0; i < 4; i++) {
			athena_font_print_scaled(gsGlobal, font, 
				draw_x + offsets[i][0] - 0.5f, 
				draw_y + offsets[i][1] - 0.5f, 
				1, scale, outline_color, text);
		}
	} 

	else if (dropshadow > 0.0f) {
		athena_font_print_scaled(gsGlobal, font, 
			draw_x + dropshadow - 0.5f, 
			draw_y + dropshadow - 0.5f, 
			1, scale, dropshadow_color, text);
	}

	athena_font_print_scaled(gsGlobal, font, draw_x - 0.5f, draw_y - 0.5f, 1, scale, color, text);
}

void unloadFont(GSFONT* font)
{
	if (!font) {
		return;
	}

	if (font->Texture) {
		texture_manager_free(font->Texture);

		if (font->Texture->Mem) {
			free(font->Texture->Mem);
			font->Texture->Mem = NULL;
		}

		if (font->Texture->Clut) {
			free(font->Texture->Clut);
			font->Texture->Clut = NULL;
		}
		
		free(font->Texture);
		font->Texture = NULL;
	}

	if (font->RawData) {
		free(font->RawData);
		font->RawData = NULL;
	}

	if (font->Path) {
		free(font->Path);
		font->Path = NULL;
	}

	if (font->Path_DAT) {
		free(font->Path_DAT);
		font->Path_DAT = NULL;
	}

	if (font->Additional) {
		free(font->Additional);
		font->Additional = NULL;
	}

	free(font);
}
