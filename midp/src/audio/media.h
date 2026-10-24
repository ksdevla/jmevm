/*
 * media.h - decoding the sound formats J2ME games ship: Standard MIDI Files,
 * JSR 135 tone sequences, and WAV (PCM, mu-law, A-law, IMA ADPCM).
 */
#ifndef MIDP_MEDIA_H
#define MIDP_MEDIA_H

#include <stdbool.h>
#include <stdint.h>

#include "synth.h"

enum { CLIP_SONG, CLIP_PCM };

typedef struct MediaClip {
    int        kind;
    SongEvent *ev;         /* CLIP_SONG: channel messages in time order */
    uint32_t   n;
    int16_t   *pcm;        /* CLIP_PCM: mono samples */
    uint32_t   samples;
    uint32_t   rate;
    uint32_t   length_ms;  /* to the end of the media */
} MediaClip;

/* Decodes data, recognised by its contents (or by a type naming tone
 * sequences, which have no signature). False for anything unsupported or
 * damaged. */
bool media_decode(const uint8_t *data, uint32_t len, const char *type, MediaClip *out);
void media_free(MediaClip *clip);

#endif
