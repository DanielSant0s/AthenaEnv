#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>

#include <sound.h>
#include <dbgprintf.h>
#include <lockman.h>
#include <taskman.h>

/* Number of chunks kept ready ahead of playback. While the IOP plays back
 * slot N, the prefetch thread below is already decoding slot N+1 (and N+2)
 * in the background, so a slow/late disk read never has to happen inside
 * the audsrv fillbuf callback itself. */
#define STREAM_PREFETCH_SLOTS 3
#define STREAM_PREFETCH_STACK_SIZE (16 * 1024)
#define STREAM_PREFETCH_PRIORITY 0x60 /* matches audsrv's own RPC thread */

typedef struct {
    char data[AUDIO_STREAM_BUFFER_SIZE];
    int size;
    bool ended; /* true on the final chunk of a non-looping stream */
} StreamSlot;

static int master_volume = 0;

static bool stream_playing = false;

static SoundStream *cur_snd = NULL;

/* Guards cur_snd and every SoundStream field reachable through it (fp,
 * loop, ...). Held briefly by the game/JS thread (play/pause/free/seek/
 * position) and by the prefetch thread below -- never across a blocking
 * audsrv RPC call, to avoid deadlocking against it. */
static int stream_sema = -1;

static StreamSlot *prefetch_slots = NULL;
static int slot_free_sema = -1;
static int slot_filled_sema = -1;
static int prefetch_stopped_sema = -1;
static int prefetch_write_idx = 0;
static int prefetch_read_idx = 0;
static int prefetch_thread_id = -1;
static volatile bool prefetch_stop_requested = false;

static void stream_lock(void) {
    if (stream_sema < 0)
        stream_sema = create_semaphore(1, 1);

    wait_semaphore(stream_sema);
}

static void stream_unlock(void) {
    signal_semaphore(stream_sema);
}

/* --- format-specific chunk readers -------------------------------------
 * Called only by the prefetch thread, only while stream_lock is held. */

static int stream_fill_wav(SoundStream *snd, char *out, bool *ended) {
    int ret = fread(out, 1, AUDIO_STREAM_BUFFER_SIZE, snd->fp);

    *ended = false;

    if (ret < AUDIO_STREAM_BUFFER_SIZE) {
        fseek(snd->fp, snd->data_offset, SEEK_SET);

        if (!snd->loop)
            *ended = true;
    }

    return ret;
}

static int stream_fill_ogg(SoundStream *snd, char *out, bool *ended) {
    int bitStream = 0;
    int decodeTotal = AUDIO_STREAM_BUFFER_SIZE;
    int bufferPtr = 0;

    *ended = false;

    while (decodeTotal > 0) {
        int ret = ov_read(snd->fp, out + bufferPtr, decodeTotal, 0, 2, 1, &bitStream);

        if (ret > 0) {
            bufferPtr += ret;
            decodeTotal -= ret;
        } else if (ret < 0) {
            dbgprintf("ogg: I/O error while reading.\n");
            break;
        } else {
            ov_pcm_seek(snd->fp, 0);

            if (!snd->loop) {
                *ended = true;
                break;
            }
        }
    }

    return bufferPtr;
}

static int stream_fill_slot(SoundStream *snd, char *out, bool *ended) {
    if (!snd) {
        *ended = true;
        return 0;
    }

    switch (snd->type) {
        case WAV_AUDIO:
            return stream_fill_wav(snd, out, ended);
        case OGG_AUDIO:
            return stream_fill_ogg(snd, out, ended);
    }

    *ended = true;
    return 0;
}

/* --- prefetch producer thread ------------------------------------------ */

static void sound_stream_prefetch_thread(void *arg) {
    bool ended = false;

    while (!prefetch_stop_requested && !ended) {
        wait_semaphore(slot_free_sema);

        if (prefetch_stop_requested)
            break;

        StreamSlot *slot = &prefetch_slots[prefetch_write_idx];

        stream_lock();
        slot->size = stream_fill_slot(cur_snd, slot->data, &ended);
        stream_unlock();

        slot->ended = ended;
        prefetch_write_idx = (prefetch_write_idx + 1) % STREAM_PREFETCH_SLOTS;

        signal_semaphore(slot_filled_sema);
    }

    if (prefetch_stop_requested) {
        /* We own a free slot at this point (it was consumed by the
         * wait_semaphore(slot_free_sema) above before we saw the flag) --
         * use it to wake up a consumer that may already be blocked
         * waiting for filled data, so it never blocks forever. */
        StreamSlot *slot = &prefetch_slots[prefetch_write_idx];
        slot->size = 0;
        slot->ended = true;
        prefetch_write_idx = (prefetch_write_idx + 1) % STREAM_PREFETCH_SLOTS;
        signal_semaphore(slot_filled_sema);
    }

    stream_lock();
    prefetch_thread_id = -1;
    stream_unlock();

    signal_semaphore(prefetch_stopped_sema);

    exit_kill_task();
}

