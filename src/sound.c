#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "include/sound.h"
#include "include/dbgprintf.h"

extern void *_gp;

static int master_volume;

static unsigned char terminate_flag, stream_playing;

static bool flag_start = true;
static bool ogg_started = false;
static bool wav_started = false;
static bool adpcm_started = false;

static bool stream_repeat = false;
static bool stream_paused = false;
static bool stream_restart = false;

static int oggThreadID, oggIoThreadID, wavThreadId;
static int outSema, inSema;
static unsigned char rdPtr, wrPtr;
static char soundBuffer[STREAM_RING_BUFFER_COUNT][STREAM_RING_BUFFER_SIZE];
static volatile unsigned char oggThreadRunning, oggIoThreadRunning, wav_thread_running;

static Sound *cur_snd;

static void wavThread(void *arg)
{
    int ret;
    wav_thread_running = 1;
    while (!terminate_flag) {
        //SleepThread();

		ret = fread(soundBuffer[0], 1, sizeof(soundBuffer[0]), cur_snd->fp);

		if (ret > 0)
		{
			audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
			audsrv_play_audio(soundBuffer[0], ret);
		}

        if (ret < sizeof(soundBuffer[0]))
		{
            fseek(cur_snd->fp, 0x30, SEEK_SET);

            if (!stream_repeat) {
                audsrv_stop_audio();
                sound_pause();
			}
		}

		if(stream_paused) {
			audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
			audsrv_stop_audio();
            SleepThread();
			flag_start = false;
		} else if (!flag_start) {
			flag_start = true;
		}

        if(stream_restart) {
            audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
            audsrv_stop_audio();

            fseek(cur_snd->fp, 0x30, SEEK_SET);

            stream_restart = false;
        }
    }

    audsrv_stop_audio();

    stream_playing = 0;
	stream_paused = false;
    wav_thread_running = 0;
}

static int init_wav() {
    int result;
    ee_thread_t thread;

    thread.func = &wavThread;
    thread.stack = memalign(128, STREAM_THREAD_STACK_SIZE);
    thread.stack_size = STREAM_THREAD_STACK_SIZE;
    thread.gp_reg = &_gp;
    thread.initial_priority = STREAM_THREAD_BASE_PRIO;
    thread.attr = 0;
    thread.option = 0;

    // sound thread will start in DORMANT state.
    wavThreadId = CreateThread(&thread);

    if (wavThreadId >= 0) {
        result = 0;
    } else {
        DeleteThread(wavThreadId);
        result = oggIoThreadID;
    }

    return result;
}

Sound * load_wav(const char* path) {
    Sound *wav = malloc(sizeof(Sound));
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

    if(!wav_started) {
	    int ret = init_wav();
        if (ret >= 0) {
            wav_started = true;
        }
	}

    return wav;
}

void play_wav(Sound * wav) {
    if(!stream_playing) {
        audsrv_set_format(&wav->fmt);

        stream_playing = 1;
        cur_snd = wav;

        stream_paused = false;
        if(wav_thread_running) {
            WakeupThread(wavThreadId);
        } else {
            StartThread(wavThreadId, NULL);
        }
    } 
}

void sound_free(Sound* snd) {
    if (snd == cur_snd) {
        terminate_flag = 1;
    }

    if (snd->type == OGG_AUDIO) {
        ov_clear(snd->fp);
        free(snd->fp);
    } else if (snd->type == WAV_AUDIO) {
        fclose(snd->fp);
    } else if (snd->type == ADPCM_AUDIO) {
        audsrv_adpcm_t* sample = snd->fp;
        free(sample->buffer);
	    sample->buffer = NULL;
	    free(sample);
    }
    snd->fp = NULL;
    free(snd);
    snd = NULL;
}

int is_sound_playing(void)
{
    int ret = (int)stream_playing;

    return ret;
}

void set_sound_repeat(bool repeat) {
	stream_repeat = repeat;
}

void sound_pause() {
	if(!stream_paused) {
		stream_paused = true;
		stream_playing = 0;
	}
}

void sound_resume(Sound* snd) {
	if(stream_paused) {
        cur_snd = snd;
        audsrv_set_format(&cur_snd->fmt);
        
        if (cur_snd->type == WAV_AUDIO)
            WakeupThread(wavThreadId);

		stream_paused = false;
		stream_playing = 1;
	}
}

void sound_setvolume(int volume) {
	audsrv_set_volume(volume);
    master_volume = volume;
}

void sound_setadpcmvolume(int slot, int volume) {
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

    audsrv_adpcm_set_volume_and_pan(slot, volume, 0);
}

void sound_restart()
{
    stream_restart = true;
}

audsrv_adpcm_t* sound_loadadpcm(const char* path){
    if(!adpcm_started) {
        audsrv_adpcm_init();
        adpcm_started = true;
    }

	FILE* adpcm;
	audsrv_adpcm_t *sample = malloc(sizeof(audsrv_adpcm_t));
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

	return sample;
}

