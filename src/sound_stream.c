#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <sound.h>
#include <dbgprintf.h>

static int master_volume = 0;

static bool stream_playing = false;

static bool stream_repeat = false;

static char soundBuffer[AUDIO_STREAM_BUFFER_SIZE];

static SoundStream *cur_snd = NULL;

static void sound_wav_fillbuf_handler(void *arg) {
    int ret = -1;

    ret = fread(soundBuffer, 1, sizeof(soundBuffer), cur_snd->fp);

    if (ret > 0) {
        audsrv_play_audio(soundBuffer, ret);
    }

    if (ret < sizeof(soundBuffer))
	{
        fseek(cur_snd->fp, 0x30, SEEK_SET);

        if (!stream_repeat) {
            sound_pause();
		}
	}
}

static void sound_ogg_fillbuf_handler(void *arg) {
    int ret = -1;
    int bitStream = 0;
    int decodeTotal = AUDIO_STREAM_BUFFER_SIZE;
    int bufferPtr = 0;

    do {
        ret = ov_read(cur_snd->fp, soundBuffer + bufferPtr, decodeTotal, 0, 2, 1, &bitStream);
        
        if (ret > 0) {
            bufferPtr += ret;
            decodeTotal -= ret;
        } else if (ret < 0) {
            dbgprintf("ogg: I/O error while reading.\n");
            return;
        } else if (ret == 0) {
            ov_pcm_seek(cur_snd->fp, 0);
            
            if (!stream_repeat) {
                sound_pause();
                return;
            }
        }
    } while (decodeTotal > 0);

    audsrv_play_audio(soundBuffer, AUDIO_STREAM_BUFFER_SIZE);
}

SoundStream * load_wav(const char* path) {
    SoundStream *wav = malloc(sizeof(SoundStream));
    t_wave header;

    int ret;
	int err;
	int bytes;

    wav->fp = fopen(path, "rb");
    fread(&header, 1, sizeof(t_wave), wav->fp);
    fseek(wav->fp, 0x30, SEEK_SET);

    wav->fmt.bits = header.w_nbitspersample;
	wav->fmt.freq = header.w_samplespersec;
	wav->fmt.channels = header.w_nchannels;
    wav->type = WAV_AUDIO;

    return wav;
}

void play_wav(SoundStream * wav) {
    if(!stream_playing) {

        stream_playing = true;
        cur_snd = wav;

        audsrv_set_format(&(cur_snd->fmt));

        audsrv_on_fillbuf(AUDIO_STREAM_BUFFER_SIZE, sound_wav_fillbuf_handler, NULL);

        sound_wav_fillbuf_handler(NULL); // Kick the first chunk
    } 
}

int is_sound_playing(void) {
    return (int)stream_playing;
}

void set_sound_repeat(bool repeat) {
	stream_repeat = repeat;
}

void sound_pause() {
	if(stream_playing) {
		stream_playing = false;

        audsrv_wait_audio(AUDIO_STREAM_BUFFER_SIZE);
        audsrv_stop_audio();
	}
}

void sound_resume(SoundStream* snd) {
    if (cur_snd->type == WAV_AUDIO) {
        play_wav(snd);
    } else {
        play_ogg(snd);
    }
}

void sound_free(SoundStream* snd) {
    if (snd == cur_snd) {
        sound_pause();
    }

    if (snd->type == OGG_AUDIO) {
        ov_clear(snd->fp);
        free(snd->fp);
    } else if (snd->type == WAV_AUDIO) {
        fclose(snd->fp);
    } 
    
    snd->fp = NULL;
    free(snd);
    snd = NULL;
}

void sound_setvolume(int volume) {
	audsrv_set_volume(volume);
    master_volume = volume;
}

void sound_restart() {
    switch (cur_snd->type) {
        case OGG_AUDIO:
            ov_pcm_seek(cur_snd->fp, 0);
            play_ogg(cur_snd);
            return;
        case WAV_AUDIO:
            fseek(cur_snd->fp, 0x30, SEEK_SET);
            play_wav(cur_snd);
            return;
    }
}

