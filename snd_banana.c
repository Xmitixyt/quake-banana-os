/*
 * snd_banana.c - Banana OS sound driver for Quake
 */

#include "quakedef.h"
#include <banana.h>

#define SND_RATE 11025
#define SND_CHANNELS 2
#define SND_BITS 16
#define SND_BUFFER_SAMPLES 16384 // circular buffer size in stereo sample pairs

extern int soundtime;

static int snd_inited = 0;
static unsigned char dma_buf[SND_BUFFER_SAMPLES * (SND_BITS / 8) * SND_CHANNELS];
volatile dma_t sn;
static int last_paintedtime = 0;
static unsigned int start_ticks = 0;

qboolean SNDDMA_Init(void)
{
    snd_inited = 0;
    shm = &sn;
    shm->splitbuffer = 0;
    shm->samplebits = SND_BITS;
    shm->speed = SND_RATE;
    shm->channels = SND_CHANNELS;
    shm->samples = SND_BUFFER_SAMPLES * SND_CHANNELS;
    shm->submission_chunk = 1;
    shm->buffer = dma_buf;
    shm->samplepos = 0;

    memset(dma_buf, 0, sizeof(dma_buf));

    start_ticks = banana_ticks();
    last_paintedtime = 0;
    snd_inited = 1;

    Con_Printf("Banana OS sound initialized: %d Hz, %d-channel, %d-bit\n",
               shm->speed, shm->channels, shm->samplebits);
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    if (!snd_inited) return 0;

    unsigned int now = banana_ticks();
    unsigned int elapsed_ms = now - start_ticks;
    unsigned long total_samples = ((unsigned long)elapsed_ms * (unsigned long)shm->speed) / 1000UL;
    shm->samplepos = (int)((total_samples * (unsigned long)shm->channels) % (unsigned long)shm->samples);

    return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
    snd_inited = 0;
    banana_audio_stop();
}

void SNDDMA_Submit(void)
{
    if (!snd_inited) return;

    // Sync if paintedtime jumped forward due to level load or buffer wrap
    if (last_paintedtime < soundtime || paintedtime < last_paintedtime) {
        last_paintedtime = soundtime;
    }

    if (paintedtime <= last_paintedtime)
        return;

    // Only skip if kernel audio queue has >1.5s buffered (e.g. VM pause)
    // Note: banana_audio_queued_ms() includes the 300ms hardware ring AHEAD
    if (banana_audio_queued_ms() > 1500) {
        last_paintedtime = paintedtime;
        return;
    }

    int fullsamples = shm->samples / shm->channels;
    int bytes_per_sample = shm->channels * (shm->samplebits / 8);

    while (last_paintedtime < paintedtime) {
        int start_idx = last_paintedtime % fullsamples;
        int chunk = fullsamples - start_idx;
        int rem = paintedtime - last_paintedtime;
        if (chunk > rem) chunk = rem;

        void *ptr = shm->buffer + start_idx * bytes_per_sample;
        banana_play(ptr, (unsigned long)(chunk * bytes_per_sample),
                    shm->speed, shm->channels, shm->samplebits);
        last_paintedtime += chunk;
    }
}
