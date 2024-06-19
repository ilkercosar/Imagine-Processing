#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "sound.h"

void AudioPlayer_init(AudioPlayer *player, char *filePath) 
{
    player->filePath = filePath;
    player->handle = NULL;
    player->params = NULL;
    player->period_size = 0;
    player->rate = 44100;
    player->dir = 0;
    player->wav_file = NULL;
}

void AudioPlayer_destroy(AudioPlayer *player) 
{
    if (player->wav_file) fclose(player->wav_file);

    if (player->handle) 
    {
        snd_pcm_drain(player->handle);
        snd_pcm_close(player->handle);
    }
}

int AudioPlayer_initialize(AudioPlayer *player) 
{
    if (snd_pcm_open(&player->handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) 
    {
        fprintf(stderr, "Error opening PCM device.\n");
        return 0;
    }

    snd_pcm_hw_params_alloca(&player->params);
    snd_pcm_hw_params_any(player->handle, player->params);
    snd_pcm_hw_params_set_access(player->handle, player->params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(player->handle, player->params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(player->handle, player->params, 2);
    snd_pcm_hw_params_set_rate_near(player->handle, player->params, &player->rate, &player->dir);

    if (snd_pcm_hw_params(player->handle, player->params) < 0) 
    {
        fprintf(stderr, "Error setting HW parameters.\n");
        return 0;
    }

    snd_pcm_hw_params_get_period_size(player->params, &player->period_size, &player->dir);

    player->wav_file = fopen(player->filePath, "rb");
    if (!player->wav_file) 
    {
        fprintf(stderr, "Cannot open wav file: %s\n", player->filePath);
        return 0;
    }

    return 1;
}

void *play_thread(void *arg) 
{
    AudioPlayer *player = (AudioPlayer *)arg;
    
    short *buf = (short *)malloc(player->period_size * 2 * sizeof(short));
    if (!buf) 
    {
        fprintf(stderr, "Memory allocation error.\n");
        pthread_exit(NULL);
    }

    int rc;
    while ((rc = fread(buf, sizeof(short), player->period_size * 2, player->wav_file)) == player->period_size * 2) 
    {
        int written = snd_pcm_writei(player->handle, buf, player->period_size);
        if (written == -EPIPE) snd_pcm_prepare(player->handle);
        else if (written < 0) fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(written));
    }

    free(buf);
    pthread_exit(NULL);
}

void AudioPlayer_play(AudioPlayer *player) 
{
    if (!player->wav_file || !player->handle) 
    {
        fprintf(stderr, "File or handle not initialized properly.\n");
        return;
    }

    // Thread oluşturma
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, play_thread, (void *)player);
    if (ret) 
    {
        fprintf(stderr, "Error creating thread: %d\n", ret);
        return;
    }

    // Thread'in tamamlanmasını bekleyelim (Opsiyonel)
    pthread_join(thread, NULL);
}
