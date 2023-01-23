
#include <audsrv.h>

void sound_setvolume(int volume);
void sound_setformat(int bits, int freq, int channels);
void sound_setadpcmvolume(int slot, int volume);
audsrv_adpcm_t* sound_loadadpcm(const char* path);
void sound_playadpcm(int slot, audsrv_adpcm_t *sample);