static void sound_stream_stop_prefetch(void) {
    stream_lock();
    bool active = (prefetch_thread_id >= 0);
    stream_unlock();

    if (!active)
        return;

    prefetch_stop_requested = true;
    signal_semaphore(slot_free_sema);

    wait_semaphore(prefetch_stopped_sema);
}

static bool sound_stream_reset_prefetch_ring(void) {
    if (!prefetch_slots) {
        prefetch_slots = (StreamSlot *)malloc(sizeof(StreamSlot) * STREAM_PREFETCH_SLOTS);
        if (!prefetch_slots)
            return false;
    }

    if (slot_free_sema >= 0)
        delete_semaphore(slot_free_sema);
    if (slot_filled_sema >= 0)
        delete_semaphore(slot_filled_sema);
    if (prefetch_stopped_sema >= 0)
        delete_semaphore(prefetch_stopped_sema);

    slot_free_sema = create_semaphore(STREAM_PREFETCH_SLOTS, STREAM_PREFETCH_SLOTS);
    slot_filled_sema = create_semaphore(0, STREAM_PREFETCH_SLOTS);
    prefetch_stopped_sema = create_semaphore(0, 1);

    prefetch_write_idx = 0;
    prefetch_read_idx = 0;
    prefetch_stop_requested = false;

    for (int i = 0; i < STREAM_PREFETCH_SLOTS; i++) {
        prefetch_slots[i].size = 0;
        prefetch_slots[i].ended = false;
    }
    return true;
}

/* --- audsrv-facing callback ---------------------------------------------
 * Invoked by audsrv's own RPC callback thread. Never touches fp/disk
 * directly anymore -- it only hands off a chunk the prefetch thread
 * already prepared, so it can't stall waiting on storage. */

static int sound_stream_fillbuf_handler(void *arg) {
    if (!stream_playing)
        return 0;

    wait_semaphore(slot_filled_sema);

    StreamSlot *slot = &prefetch_slots[prefetch_read_idx];
    prefetch_read_idx = (prefetch_read_idx + 1) % STREAM_PREFETCH_SLOTS;

    if (slot->size > 0)
        audsrv_play_audio(slot->data, slot->size); /* blocking: safe to recycle the slot once this returns */

    bool ended = slot->ended;

    signal_semaphore(slot_free_sema);

    if (ended)
        sound_pause();

    return 0;
}

SoundStream *load_wav(const char *path) {
    SoundStream *wav;
    t_wave header;
    long file_size;

    wav = malloc(sizeof(SoundStream));
    if (!wav)
        return NULL;

    wav->fp = fopen(path, "rb");
    if (!wav->fp) {
        free(wav);
        return NULL;
    }

    if (fread(&header, 1, sizeof(t_wave), wav->fp) != sizeof(t_wave)) {
        fclose(wav->fp);
        free(wav);
        return NULL;
    }

    /* Data starts right after whatever header we just read, instead of
     * assuming every WAV file places it at a fixed byte offset. */
    wav->data_offset = ftell(wav->fp);

    fseek(wav->fp, 0, SEEK_END);
    file_size = ftell(wav->fp);
    fseek(wav->fp, wav->data_offset, SEEK_SET);

    wav->fmt.bits = header.w_nbitspersample;
    wav->fmt.freq = header.w_samplespersec;
    wav->fmt.channels = header.w_nchannels;
    wav->type = WAV_AUDIO;
    wav->loop = false;

    wav->duration_ms = header.w_navgbytespersec
        ? (int)(((long long)(file_size - wav->data_offset) * 1000) / header.w_navgbytespersec)
        : 0;

    return wav;
}

