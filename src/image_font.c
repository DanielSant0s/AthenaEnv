
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <fcntl.h>

#include <time.h>

#include <graphics.h>
#include <athena_math.h>
#include <dbgprintf.h>
#include <fntsys.h>

#include <owl_packet.h>

int athena_font_upload(GSGLOBAL *gsGlobal, GSFONT *gsFont)
{
	int i;

	if( gsFont->Type == FONT_TYPE_FNT )
	{
		//if( gsKit_texture_fnt(gsGlobal, gsFont) == -1 )
		//{
			printf("Error uploading font!\n");
			return -1;
		//}

		for (i=0; i<256; i++) {
            gsFont->Additional[i] = gsFont->CharWidth;
        }

		return 0;
	}
	else if( (gsFont->Type == FONT_TYPE_PNG_DAT) || (gsFont->Type == FONT_TYPE_BMP_DAT)) {
		if( load_image(gsFont->Texture, gsFont->Path, true) == -1) {
			dbgprintf("Error uploading font!\n");
			return -1;
		}

		gsFont->HChars=16;
		gsFont->VChars=16;
		gsFont->CharWidth = gsFont->Texture->Width / 16;
		gsFont->CharHeight = gsFont->Texture->Height / 16;

        if(gsFont->Path_DAT) {
            FILE* File = fopen(gsFont->Path_DAT, "rb");
            if (File)
            {
                fseek(File, 0, SEEK_SET);
                for(i=0; i<256; i++) {
                    if(fread(&gsFont->Additional[i], 1, 2, File) != 2)
                    {
                        printf("Problem reading font sizes %s\n", gsFont->Path_DAT);
                        fclose(File);
                        return -1;
                    }
                }
                fclose(File);
                return 0;
            }
            else
            {
                for (i=0; i<256; i++) {
                    gsFont->Additional[i] = (short)gsFont->CharWidth;
                    return 0;
                }
            }
        }
	}

	// if it reaches here, something's wrong
	printf("Error uploading font!\n");

	return -1;
}

