#ifndef __SOUND__
#define __SOUND__

#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C"
{
#endif 

typedef struct 
{
    char *filePath;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t period_size;
    unsigned int rate;
    int dir;
    FILE *wav_file;
} AudioPlayer;

void AudioPlayer_play(AudioPlayer *player);
int AudioPlayer_initialize(AudioPlayer *player);
void AudioPlayer_destroy(AudioPlayer *player);
void AudioPlayer_init(AudioPlayer *player, char *filePath);

#ifdef __cplusplus
}
#endif
#endif
