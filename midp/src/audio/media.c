/*
 * media.c - sound formats into what the synthesizer plays.
 *
 * MIDI: Standard MIDI Files, formats 0, 1 and 2 (2 is played like 1), with
 * running status, tempo changes and SMPTE time. Every track's channel
 * messages are merged in time order and their times converted to
 * milliseconds through the tempo map.
 *
 * Tone sequences (JSR 135 ToneControl): notes, silences, blocks, repeats
 * and volume, as notes on MIDI channel 0.
 *
 * WAV: PCM (8 and 16 bit), mu-law, A-law and IMA ADPCM, mono or stereo
 * (mixed down), as 16-bit mono.
 */
#include "media.h"

#include <stdlib.h>
#include <string.h>

#define MAX_EVENTS (1u << 20)   /* what a damaged file can make us allocate */

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* A growing list of events. */
typedef struct Events {
    SongEvent *ev;
    uint32_t   n, cap;
    bool       failed;
} Events;

static void add(Events *e, SongEvent v)
{
    if (e->failed)
        return;
    if (e->n == e->cap) {
        uint32_t cap = e->cap ? e->cap * 2 : 256;
        SongEvent *grown = cap <= MAX_EVENTS ? realloc(e->ev, sizeof(SongEvent) * cap) : NULL;
        if (!grown) {
            e->failed = true;
            return;
        }
        e->ev = grown;
        e->cap = cap;
    }
    e->ev[e->n++] = v;
}

/* ---- MIDI ---- */

typedef struct Timed {
    uint32_t  tick;
    uint32_t  order;   /* track, then position: keeps simultaneous events in order */
    SongEvent ev;      /* ms filled in later */
} Timed;

typedef struct Tempo {
    uint32_t tick;
    uint32_t us_per_quarter;
} Tempo;

static int by_tick(const void *a, const void *b)
{
    const Timed *x = a, *y = b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    return x->order < y->order ? -1 : x->order > y->order;
}

static int tempo_by_tick(const void *a, const void *b)
{
    const Tempo *x = a, *y = b;
    return x->tick < y->tick ? -1 : x->tick > y->tick;
}

/* A variable-length quantity; false if it runs past end. */
static bool vlq(const uint8_t *d, uint32_t end, uint32_t *pos, uint32_t *out)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (*pos >= end)
            return false;
        uint8_t b = d[(*pos)++];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) {
            *out = v;
            return true;
        }
    }
    return false;
}

