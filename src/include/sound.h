
#include <audsrv.h>
#include <vorbis/vorbisfile.h>

void sound_setvolume(int volume);
void sound_setformat(int bits, int freq, int channels);

void sound_setadpcmvolume(int slot, int volume);
audsrv_adpcm_t* sound_loadadpcm(const char* path);
void sound_playadpcm(int slot, audsrv_adpcm_t *sample);

int ogg_load_play(const char* path);
void ogg_unload_stop(void);
int is_ogg_playing(void);
void set_ogg_repeat(bool repeat);
void ogg_pause(void);
void ogg_resume(void);