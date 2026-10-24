/*
 * synth.h - the software synthesizer: MIDI sequences and PCM clips mixed
 * to 16-bit stereo at SYNTH_RATE.
 *
 * SYNTH_SLOTS players sound at once, each playing one sequence or one clip.
 * Sequences drive a pool of voices with General MIDI instrument families
 * and drums. Integer arithmetic only: every platform makes the same samples.
 */
#ifndef MIDP_SYNTH_H
#define MIDP_SYNTH_H

#include <stdbool.h>
#include <stdint.h>

#define SYNTH_RATE  32768
#define SYNTH_SLOTS 6
/* Most frames one millisecond can have (SYNTH_RATE / 1000, rounded up). */
#define SYNTH_MS_FRAMES 33

/* A MIDI channel message at a time in milliseconds from the start. */
typedef struct SongEvent {
    uint32_t ms;
    uint8_t  status, d1, d2, pad;
} SongEvent;

/* Silences everything and resets the clock. */
void synth_reset(void);

/* Plays a sequence (events in time order, kept by the caller until stopped)
 * on a slot, from start_ms into it. volume is 0-100. */
void synth_song(int slot, const SongEvent *ev, uint32_t n, uint32_t start_ms, int volume);
/* Plays 16-bit mono samples at rate, from sample start. */
void synth_pcm(int slot, const int16_t *pcm, uint32_t samples, uint32_t rate, uint32_t start,
               int volume);
void synth_stop(int slot);
void synth_volume(int slot, int volume);
void synth_master(int volume);
bool synth_playing(int slot);

/* Makes the next millisecond of sound: 32 or 33 stereo frames (SYNTH_RATE
 * is not a multiple of 1000) into out. Returns the number of frames. */
int synth_render_ms(int16_t out[2 * SYNTH_MS_FRAMES]);

#endif
