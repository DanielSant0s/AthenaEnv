#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include "include/sound.h"

static bool adpcm_started = false;
static bool audsrv_started = false;

/*static int fillbuffer(void *arg)
{
	iSignalSema((int)arg);
	return 0;
}*/

/*int main(int argc, char **argv)
{
	int ret;
	int played;
	int err;
	int bytes;
	char chunk[2048];
	FILE *wav;
	ee_sema_t sema;
	int fillbuffer_sema;

	sema.init_count = 0;
	sema.max_count = 1;
	sema.option = 0;
	fillbuffer_sema = CreateSema(&sema);

	audsrv_on_fillbuf(sizeof(chunk), fillbuffer, (void *)fillbuffer_sema);

	wav = fopen("host:song_22k.wav", "rb");

	fseek(wav, 0x30, SEEK_SET);

	printf("starting play loop\n");
	played = 0;
	bytes = 0;
	while (1)
	{
		ret = fread(chunk, 1, sizeof(chunk), wav);
		if (ret > 0)
		{
			WaitSema(fillbuffer_sema);
			audsrv_play_audio(chunk, ret);
		}

		if (ret < sizeof(chunk))
		{
			break;
		}

		played++;
		bytes = bytes + ret;

		if (played % 8 == 0)
		{
			printf("\r%d bytes sent..", bytes);
		}

		if (played == 512) break;
	}

	fclose(wav);

}*/


void sound_setvolume(int volume) {
    if(!audsrv_started) {
        audsrv_init();
        audsrv_started = true;
    }

	audsrv_set_volume(volume);
}

void sound_setformat(int bits, int freq, int channels){
    if(!audsrv_started) {
        audsrv_init();
        audsrv_started = true;
    }

	struct audsrv_fmt_t format;

    format.bits = bits;
	format.freq = freq;
	format.channels = channels;
	
	audsrv_set_format(&format);
}

void sound_setadpcmvolume(int slot, int volume) {
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

	audsrv_adpcm_set_volume(slot, volume);
}

audsrv_adpcm_t* sound_loadadpcm(const char* path){
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

	FILE* adpcm;
	audsrv_adpcm_t *sample = (audsrv_adpcm_t *)malloc(sizeof(audsrv_adpcm_t));
	int size;
	u8* buffer;

	adpcm = fopen(path, "rb");

	fseek(adpcm, 0, SEEK_END);
	size = ftell(adpcm);
	fseek(adpcm, 0, SEEK_SET);

	buffer = (u8*)malloc(size);

	fread(buffer, 1, size, adpcm);
	fclose(adpcm);

	audsrv_load_adpcm(sample, buffer, size);

	free(buffer);

	return sample;
}

void sound_playadpcm(int slot, audsrv_adpcm_t *sample) {
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

	audsrv_ch_play_adpcm(slot, sample);
}