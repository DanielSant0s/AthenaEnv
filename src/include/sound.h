
#include <audsrv.h>
#include <vorbis/vorbisfile.h>

#define WAV_AUDIO 0
#define OGG_AUDIO 1
#define ADPCM_AUDIO 2

#define STREAM_THREAD_BASE_PRIO  0x10
#define STREAM_THREAD_STACK_SIZE 0x1000

#define STREAM_RING_BUFFER_COUNT 16
#define STREAM_RING_BUFFER_SIZE  4096

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
} Sound;

void sound_setadpcmvolume(int slot, int volume);

void sound_setvolume(int volume);
int is_sound_playing(void);
int sound_get_duration(Sound* snd);
void set_sound_repeat(bool repeat);
void sound_pause(void);
void sound_resume(Sound* snd);
void sound_free(Sound* snd);
void sound_deinit(void);
void sound_restart(void);
void sound_set_position(Sound* snd, int ms);
int sound_get_position(Sound* snd);

audsrv_adpcm_t* sound_loadadpcm(const char* path);
void sound_playadpcm(int slot, audsrv_adpcm_t *sample);

Sound* load_wav(const char* path);
void play_wav(Sound* wav);

Sound* load_ogg(const char* path);
void play_ogg(Sound* ogg);