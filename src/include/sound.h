
#include <audsrv.h>
#include <vorbis/vorbisfile.h>
#include <stdbool.h>

#define WAV_AUDIO 0
#define OGG_AUDIO 1

#define AUDIO_STREAM_BUFFER_SIZE  4096

typedef struct _wave
{
    char  w_fileid[4];              /* chunk id 'RIFF'            */
    uint32_t w_chunksize;             /* chunk size                 */
    char  w_waveid[4];              /* wave chunk id 'WAVE'       */
    char  w_fmtid[4];               /* format chunk id 'fmt '     */
    uint32_t w_fmtchunksize;          /* format chunk size          */
    uint16_t  w_fmttag;               /* format tag, 1 for PCM      */
    uint16_t  w_nchannels;            /* number of channels         */
    uint32_t w_samplespersec;         /* sample rate in hz          */
    uint32_t w_navgbytespersec;       /* average bytes per second   */
    uint16_t  w_nblockalign;          /* number of bytes per sample */
    uint16_t  w_nbitspersample;       /* number of bits in a sample */
    char  w_datachunkid[4];           /* data chunk id 'data'       */
    uint32_t w_datachunksize;         /* length of data chunk       */
} t_wave;

typedef struct {
    void* fp;
    struct audsrv_fmt_t fmt;
    int type;
    bool loop;
} SoundStream;

SoundStream *sound_load(const char* path);

void sound_play(SoundStream * snd);

void sound_setvolume(int volume);
int is_sound_playing(SoundStream* snd);
int sound_get_duration(SoundStream* snd);
void sound_pause(void);
void sound_free(SoundStream* snd);
void sound_rewind(SoundStream* snd);
void sound_set_position(SoundStream* snd, int ms);
int sound_get_position(SoundStream* snd);

typedef struct {
    audsrv_adpcm_t sound;
    int sample_rate;
    int volume;
    int pan;
} Sfx;

Sfx* sound_sfx_load(const char* path);
void sound_sfx_free(Sfx* snd);
int sound_sfx_play(int channel, Sfx *snd);
void sound_sfx_channel_volume(int channel, int volume, int pan);
int sound_sfx_length(Sfx *snd);
int sound_sfx_find_channel();
bool sound_sfx_is_playing(Sfx *snd, int channel);
int sound_sfx_get_pitch(Sfx *snd);
void sound_sfx_set_pitch(Sfx *snd, int pitch);