int sound_get_duration(SoundStream* snd) {
    uint32_t f_pos, f_sz;

    if (snd->type == OGG_AUDIO) {
        return (int)(ov_time_total(snd->fp, -1)*1000);
    } else if (snd->type == WAV_AUDIO) {
        t_wave tmp_header;
        if (snd == cur_snd)
            sound_pause();
        f_pos = ftell(snd->fp);

        fseek(snd->fp, 0, SEEK_SET);
        fread(&tmp_header, 1, sizeof(t_wave), snd->fp);

        fseek(snd->fp, 0, SEEK_END);
        f_sz = ftell(snd->fp);

        fseek(snd->fp, f_pos, SEEK_SET);
        if (snd == cur_snd)
            sound_resume(snd);

        return (f_sz/tmp_header.w_navgbytespersec)*1000;
    } 

    return -1;
}

void sound_set_position(SoundStream* snd, int ms) {
    uint32_t f_pos, n_samples;

    if (snd->type == OGG_AUDIO) {
        if (ms < sound_get_duration(snd)) {
            if (snd == cur_snd)
                sound_pause();

            n_samples = ms / 1000 * snd->fmt.freq;

            f_pos = (ms / 1000 * snd->fmt.freq) * (snd->fmt.bits / 16);

            ov_pcm_seek(cur_snd->fp, round(f_pos / AUDIO_STREAM_BUFFER_SIZE) * AUDIO_STREAM_BUFFER_SIZE);

            if (snd == cur_snd)
                sound_resume(snd);
        }

    } else if (snd->type == WAV_AUDIO) {
        if (ms < sound_get_duration(snd)) {
            if (snd == cur_snd)
                sound_pause();

            n_samples = ms / 1000 * snd->fmt.freq;

            f_pos = (ms / 1000 * snd->fmt.freq) * (snd->fmt.bits / 4);

            fseek(snd->fp, f_pos, SEEK_SET);

            if (snd == cur_snd)
                sound_resume(snd);
        }

    } 
}

int sound_get_position(SoundStream* snd) {
    uint32_t f_pos, ms;

    if (snd->type == OGG_AUDIO) {
        f_pos = ov_pcm_tell(snd->fp);
	    
        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 16)));

        return ms;
    } else if (snd->type == WAV_AUDIO) {
        f_pos = ftell(snd->fp);

        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 4)));

        return ms;
    } 

    return -1;
}

// OGG Support
SoundStream* load_ogg(const char* path) {
    FILE *oggFile;
    SoundStream* ogg;

    ogg = malloc(sizeof(SoundStream));
    ogg->fp = calloc(1, sizeof(OggVorbis_File));

    oggFile = fopen(path, "rb");
    if (oggFile == NULL) {
        dbgprintf("ogg: Failed to open Ogg file %s\n", path);
        return -ENOENT;
    }

    if (ov_open_callbacks(oggFile, ogg->fp, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
        dbgprintf("ogg: Input does not appear to be an Ogg bitstream.\n");
        return -ENOENT;
    }

	vorbis_info *vi = ov_info(ogg->fp, -1);
    ov_pcm_seek(ogg->fp, 0);

    ogg->fmt.channels = vi->channels;
    ogg->fmt.freq = vi->rate;
    ogg->fmt.bits = 16;
    ogg->type = OGG_AUDIO;

    return ogg;
}

void play_ogg(SoundStream* ogg) {
    if(!stream_playing) {
        stream_playing = true;
        cur_snd = ogg;

        audsrv_set_format(&cur_snd->fmt);

        audsrv_on_fillbuf(AUDIO_STREAM_BUFFER_SIZE, sound_ogg_fillbuf_handler, NULL);

        sound_ogg_fillbuf_handler(NULL); // Kick the first chunk
    } 
}
