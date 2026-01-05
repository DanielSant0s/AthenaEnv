#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <macros.h>

#include <sound.h>

typedef struct {
	char magic[4];	//"APCM"
	unsigned char version;
	unsigned char channels;
	unsigned char loop;
	unsigned char reserved;
	unsigned int pitch;
	unsigned int samples;
} adpcm_header_t;

#define adpcm_header(snd) ((adpcm_header_t *)(snd->sound.buffer))

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

    snd->sample_rate = (snd->sound.pitch * 48000) / 4096;
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

static int sound_get_adpcm_duration(Sfx *snd) {
    return (adpcm_header(snd)->samples / snd->sample_rate) * 1000;
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

inline float map_pitch_range(int value) {
    return (float)((clamp(value, -100, 100) + 100) / 100.0f);
}

inline int unmap_pitch_range(float value) {
    return clamp((int)((value * 100.0f) - 100), -100, 100);
}

int sound_sfx_get_pitch(Sfx *snd) {
    float mapped_pitch = ((float)snd->sound.pitch * 48000) / (snd->sample_rate * 4096);

    return unmap_pitch_range(mapped_pitch);
}

void sound_sfx_set_pitch(Sfx *snd, int pitch) {
    snd->sound.pitch = (snd->sample_rate * (4096 * map_pitch_range(pitch))) / 48000;
}