void sound_playadpcm(int slot, audsrv_adpcm_t *sample) {
	audsrv_ch_play_adpcm(slot, sample);
}

static int get_sample_qt_duration(int nSamples)
{
    float sampleRate = 44100; // 44.1kHz

    // Return duration in milliseconds
    return (nSamples / sampleRate) * 1000;
}

static int sound_get_adpcm_duration(audsrv_adpcm_t *sample)
{
    // Calculate duration based on number of samples
    int duration_ms = get_sample_qt_duration(((u32 *)sample->buffer)[3]);
    // Estimate duration based on filesize, if the ADPCM header was 0
    if (duration_ms == 0)
        duration_ms = sample->size / 47;

    return duration_ms;
}

int sound_get_duration(Sound* snd) {
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
    } else if (snd->type == ADPCM_AUDIO) {
        return sound_get_adpcm_duration(snd->fp);
    }

    return -1;
}

void sound_set_position(Sound* snd, int ms) {
    uint32_t f_pos, n_samples;

    if (snd->type == OGG_AUDIO) {
        if (ms < sound_get_duration(snd)) {
            if (snd == cur_snd)
                sound_pause();

            n_samples = ms / 1000 * snd->fmt.freq;

            f_pos = (ms / 1000 * snd->fmt.freq) * (snd->fmt.bits / 16);

            ov_pcm_seek(cur_snd->fp, round(f_pos / STREAM_RING_BUFFER_SIZE) * STREAM_RING_BUFFER_SIZE);

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

    } else if (snd->type == ADPCM_AUDIO) {
        return -1;
    }
}

int sound_get_position(Sound* snd) {
    uint32_t f_pos, ms;

    if (snd->type == OGG_AUDIO) {
        f_pos = ov_pcm_tell(snd->fp);
	    
        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 16)));

        return ms;
    } else if (snd->type == WAV_AUDIO) {
        f_pos = ftell(snd->fp);

        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 4)));

        return ms;
    } else if (snd->type == ADPCM_AUDIO) {
        return -1;
    }
}

// OGG Support

static void oggThread(void *arg)
{
    oggThreadRunning = 1;

    while (!terminate_flag) {
        SleepThread();

        while (PollSema(outSema) == outSema) {
            audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
            audsrv_play_audio(soundBuffer[rdPtr], STREAM_RING_BUFFER_SIZE);
            rdPtr = (rdPtr + 1) % STREAM_RING_BUFFER_COUNT;

            SignalSema(inSema);
        }

		if(stream_paused) {
			audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
			audsrv_stop_audio();
			flag_start = false;
		} else if (!flag_start) {
			flag_start = true;
		}

        if(stream_restart) {
            audsrv_wait_audio(STREAM_RING_BUFFER_SIZE);
            audsrv_stop_audio();

            fseek(cur_snd->fp, 0x30, SEEK_SET);

            stream_restart = false;
        }
    }

    audsrv_stop_audio();

    rdPtr = 0;
    wrPtr = 0;

    oggThreadRunning = 0;
    stream_playing = 0;
	stream_paused = false;
}

static void oggIoThread(void *arg)
{
    int partsToRead, decodeTotal, bitStream, i;

    oggIoThreadRunning = 1;
    do {
		if(!stream_paused && cur_snd->type == OGG_AUDIO) {
			WaitSema(inSema);
        	partsToRead = 1;

        	while ((wrPtr + partsToRead < STREAM_RING_BUFFER_COUNT) && (PollSema(inSema) == inSema))
        	    partsToRead++;

        	decodeTotal = STREAM_RING_BUFFER_SIZE;
        	int bufferPtr = 0;
        	do {
        	    int ret = ov_read(cur_snd->fp, soundBuffer[wrPtr] + bufferPtr, decodeTotal, 0, 2, 1, &bitStream);
        	    if (ret > 0) {
        	        bufferPtr += ret;
        	        decodeTotal -= ret;
        	    } else if (ret < 0) {
        	        dbgprintf("ogg: I/O error while reading.\n");
        	        terminate_flag = 1;
        	        break;
        	    } else if (ret == 0) {
                    ov_pcm_seek(cur_snd->fp, 0);

					if (!stream_repeat) {
						audsrv_stop_audio();
                        sound_pause();
					}
				}

        	} while (decodeTotal > 0);

        	wrPtr = (wrPtr + partsToRead) % STREAM_RING_BUFFER_COUNT;
        	for (i = 0; i < partsToRead; i++)
        	    SignalSema(outSema);
        	WakeupThread(oggThreadID);
		}
    } while (!terminate_flag);

    oggIoThreadRunning = 0;
    terminate_flag = 1;
    WakeupThread(oggThreadID);
}