static bool decode_midi(const uint8_t *d, uint32_t len, MediaClip *out)
{
    if (len < 14 || memcmp(d, "MThd", 4) != 0 || be32(d + 4) < 6)
        return false;
    uint32_t header = be32(d + 4);
    unsigned tracks = (unsigned)((d[10] << 8) | d[11]);
    unsigned division = (unsigned)((d[12] << 8) | d[13]);
    if (division == 0 || header > len - 8)
        return false;

    Timed   *timed = NULL;
    uint32_t n = 0, cap = 0, end_tick = 0, order = 0;
    Tempo   *tempos = NULL;
    uint32_t n_tempo = 0, tempo_cap = 0;
    bool     ok = true;
    uint32_t pos = 8 + header;
    for (unsigned t = 0; t < tracks && ok && pos + 8 <= len; t++) {
        uint32_t size = be32(d + pos + 4);
        if (memcmp(d + pos, "MTrk", 4) != 0) {   /* unknown chunk: skip it */
            pos += 8 + (size < len - pos - 8 ? size : len - pos - 8);
            t--;
            continue;
        }
        uint32_t p = pos + 8, end = size < len - p ? p + size : len;
        uint32_t tick = 0;
        uint8_t  status = 0;
        while (p < end && ok) {
            uint32_t delta;
            if (!vlq(d, end, &p, &delta))
                break;
            tick += delta;
            if (p >= end)
                break;
            uint8_t b = d[p];
            if (b == 0xFF) {   /* meta event */
                uint32_t mlen;
                if (p + 2 > end)
                    break;
                uint8_t type = d[p + 1];
                p += 2;
                if (!vlq(d, end, &p, &mlen) || mlen > end - p)
                    break;
                if (type == 0x51 && mlen == 3) {
                    if (n_tempo == tempo_cap) {
                        tempo_cap = tempo_cap ? tempo_cap * 2 : 16;
                        Tempo *grown = tempo_cap <= MAX_EVENTS ? realloc(tempos, sizeof(Tempo) * tempo_cap) : NULL;
                        if (!grown) {
                            ok = false;
                            break;
                        }
                        tempos = grown;
                    }
                    tempos[n_tempo].tick = tick;
                    tempos[n_tempo].us_per_quarter = ((uint32_t)d[p] << 16) | (d[p + 1] << 8) | d[p + 2];
                    n_tempo++;
                }
                p += mlen;
                if (type == 0x2F)   /* end of track */
                    break;
                continue;
            }
            if (b == 0xF0 || b == 0xF7) {   /* system exclusive */
                uint32_t slen;
                p++;
                if (!vlq(d, end, &p, &slen) || slen > end - p)
                    break;
                p += slen;
                continue;
            }
            if (b & 0x80) {
                status = b;
                p++;
            } else if (!status) {
                break;   /* data without a status: damaged */
            }
            int nbytes = (status & 0xE0) == 0xC0 ? 1 : 2;   /* program change, channel pressure */
            if (status >= 0xF0 || p + nbytes > end)
                break;
            if (n == cap) {
                cap = cap ? cap * 2 : 1024;
                Timed *grown = cap <= MAX_EVENTS ? realloc(timed, sizeof(Timed) * cap) : NULL;
                if (!grown) {
                    ok = false;
                    break;
                }
                timed = grown;
            }
            Timed *e = &timed[n++];
            e->tick = tick;
            e->order = order++;
            e->ev.ms = 0;
            e->ev.status = status;
            e->ev.d1 = d[p] & 0x7F;
            e->ev.d2 = nbytes == 2 ? d[p + 1] & 0x7F : 0;
            e->ev.pad = 0;
            p += (uint32_t)nbytes;
        }
        if (tick > end_tick)
            end_tick = tick;
        pos = end;
    }
    if (!ok) {
        free(timed);
        free(tempos);
        return false;
    }
    if (n)
        qsort(timed, n, sizeof(Timed), by_tick);
    if (n_tempo)
        qsort(tempos, n_tempo, sizeof(Tempo), tempo_by_tick);

    /* Ticks to microseconds. With SMPTE timing the division gives frames a
     * second and ticks a frame; otherwise ticks a quarter note, at the tempo
     * of the moment (500000 us a quarter until the first tempo event). */
    uint64_t smpte = 0;
    if (division & 0x8000) {
        int fps = -(int8_t)(division >> 8);
        smpte = (uint64_t)(fps > 0 ? fps : 25) * (division & 0xFF ? division & 0xFF : 1);
    }
    out->ev = malloc(sizeof(SongEvent) * (n ? n : 1));
    if (!out->ev) {
        free(timed);
        free(tempos);
        return false;
    }
    uint64_t us_at = 0, tick_at = 0, us_per_quarter = 500000;
    uint32_t next_tempo = 0;
    for (uint32_t i = 0; i <= n; i++) {
        uint32_t tick = i < n ? timed[i].tick : end_tick;
        uint64_t us;
        if (smpte) {
            us = (uint64_t)tick * 1000000 / smpte;
        } else {
            while (next_tempo < n_tempo && tempos[next_tempo].tick <= tick) {
                us_at += (tempos[next_tempo].tick - tick_at) * us_per_quarter / division;
                tick_at = tempos[next_tempo].tick;
                if (tempos[next_tempo].us_per_quarter)
                    us_per_quarter = tempos[next_tempo].us_per_quarter;
                next_tempo++;
            }
            us = us_at + (tick - tick_at) * us_per_quarter / division;
        }
        if (i < n) {
            out->ev[i] = timed[i].ev;
            out->ev[i].ms = (uint32_t)(us / 1000);
        } else {
            out->length_ms = (uint32_t)(us / 1000);
        }
    }
    out->kind = CLIP_SONG;
    out->n = n;
    free(timed);
    free(tempos);
    return true;
}

/* ---- tone sequences ---- */

enum {
    TONE_SILENCE = -1, TONE_VERSION = -2, TONE_TEMPO = -3, TONE_RESOLUTION = -4,
    TONE_BLOCK_START = -5, TONE_BLOCK_END = -6, TONE_PLAY_BLOCK = -7, TONE_SET_VOLUME = -8,
    TONE_REPEAT = -9
};

#define TONE_PROGRAM 80   /* General MIDI square lead */
#define MAX_DEPTH    16   /* blocks played inside blocks */

typedef struct ToneState {
    const int8_t *seq;
    uint32_t      len;
    uint32_t      blocks[128];   /* where each block's events start; 0: undefined */
    int           tempo, resolution, velocity;
    uint64_t      us;
    Events        out;
} ToneState;

/* Plays one note (or silence, note < 0) of duration d. */
static void tone(ToneState *s, int note, int d)
{
    uint64_t us = (uint64_t)d * 60000000u / ((uint64_t)s->tempo * s->resolution);
    if (note >= 0) {
        add(&s->out, (SongEvent){(uint32_t)(s->us / 1000), 0x90, (uint8_t)note, (uint8_t)s->velocity, 0});
        add(&s->out, (SongEvent){(uint32_t)((s->us + us) / 1000), 0x80, (uint8_t)note, 0, 0});
    }
    s->us += us;
}