SoundStream *load_ogg(const char *path) {
    FILE *oggFile;
    SoundStream *ogg;

    ogg = malloc(sizeof(SoundStream));
    if (!ogg)
        return NULL;

    ogg->fp = calloc(1, sizeof(OggVorbis_File));
    if (!ogg->fp) {
        free(ogg);
        return NULL;
    }

    oggFile = fopen(path, "rb");
    if (!oggFile) {
        dbgprintf("ogg: Failed to open Ogg file %s\n", path);
        free(ogg->fp);
        free(ogg);
        return NULL;
    }

    if (ov_open_callbacks(oggFile, ogg->fp, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
        dbgprintf("ogg: Input does not appear to be an Ogg bitstream.\n");
        fclose(oggFile);
        free(ogg->fp);
        free(ogg);
        return NULL;
    }

    vorbis_info *vi = ov_info(ogg->fp, -1);
    ov_pcm_seek(ogg->fp, 0);

    ogg->fmt.channels = vi->channels;
    ogg->fmt.freq = vi->rate;
    ogg->fmt.bits = 16;
    ogg->type = OGG_AUDIO;
    ogg->loop = false;
    ogg->data_offset = 0; /* unused for OGG */
    ogg->duration_ms = (int)(ov_time_total(ogg->fp, -1) * 1000);

    return ogg;
}

SoundStream *sound_load(const char *path) {
    FILE *f;
    uint32_t magic = 0;

    if (!path)
        return NULL;

    f = fopen(path, "rb");
    if (!f)
        return NULL;

    fread(&magic, 1, 4, f);
    fclose(f);

    switch (magic) {
        case 0x5367674F: /* OGG */
            return load_ogg(path);
        case 0x46464952: /* WAV */
            return load_wav(path);
    }

    return NULL;
}

void sound_play(SoundStream *snd) {
    if (!snd)
        return;

    stream_lock();

    if (stream_playing) {
        stream_unlock();
        return;
    }

    stream_playing = true;
    cur_snd = snd;

    audsrv_set_format(&(cur_snd->fmt));

    if (!sound_stream_reset_prefetch_ring()) {
        stream_playing = false;
        cur_snd = NULL;
        stream_unlock();
        return;
    }

    stream_unlock();

    int tid = create_task("AthenaEnv: Audio prefetch", sound_stream_prefetch_thread, STREAM_PREFETCH_STACK_SIZE, STREAM_PREFETCH_PRIORITY);
    if (tid < 0) {
        dbgprintf("sound: failed to create audio prefetch thread\n");
        stream_lock();
        stream_playing = false;
        cur_snd = NULL;
        stream_unlock();
        return;
    }

    stream_lock();
    prefetch_thread_id = tid;
    stream_unlock();

    init_task(tid, NULL);

    audsrv_on_fillbuf(AUDIO_STREAM_BUFFER_SIZE, sound_stream_fillbuf_handler, NULL);
    sound_stream_fillbuf_handler(NULL); // Kick the first chunk
}

int is_sound_playing(SoundStream *snd) {
    return ((snd == cur_snd) && stream_playing);
}

void sound_pause(void) {
    stream_lock();
    bool was_playing = stream_playing;
    stream_playing = false;
    stream_unlock();

    if (!was_playing)
        return;

    /* Must run unlocked: it rendezvous with the prefetch thread, which
     * needs stream_lock itself to finish its current chunk and notice
     * the stop request. */
    sound_stream_stop_prefetch();

    audsrv_wait_audio(AUDIO_STREAM_BUFFER_SIZE);
    audsrv_stop_audio();
}

void sound_free(SoundStream *snd) {
    if (!snd)
        return;

    if (snd == cur_snd)
        sound_pause();

    stream_lock();

    if (snd->type == OGG_AUDIO) {
        ov_clear(snd->fp);
        free(snd->fp);
    } else if (snd->type == WAV_AUDIO) {
        fclose(snd->fp);
    }

    snd->fp = NULL;

    if (snd == cur_snd)
        cur_snd = NULL;

    stream_unlock();

    free(snd);
}

void sound_setvolume(int volume) {
    audsrv_set_volume(volume);
    master_volume = volume;
}

void sound_rewind(SoundStream *snd) {
    if (!snd)
        return;

    stream_lock();

    switch (snd->type) {
        case OGG_AUDIO:
            ov_pcm_seek(snd->fp, 0);
            break;
        case WAV_AUDIO:
            fseek(snd->fp, snd->data_offset, SEEK_SET);
            break;
    }

    stream_unlock();
}

int sound_get_duration(SoundStream *snd) {
    /* Cached once at load time -- never touches fp, so it can be polled
     * every frame (e.g. for a seek bar) without ever glitching playback. */
    return snd ? snd->duration_ms : -1;
}

void sound_set_position(SoundStream *snd, int ms) {
    if (!snd || ms < 0 || ms >= sound_get_duration(snd))
        return;

    bool is_current = (snd == cur_snd);

    if (is_current)
        sound_pause();

    stream_lock();

    if (snd->type == OGG_AUDIO) {
        /* ov_pcm_seek() takes a position in sample-frames, not bytes --
         * rounding to AUDIO_STREAM_BUFFER_SIZE (a byte count with no
         * relation to the sample rate) doesn't belong here. */
        uint32_t frame_pos = (uint32_t)((long long)ms * snd->fmt.freq / 1000);
        ov_pcm_seek(snd->fp, frame_pos);
    } else if (snd->type == WAV_AUDIO) {
        uint32_t bytes_per_frame = snd->fmt.channels * (snd->fmt.bits / 8);
        uint32_t f_pos = (uint32_t)((long long)ms * snd->fmt.freq / 1000) * bytes_per_frame;
        fseek(snd->fp, snd->data_offset + f_pos, SEEK_SET);
    }

    stream_unlock();

    if (is_current)
        sound_play(snd);
}

int sound_get_position(SoundStream *snd) {
    uint32_t f_pos, ms;

    if (!snd)
        return -1;

    stream_lock();

    if (snd->type == OGG_AUDIO) {
        f_pos = ov_pcm_tell(snd->fp);
        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 16)));
    } else if (snd->type == WAV_AUDIO) {
        f_pos = ftell(snd->fp);
        ms = round(f_pos / (snd->fmt.freq / 1000 * (snd->fmt.bits / 4)));
    } else {
        stream_unlock();
        return -1;
    }

    stream_unlock();

    return ms;
}