static int oggInit(void)
{
    ee_thread_t thread;
    ee_sema_t sema;
    int result;

    terminate_flag = 0;
    rdPtr = 0;
    wrPtr = 0;
    oggThreadRunning = 0;
    oggIoThreadRunning = 0;

    sema.max_count = STREAM_RING_BUFFER_COUNT;
    sema.init_count = STREAM_RING_BUFFER_COUNT;
    sema.attr = 0;
    sema.option = (u32) "ogg-in-sema";
    inSema = CreateSema(&sema);

    if (inSema >= 0) {
        sema.max_count = STREAM_RING_BUFFER_COUNT;
        sema.init_count = 0;
        sema.attr = 0;
        sema.option = (u32) "ogg-out-sema";
        outSema = CreateSema(&sema);

        if (outSema < 0) {
            DeleteSema(inSema);
            return outSema;
        }
    } else
        return inSema;

    thread.func = &oggThread;
    thread.stack = memalign(128, STREAM_THREAD_STACK_SIZE);
    thread.stack_size = STREAM_THREAD_STACK_SIZE;
    thread.gp_reg = &_gp;
    thread.initial_priority = STREAM_THREAD_BASE_PRIO;
    thread.attr = 0;
    thread.option = 0;

    // ogg thread will start in DORMANT state.
    oggThreadID = CreateThread(&thread);

    if (oggThreadID >= 0) {
        thread.func = &oggIoThread;
        thread.stack = memalign(128, STREAM_THREAD_STACK_SIZE);
        thread.stack_size = STREAM_THREAD_STACK_SIZE;
        thread.gp_reg = &_gp;
        thread.initial_priority = STREAM_THREAD_BASE_PRIO + 1;
        thread.attr = 0;
        thread.option = 0;

        // ogg I/O thread will start in DORMANT state.
        oggIoThreadID = CreateThread(&thread);
        if (oggIoThreadID >= 0) {
            result = 0;
        } else {
            DeleteSema(inSema);
            DeleteSema(outSema);
            DeleteThread(oggThreadID);
            result = oggIoThreadID;
        }
    } else {
        result = oggThreadID;
        DeleteSema(inSema);
        DeleteSema(outSema);
    }

    return result;
}

static void oggDeinit(void)
{
    DeleteSema(inSema);
    DeleteSema(outSema);
    DeleteThread(oggThreadID);
    DeleteThread(oggIoThreadID);

    // Vorbisfile takes care of fclose.
    ov_clear(cur_snd->fp);
}

Sound* load_ogg(const char* path)
{
    FILE *oggFile;
    Sound* ogg;

	if(!ogg_started) {
		int ret = oggInit();
    	if (ret >= 0) {
    	    ogg_started = true;
    	}
	}

    ogg = malloc(sizeof(Sound));
    ogg->fp = calloc(1, sizeof(OggVorbis_File));

    oggFile = fopen(path, "rb");
    if (oggFile == NULL) {
        dbgprintf("ogg: Failed to open Ogg file %s\n", path);
		oggDeinit();
        return -ENOENT;
    }

    if (ov_open_callbacks(oggFile, ogg->fp, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
        dbgprintf("ogg: Input does not appear to be an Ogg bitstream.\n");
		oggDeinit();
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

void play_ogg(Sound* ogg) {
    if(!stream_playing) {
        audsrv_set_format(&ogg->fmt);

        stream_playing = 1;
        cur_snd = ogg;

        if(stream_paused) {
            stream_paused = false;
        } else if (!oggThreadRunning && !oggIoThreadRunning) {
            StartThread(oggIoThreadID, NULL);
            StartThread(oggThreadID, NULL);
        }
    } 
}

static void oggShutdownDelayCallback(s32 alarm_id, u16 time, void *common)
{
    iWakeupThread((int)common);
}

void sound_deinit(void)
{
    int threadId;
    threadId = GetThreadId();

    terminate_flag = 1;

    if(ogg_started) {
        WakeupThread(oggThreadID);

        while (oggIoThreadRunning) {
            SetAlarm(200 * 16, &oggShutdownDelayCallback, (void *)threadId);
            SleepThread();
        }

        while (oggThreadRunning) {
            SetAlarm(200 * 16, &oggShutdownDelayCallback, (void *)threadId);
            SleepThread();
        }

        oggDeinit();
        if (cur_snd->type == OGG_AUDIO)
            free(cur_snd->fp);
        ogg_started = false;
    }

    if(wav_started) {
        if(stream_paused && cur_snd->type == WAV_AUDIO)
            WakeupThread(wavThreadId);

        while (wav_thread_running) {
            SetAlarm(200 * 16, &oggShutdownDelayCallback, (void *)threadId);
            SleepThread();
        }

        DeleteThread(wavThreadId);
        if (cur_snd->type == WAV_AUDIO)
            fclose(cur_snd->fp);
        wav_started = false;
    }

    cur_snd->fp = NULL;
    free(cur_snd);
    cur_snd = NULL;

}