/* Plays events from start until the end of the sequence or a BLOCK_END,
 * following PLAY_BLOCK with an explicit stack instead of recursion. */
static bool tone_play(ToneState *s, uint32_t start)
{
    uint32_t stack[MAX_DEPTH];
    int      depth = 0;
    uint32_t p = start;
    for (;;) {
        if (p + 1 >= s->len) {
            if (depth == 0)
                return true;
            return false;   /* a block without its end */
        }
        int a = s->seq[p], b = s->seq[p + 1];
        p += 2;
        if (a >= 0) {
            if (b <= 0)
                return false;
            tone(s, a, b);
        } else if (a == TONE_SILENCE) {
            if (b <= 0)
                return false;
            tone(s, -1, b);
        } else if (a == TONE_SET_VOLUME) {
            if (b < 0 || b > 100)
                return false;
            s->velocity = b * 127 / 100;
        } else if (a == TONE_REPEAT) {
            if (b < 2 || p + 1 >= s->len)
                return false;
            int note = s->seq[p], d = s->seq[p + 1];
            p += 2;
            if (d <= 0 || note < TONE_SILENCE)
                return false;
            for (int i = 0; i < b; i++)
                tone(s, note, d);
        } else if (a == TONE_PLAY_BLOCK) {
            if (b < 0 || !s->blocks[b] || depth == MAX_DEPTH)
                return false;
            stack[depth++] = p;
            p = s->blocks[b];
        } else if (a == TONE_BLOCK_END) {
            if (depth == 0)
                return false;
            p = stack[--depth];
        } else {
            return false;
        }
    }
}

static bool decode_tones(const uint8_t *data, uint32_t len, MediaClip *out)
{
    ToneState *s = calloc(1, sizeof(ToneState));
    if (!s)
        return false;
    s->seq = (const int8_t *)data;
    s->len = len;
    s->tempo = 30;   /* 120 beats a minute */
    s->resolution = 64;
    s->velocity = 127;
    bool     ok = len >= 2 && len % 2 == 0 && s->seq[0] == TONE_VERSION && s->seq[1] == 1;
    uint32_t p = 2;
    /* Header: tempo and resolution, then block definitions. */
    while (ok && p + 1 < len) {
        int a = s->seq[p], b = s->seq[p + 1];
        if (a == TONE_TEMPO && b >= 5) {
            s->tempo = b;
        } else if (a == TONE_RESOLUTION && b >= 1) {
            s->resolution = b;
        } else if (a == TONE_BLOCK_START && b >= 0) {
            s->blocks[b] = p + 2;
            for (p += 2; p + 1 < len && !(s->seq[p] == TONE_BLOCK_END && s->seq[p + 1] == b); p += 2)
                ;
            ok = p + 1 < len;
        } else if (a == TONE_TEMPO || a == TONE_RESOLUTION || a == TONE_BLOCK_START) {
            ok = false;
        } else {
            break;
        }
        p += 2;
    }
    add(&s->out, (SongEvent){0, 0xC0, TONE_PROGRAM, 0, 0});
    ok = ok && tone_play(s, p) && !s->out.failed;
    if (ok) {
        out->kind = CLIP_SONG;
        out->ev = s->out.ev;
        out->n = s->out.n;
        out->length_ms = (uint32_t)(s->us / 1000);
    }
    if (!ok)
        free(s->out.ev);
    free(s);
    return ok;
}

/* ---- WAV ---- */

static int16_t from_ulaw(uint8_t u)
{
    u = (uint8_t)~u;
    int t = ((u & 0x0F) << 3) + 0x84;
    t <<= (u & 0x70) >> 4;
    return (int16_t)((u & 0x80) ? 0x84 - t : t - 0x84);
}

static int16_t from_alaw(uint8_t a)
{
    a ^= 0x55;
    int t = (a & 0x0F) << 4;
    int seg = (a & 0x70) >> 4;
    if (seg == 0)
        t += 8;
    else
        t = (t + 0x108) << (seg - 1);
    return (int16_t)((a & 0x80) ? t : -t);
}