GSFONT *athena_init_font(u8 type, char *path)
{
    char *tmp = NULL;

	GSFONT *gsFont = calloc(1,sizeof(GSFONT));
	gsFont->Texture = calloc(1,sizeof(GSTEXTURE));
	gsFont->Path = calloc(1,strlen(path));
	gsFont->Additional=calloc(1,sizeof(short)*256);

    gsFont->Type = type;
    strcpy(gsFont->Path, path);

    if(gsFont->Type == FONT_TYPE_BMP_DAT || gsFont->Type == FONT_TYPE_PNG_DAT)
	{
        gsFont->Path_DAT = calloc(1,strlen(path));
        strcpy(gsFont->Path_DAT, path);

        if(gsFont->Type == FONT_TYPE_BMP_DAT) tmp = strstr(gsFont->Path_DAT, ".bmp");
        if(gsFont->Type == FONT_TYPE_PNG_DAT) tmp = strstr(gsFont->Path_DAT, ".png");

        if (tmp == NULL)
        {
            free(gsFont->Additional);
            free(gsFont->Path);
            free(gsFont->Path_DAT);
            free(gsFont->Texture);
            free(gsFont);

            printf("Error initializing .dat\n");

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

void athena_font_print_scaled(GSGLOBAL *gsGlobal, GSFONT *gsFont, float X, float Y, int Z,
                      float scale, unsigned long color, const char *String)
{
	if( gsFont->Type == FONT_TYPE_PNG_DAT)
	{
		//u64 oldalpha = gsGlobal->PrimAlpha;
		//u8 oldpabe = gsGlobal->PABE;
		//u8 fixate = 0;
		//if(gsGlobal->Test->ATE)
		//{
		//	gsKit_set_test(gsGlobal, GS_ATEST_OFF);
		//	fixate = 1;
		//}
		//gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 1);


		int cx,cy,i,l;
		u8 c;
		cx=X;
		cy=Y;

		l=strlen( String );
		for( i=0;i<l;i++ )
		{
			c=String[i];
			if( c=='\n' )
			{
				cx=X;
				cy+=(gsFont->CharHeight*scale)+1;
			}
			else
			{
				int px,py,charsiz;

				px=c%16;
				py=(c-px)/16;
				charsiz=gsFont->Additional[(u8)c];

				//gsKit_prim_sprite_texture(gsGlobal, gsFont->Texture, cx, cy,
				//	(px*gsFont->CharWidth), (py*gsFont->CharHeight+1),
				//	cx+(charsiz*scale), cy+(gsFont->CharHeight*scale),
				//	(px*gsFont->CharWidth)+charsiz, (py*gsFont->CharHeight)+16+1,
				//	Z, color);
				cx+=(charsiz*scale)+1;
			}
		}
		//gsGlobal->PABE = oldpabe;
		//gsGlobal->PrimAlpha=oldalpha;
		//gsKit_set_primalpha(gsGlobal, gsGlobal->PrimAlpha, gsGlobal->PABE);
		//gsKit_set_clamp(gsGlobal, GS_CMODE_RESET); -- not athena comment

		//if(fixate)
		//	gsKit_set_test(gsGlobal, GS_ATEST_ON);

	}

	if( gsFont->Type == FONT_TYPE_BMP_DAT ||
		gsFont->Type == FONT_TYPE_FNT)
	{
		u64 oldalpha;
		u8 oldpabe;
		int cx,cy,i,l;

		//oldpabe = gsGlobal->PABE;
		//oldalpha = gsGlobal->PrimAlpha;

		//gsKit_set_primalpha(gsGlobal, ALPHA_BLEND_ADD, 1);

		cx=X;
		cy=Y;

		l=strlen( String );
		for( i=0;i<l;i++ )
		{
			unsigned char c=String[i];
			if( c=='\n' )
			{
				cx=X;
				cy+=(gsFont->CharHeight*scale)+1;
			}
			else
			{
				int px,py,charsiz;

				px=c%16;
				py=(c-px)/16;
				charsiz=gsFont->Additional[(u8)c];

				//gsKit_prim_sprite_texture(gsGlobal, gsFont->Texture, cx, cy,
				//	(px*gsFont->CharWidth), (py*gsFont->CharHeight+1),
				//	cx+(charsiz*scale), cy+(gsFont->CharHeight*scale),
				//	(px*gsFont->CharWidth)+charsiz, (py*gsFont->CharHeight)+16+1,
				//	Z, color);
				cx+=(charsiz*scale)+1;
			}
		}

		//gsKit_set_primalpha(gsGlobal, oldalpha, oldpabe);
	}
}



GSFONT* loadFont(const char* path){
	int file = open(path, O_RDONLY, 0777);
	uint16_t magic;
	read(file, &magic, 2);
	close(file);
	GSFONT* font = NULL;
	if (magic == 0x4D42) {
		font = athena_init_font(FONT_TYPE_BMP_DAT, (char*)path);
		athena_font_upload(gsGlobal, font);
	} else if (magic == 0x4246) {
		font = athena_init_font(FONT_TYPE_FNT, (char*)path);
		athena_font_upload(gsGlobal, font);
	} else if (magic == 0x5089) { 
		font = athena_init_font(FONT_TYPE_PNG_DAT, (char*)path);
		athena_font_upload(gsGlobal, font);
	}

	return font;
}

void printFontText(GSFONT* font, const char* text, float x, float y, float scale, Color color)
{ 
	//gsKit_set_test(gsGlobal, GS_ATEST_ON);
	athena_font_print_scaled(gsGlobal, font, x-0.5f, y-0.5f, 1, scale, color, text);
}

void unloadFont(GSFONT* font)
{
	texture_manager_free(font->Texture);
	// clut was pointing to static memory, so do not free
	font->Texture->Clut = NULL;
	// mem was pointing to 'TexBase', so do not free
	font->Texture->Mem = NULL;
	// free texture
	free(font->Texture);
	font->Texture = NULL;

	if (font->RawData)
		free(font->RawData);

	free(font);
	font = NULL;
}
