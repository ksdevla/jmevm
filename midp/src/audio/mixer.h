/*
 * mixer.h - the synthesizer (synth.h) driven by the emulated clock, for
 * platforms that play the resulting samples: it implements the
 * pal_audio_* functions of pal.h.
 */
#ifndef MIDP_MIXER_H
#define MIDP_MIXER_H

#include <stdint.h>

#include "synth.h"

#define MIXER_RATE SYNTH_RATE

/* Receives one stereo frame (left, right). */
typedef void (*MixerOut)(const int16_t frame[2], void *user);

/* Starts the clock at now_ms (emulated milliseconds). */
void mixer_reset(int64_t now_ms);

/* Makes the sound of every millisecond up to now_ms and passes it to out. */
void mixer_advance(int64_t now_ms, MixerOut out, void *user);

#endif