static const int16_t ima_steps[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t ima_index_change[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

typedef struct Ima {
    int sample, index;
} Ima;

static int16_t ima_next(Ima *s, unsigned code)
{
    int step = ima_steps[s->index];
    int diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    s->sample += (code & 8) ? -diff : diff;
    s->sample = s->sample > 32767 ? 32767 : s->sample < -32768 ? -32768 : s->sample;
    s->index += ima_index_change[code & 15];
    s->index = s->index < 0 ? 0 : s->index > 88 ? 88 : s->index;
    return (int16_t)s->sample;
}

/* IMA ADPCM blocks (mono or stereo) into mono samples. Returns the count. */
static uint32_t decode_ima(const uint8_t *d, uint32_t len, int channels, uint32_t block,
                           int16_t *out, uint32_t max)
{
    uint32_t n = 0;
    int32_t  frame[2][8];
    for (uint32_t at = 0; at + 4u * channels <= len && n < max; at += block) {
        uint32_t size = block < len - at ? block : len - at;
        Ima      st[2];
        for (int c = 0; c < channels; c++) {
            st[c].sample = (int16_t)le16(d + at + 4 * c);
            st[c].index = d[at + 4 * c + 2] > 88 ? 88 : d[at + 4 * c + 2];
        }
        int32_t first = channels == 2 ? (st[0].sample + st[1].sample) / 2 : st[0].sample;
        out[n++] = (int16_t)first;
        /* Then words of 4 bytes per channel, 8 samples each, interleaved. */
        for (uint32_t p = 4u * channels; p + 4u * channels <= size && n < max; p += 4u * channels) {
            for (int c = 0; c < channels; c++) {
                for (int k = 0; k < 8; k++) {
                    uint8_t byte = d[at + p + 4 * c + k / 2];
                    frame[c][k] = ima_next(&st[c], k & 1 ? byte >> 4 : byte & 15);
                }
            }
            for (int k = 0; k < 8 && n < max; k++)
                out[n++] = (int16_t)(channels == 2 ? (frame[0][k] + frame[1][k]) / 2 : frame[0][k]);
        }
    }
    return n;
}

static bool decode_wav(const uint8_t *d, uint32_t len, MediaClip *out)
{
    if (len < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0)
        return false;
    unsigned       format = 0, channels = 0, bits = 0, block = 0;
    uint32_t       rate = 0, size = 0;
    const uint8_t *data = NULL;
    for (uint32_t p = 12; p + 8 <= len;) {
        uint32_t chunk = le32(d + p + 4);
        const uint8_t *body = d + p + 8;
        uint32_t avail = len - p - 8;
        if (memcmp(d + p, "fmt ", 4) == 0 && chunk >= 16 && avail >= 16) {
            format = le16(body);
            channels = le16(body + 2);
            rate = le32(body + 4);
            block = le16(body + 12);
            bits = le16(body + 14);
        } else if (memcmp(d + p, "data", 4) == 0) {
            data = body;
            size = chunk < avail ? chunk : avail;   /* a truncated file plays what it has */
            break;
        }
        if (chunk > avail)
            break;
        p += 8 + chunk + (chunk & 1);
    }
    if (!data || channels < 1 || channels > 2 || rate < 1000 || rate > 96000)
        return false;

    uint32_t max;
    switch (format) {
    case 1:
        if (bits != 8 && bits != 16)
            return false;
        max = size / (bits / 8) / channels;
        break;
    case 6:
    case 7:
        max = size / channels;
        break;
    case 0x11:
        if (block < 4 * channels)
            return false;
        max = size * 2 / channels + 2;
        break;
    default:
        return false;
    }
    int16_t *pcm = malloc(sizeof(int16_t) * (max ? max : 1));
    if (!pcm)
        return false;
    uint32_t n = 0;
    if (format == 0x11) {
        n = decode_ima(data, size, (int)channels, block, pcm, max);
    } else {
        for (; n < max; n++) {
            int32_t sum = 0;
            for (unsigned c = 0; c < channels; c++) {
                uint32_t i = n * channels + c;
                if (format == 1 && bits == 8)
                    sum += (data[i] - 128) << 8;
                else if (format == 1)
                    sum += (int16_t)le16(data + 2 * i);
                else if (format == 7)
                    sum += from_ulaw(data[i]);
                else
                    sum += from_alaw(data[i]);
            }
            pcm[n] = (int16_t)(sum / (int32_t)channels);
        }
    }
    out->kind = CLIP_PCM;
    out->pcm = pcm;
    out->samples = n;
    out->rate = rate;
    out->length_ms = (uint32_t)((uint64_t)n * 1000 / rate);
    return true;
}

/* ---- */

bool media_decode(const uint8_t *data, uint32_t len, const char *type, MediaClip *out)
{
    memset(out, 0, sizeof *out);
    if (len >= 4 && memcmp(data, "MThd", 4) == 0)
        return decode_midi(data, len, out);
    if (len >= 4 && memcmp(data, "RIFF", 4) == 0)
        return decode_wav(data, len, out);
    if ((type && strstr(type, "tone")) || (len >= 2 && (int8_t)data[0] == TONE_VERSION && data[1] == 1))
        return decode_tones(data, len, out);
    return false;
}

void media_free(MediaClip *clip)
{
    free(clip->ev);
    free(clip->pcm);
    memset(clip, 0, sizeof *clip);
}
