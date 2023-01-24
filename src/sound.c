#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "include/sound.h"

static bool adpcm_started = false;

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
	audsrv_set_volume(volume);
}

void sound_setformat(int bits, int freq, int channels){
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


// OGG Support

#define ogg_RING_BUFFER_COUNT 16
#define ogg_RING_BUFFER_SIZE  4096
#define ogg_THREAD_BASE_PRIO  0x40
#define ogg_THREAD_STACK_SIZE 0x1000

extern void *_gp;

static bool flag_start = true;
static bool ogg_started = false;
static bool ogg_repeat = false;
static bool ogg_paused = false;

static int oggThreadID, oggIoThreadID;
static int outSema, inSema;
static unsigned char terminateFlag, oggIsPlaying;
static unsigned char rdPtr, wrPtr;
static char oggBuffer[ogg_RING_BUFFER_COUNT][ogg_RING_BUFFER_SIZE];
static volatile unsigned char oggThreadRunning, oggIoThreadRunning;

static u8 oggThreadStack[ogg_THREAD_STACK_SIZE] __attribute__((aligned(16)));
static u8 oggIoThreadStack[ogg_THREAD_STACK_SIZE] __attribute__((aligned(16)));

static OggVorbis_File *vorbisFile;

static void oggThread(void *arg)
{
    oggThreadRunning = 1;

    while (!terminateFlag) {
        SleepThread();

        while (PollSema(outSema) == outSema) {
            audsrv_wait_audio(ogg_RING_BUFFER_SIZE);
            audsrv_play_audio(oggBuffer[rdPtr], ogg_RING_BUFFER_SIZE);
            rdPtr = (rdPtr + 1) % ogg_RING_BUFFER_COUNT;

            SignalSema(inSema);
        }

		if(ogg_paused) {
			audsrv_wait_audio(ogg_RING_BUFFER_SIZE);
			audsrv_stop_audio();
			flag_start = false;
		} else if (!flag_start) {
			flag_start = true;
		}
    }

    audsrv_stop_audio();

    rdPtr = 0;
    wrPtr = 0;

    oggThreadRunning = 0;
    oggIsPlaying = 0;
	ogg_paused = false;
}

static void oggIoThread(void *arg)
{
    int partsToRead, decodeTotal, bitStream, i;

    oggIoThreadRunning = 1;
    do {
		if(!ogg_paused) {
			WaitSema(inSema);
        	partsToRead = 1;

        	while ((wrPtr + partsToRead < ogg_RING_BUFFER_COUNT) && (PollSema(inSema) == inSema))
        	    partsToRead++;

        	decodeTotal = ogg_RING_BUFFER_SIZE;
        	int bufferPtr = 0;
        	do {
        	    int ret = ov_read(vorbisFile, oggBuffer[wrPtr] + bufferPtr, decodeTotal, 0, 2, 1, &bitStream);
        	    if (ret > 0) {
        	        bufferPtr += ret;
        	        decodeTotal -= ret;
        	    } else if (ret < 0) {
        	        printf("ogg: I/O error while reading.\n");
        	        terminateFlag = 1;
        	        break;
        	    } else if (ret == 0) {
					if (!ogg_repeat) {
						terminateFlag = 1;
						break;
					}
        	        ov_pcm_seek(vorbisFile, 0);
				}

        	} while (decodeTotal > 0);

        	wrPtr = (wrPtr + partsToRead) % ogg_RING_BUFFER_COUNT;
        	for (i = 0; i < partsToRead; i++)
        	    SignalSema(outSema);
        	WakeupThread(oggThreadID);
		}
    } while (!terminateFlag);

    oggIoThreadRunning = 0;
    terminateFlag = 1;
    WakeupThread(oggThreadID);
}

static int oggInit(void)
{
    ee_thread_t thread;
    ee_sema_t sema;
    int result;

    terminateFlag = 0;
    rdPtr = 0;
    wrPtr = 0;
    oggThreadRunning = 0;
    oggIoThreadRunning = 0;

    sema.max_count = ogg_RING_BUFFER_COUNT;
    sema.init_count = ogg_RING_BUFFER_COUNT;
    sema.attr = 0;
    sema.option = (u32) "ogg-in-sema";
    inSema = CreateSema(&sema);

    if (inSema >= 0) {
        sema.max_count = ogg_RING_BUFFER_COUNT;
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
    thread.stack = oggThreadStack;
    thread.stack_size = sizeof(oggThreadStack);
    thread.gp_reg = &_gp;
    thread.initial_priority = ogg_THREAD_BASE_PRIO;
    thread.attr = 0;
    thread.option = 0;

    // ogg thread will start in DORMANT state.
    oggThreadID = CreateThread(&thread);

    if (oggThreadID >= 0) {
        thread.func = &oggIoThread;
        thread.stack = oggIoThreadStack;
        thread.stack_size = sizeof(oggIoThreadStack);
        thread.gp_reg = &_gp;
        thread.initial_priority = ogg_THREAD_BASE_PRIO + 1;
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
    ov_clear(vorbisFile);
    free(vorbisFile);
    vorbisFile = NULL;
	ogg_started = false;
}

int ogg_load_play(const char* path)
{
    FILE *oggFile;
	struct audsrv_fmt_t audsrvFmt;

	if(!ogg_started) {
		int ret = oggInit();
    	if (ret >= 0) {
    	    ogg_started = true;
    	}
	}

    vorbisFile = calloc(1, sizeof(OggVorbis_File));

    oggFile = fopen(path, "rb");
    if (oggFile == NULL) {
        printf("ogg: Failed to open Ogg file %s\n", path);
		oggDeinit();
        return -ENOENT;
    }

    if (ov_open_callbacks(oggFile, vorbisFile, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
        printf("ogg: Input does not appear to be an Ogg bitstream.\n");
		oggDeinit();
        return -ENOENT;
    }

	vorbis_info *vi = ov_info(vorbisFile, -1);
    ov_pcm_seek(vorbisFile, 0);

    audsrvFmt.channels = vi->channels;
    audsrvFmt.freq = vi->rate;
    audsrvFmt.bits = 16;

    audsrv_set_format(&audsrvFmt);

    oggIsPlaying = 1;

    StartThread(oggIoThreadID, NULL);
    StartThread(oggThreadID, NULL);

    return 0;
}

static void oggShutdownDelayCallback(s32 alarm_id, u16 time, void *common)
{
    iWakeupThread((int)common);
}

void ogg_stop(void)
{
    int threadId;

    terminateFlag = 1;
    WakeupThread(oggThreadID);

    threadId = GetThreadId();
    while (oggIoThreadRunning) {
        SetAlarm(200 * 16, &oggShutdownDelayCallback, (void *)threadId);
        SleepThread();
    }
    while (oggThreadRunning) {
        SetAlarm(200 * 16, &oggShutdownDelayCallback, (void *)threadId);
        SleepThread();
    }

    oggDeinit();
}

int is_ogg_playing(void)
{
    int ret = (int)oggIsPlaying;

    return ret;
}

void set_ogg_repeat(bool repeat) {
	ogg_repeat = repeat;
}

void ogg_pause() {
	if(!ogg_paused) {
		ogg_paused = true;
		oggIsPlaying = 0;
	}
}

void ogg_resume() {
	if(ogg_paused) {
		ogg_paused = false;
		oggIsPlaying = 1;
	}
}