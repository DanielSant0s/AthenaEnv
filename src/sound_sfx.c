#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <sound.h>

static bool adpcm_started = false;

#define sfx_channels_size 24

Sfx *sfx_channels[sfx_channels_size] = { NULL };

uint8_t sfx_channel_volumes[sfx_channels_size] = { [0 ... (sfx_channels_size-1)] = 100 };
uint8_t sfx_channel_pans[sfx_channels_size] = { 0 };

Sfx* sound_sfx_load(const char* path) {
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

	FILE* adpcm;
	Sfx *snd = malloc(sizeof(Sfx));
	int size;
	u8* buffer;

	adpcm = fopen(path, "rb");

	fseek(adpcm, 0, SEEK_END);
	size = ftell(adpcm);
	fseek(adpcm, 0, SEEK_SET);

	buffer = (u8*)malloc(size);

	fread(buffer, 1, size, adpcm);
	fclose(adpcm);

	audsrv_load_adpcm(&snd->sound, buffer, size);

    snd->volume = 100;
    snd->pan = 0;

	return snd;
}
void sound_sfx_free(Sfx* snd) {
    free(snd->sound.buffer);
    snd->sound.buffer = NULL;
    free(snd);
}

int sound_sfx_play(int channel, Sfx *snd) {
    if (channel < 0) {
        channel = sound_sfx_find_channel();
    }

    if ((sfx_channel_volumes[channel] != snd->volume) || (sfx_channel_pans[channel] != snd->pan)) 
        sound_sfx_channel_volume(channel, snd->volume, snd->pan);

    audsrv_ch_play_adpcm(channel, &snd->sound);

    sfx_channels[channel] = snd;

    return channel;
}

void sound_sfx_channel_volume(int channel, int volume, int pan) {
    audsrv_adpcm_set_volume_and_pan(channel, volume, pan);

    sfx_channel_volumes[channel] = volume;
    sfx_channel_pans[channel] = pan;
}

static int get_sample_qt_duration(int nSamples) {
    float sampleRate = 44100;

    return (nSamples / sampleRate) * 1000;
}

static int sound_get_adpcm_duration(audsrv_adpcm_t *sample) {
    int duration_ms = get_sample_qt_duration(((u32 *)sample->buffer)[3]);

    if (duration_ms == 0)
        duration_ms = sample->size / 47;

    return duration_ms;
}

int sound_sfx_length(Sfx *snd) {
    return sound_get_adpcm_duration(&snd->sound);
}

bool sound_sfx_is_playing(Sfx *snd, int channel) {
    return (bool)audsrv_is_adpcm_playing(channel, &snd->sound);
}

int sound_sfx_find_channel() {
    for (int i = 0; i < sfx_channels_size; i++) {
        if (!sfx_channels[i])
            return i;

        if (!audsrv_is_adpcm_playing(i, &sfx_channels[i]->sound)) {
            sfx_channels[i] = NULL;
            return i;
        }
    }

    return -1;
}