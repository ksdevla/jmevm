/*
 * mixer.c - see mixer.h.
 */
#include "mixer.h"
#include "../pal/pal.h"

#include <stdbool.h>

static int64_t last_ms;
static bool    started;

static void start(void)
{
    if (!started) {
        synth_reset();
        started = true;
    }
}

void mixer_reset(int64_t now_ms)
{
    start();
    last_ms = now_ms;
}

void mixer_advance(int64_t now_ms, MixerOut out, void *user)
{
    start();
    if (now_ms < last_ms)
        last_ms = now_ms;   /* the clock went back: a new VM, or a loaded state */
    int16_t buf[2 * SYNTH_MS_FRAMES];
    for (; last_ms < now_ms; last_ms++) {
        int frames = synth_render_ms(buf);
        for (int i = 0; out && i < frames; i++)
            out(buf + 2 * i, user);
    }
}

/* ---- pal.h ---- */

bool pal_audio_available(void)
{
    return true;
}

void pal_audio_song(int slot, const SongEvent *ev, uint32_t n, uint32_t start_ms, int volume)
{
    start();
    synth_song(slot, ev, n, start_ms, volume);
}

void pal_audio_pcm(int slot, const int16_t *pcm, uint32_t samples, uint32_t rate, uint32_t start_at,
                   int volume)
{
    start();
    synth_pcm(slot, pcm, samples, rate, start_at, volume);
}

void pal_audio_stop(int slot)
{
    start();
    synth_stop(slot);
}

void pal_audio_volume(int slot, int volume)
{
    start();
    synth_volume(slot, volume);
}
