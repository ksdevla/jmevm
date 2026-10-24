/*
 * synth.c - the software synthesizer of synth.h.
 *
 * Voices play 256-sample single-cycle waves (built at start from a fixed
 * sine table) or LFSR noise, shaped by attack-decay-sustain-release
 * envelopes. Each MIDI program family has its own wave and envelope;
 * channel 10 plays drums. Sequences advance in whole milliseconds; within
 * one, all voices and clips are mixed frame by frame.
 */
#include "synth.h"

#include <string.h>

#define VOICES    32
#define WAVE_LEN  256
#define ENV_MAX   (1 << 24)
#define DRUMS     9          /* MIDI channel 10 */
#define FAST_MS   5          /* release when a slot is stopped */

enum { W_SINE, W_TRIANGLE, W_SQUARE, W_SAW, W_ORGAN, W_WAVES, W_NOISE = W_WAVES };
enum { ATTACK, DECAY, SUSTAIN, RELEASE };
enum { EMPTY, SONG, PCM };

/* sin(2 pi k / 256) * 32767 for k = 0..64. */
static const int16_t quarter[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039,
    11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159,
    20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245,
    27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580,
    31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767};
/* 2^(s/12) * 65536: the semitones of an octave. */
static const uint32_t semitone[12] = {
    65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715};
/* 2^(k/384) * 65536: 32nds of a semitone, for pitch bend. */
static const uint32_t fine[32] = {
    65536, 65654, 65773, 65892, 66011, 66130, 66250, 66369, 66489, 66609, 66730, 66850, 66971,
    67092, 67213, 67335, 67456, 67578, 67700, 67823, 67945, 68068, 68191, 68314, 68438, 68561,
    68685, 68809, 68933, 69058, 69183, 69308};
/* Phase step of A4 (440 Hz): 440 * 2^32 / SYNTH_RATE. */
#define A4_STEP 57671680u

/* An instrument family: wave and envelope (times in ms, sustain in %). */
typedef struct Patch {
    uint8_t  wave;
    uint16_t attack, decay;
    uint8_t  sustain;
    uint16_t release;
} Patch;

/* The 16 General MIDI families, programs 0-7, 8-15, ... */
static const Patch families[16] = {
    {W_SAW, 2, 700, 25, 200},        /* piano */
    {W_SINE, 1, 500, 0, 150},        /* chromatic percussion */
    {W_ORGAN, 5, 0, 100, 60},        /* organ */
    {W_SAW, 2, 600, 15, 150},        /* guitar */
    {W_TRIANGLE, 2, 300, 60, 80},    /* bass */
    {W_SAW, 60, 200, 80, 250},       /* strings */
    {W_SAW, 80, 200, 80, 300},       /* ensemble */
    {W_SQUARE, 20, 120, 80, 120},    /* brass */
    {W_SQUARE, 15, 100, 85, 90},     /* reed */
    {W_TRIANGLE, 20, 100, 90, 120},  /* pipe */
    {W_SQUARE, 5, 60, 90, 90},       /* synth lead */
    {W_SAW, 150, 300, 80, 450},      /* synth pad */
    {W_TRIANGLE, 50, 400, 60, 300},  /* synth effects */
    {W_SAW, 3, 350, 40, 150},        /* ethnic */
    {W_SINE, 1, 250, 0, 80},         /* percussive */
    {W_TRIANGLE, 5, 300, 30, 200},   /* sound effects */
};

typedef struct Channel {
    uint8_t program, volume, expression, pan;
    bool    sustain;
    int16_t bend;   /* -8192..8191, two semitones each way */
} Channel;

typedef struct Voice {
    bool     on;
    bool     held;        /* key down */
    bool     sustained;   /* key up while the sustain pedal is down */
    bool     drum;
    uint8_t  slot, channel, note, velocity, wave;
    uint32_t phase, step;
    int      sweep;       /* > 0: the pitch falls (kick drum, toms) */
    uint16_t lfsr;
    int16_t  noise;
    int32_t  env, attack, decay, sustain, release;
    int      stage;
    uint32_t age;
    int32_t  gain_l, gain_r;   /* q16 */
} Voice;

typedef struct Slot {
    int              kind;
    int              volume;
    const SongEvent *ev;
    uint32_t         n, next, ms;
    const int16_t   *pcm;
    uint32_t         samples;
    uint64_t         pos, step;   /* 32.32 sample position and step */
    Channel          ch[16];
} Slot;

static int16_t  waves[W_WAVES][WAVE_LEN];
static bool     ready;
static Voice    voices[VOICES];
static Slot     slots[SYNTH_SLOTS];
static int      master = 100;
static uint64_t clock_ms;
static uint32_t ages;

/* ---- tables ---- */

static int32_t sine_at(int k)
{
    k &= 255;
    if (k <= 64)
        return quarter[k];
    if (k <= 128)
        return quarter[128 - k];
    if (k <= 192)
        return -quarter[k - 128];
    return -quarter[256 - k];
}

/* A wave as a sum of harmonics h with amplitudes amp[h] / 1000, scaled to
 * peak at 30000. */
static void harmonics(int16_t *wave, const int *amp, int count)
{
    int32_t sum[WAVE_LEN], peak = 1;
    for (int k = 0; k < WAVE_LEN; k++) {
        int64_t v = 0;
        for (int h = 1; h <= count; h++)
            v += (int64_t)sine_at(h * k) * amp[h - 1] / 1000;
        sum[k] = (int32_t)v;
        if (v > peak) peak = (int32_t)v;
        if (-v > peak) peak = (int32_t)-v;
    }
    for (int k = 0; k < WAVE_LEN; k++)
        wave[k] = (int16_t)((int64_t)sum[k] * 30000 / peak);
}

static void build_tables(void)
{
    static const int sine[1] = {1000};
    static const int square[9] = {1000, 0, 333, 0, 200, 0, 143, 0, 111};
    static const int saw[10] = {1000, -500, 333, -250, 200, -167, 143, -125, 111, -100};
    static const int organ[4] = {1000, 500, 250, 125};
    harmonics(waves[W_SINE], sine, 1);
    harmonics(waves[W_SQUARE], square, 9);
    harmonics(waves[W_SAW], saw, 10);
    harmonics(waves[W_ORGAN], organ, 4);
    for (int k = 0; k < WAVE_LEN; k++)
        waves[W_TRIANGLE][k] = (int16_t)(k < 128 ? -30000 + k * 468 : 30000 - (k - 128) * 468);
    ready = true;
}

/* Phase step of a pitch in 32nds of a semitone (MIDI note * 32). */
static uint32_t step_of(int32_t pitch)
{
    if (pitch < 0)
        pitch = 0;
    if (pitch > 127 * 32 + 31)
        pitch = 127 * 32 + 31;
    int      k = pitch / 32 - 69 + 120;   /* semitones from A4, made positive */
    int      octave = k / 12 - 10;
    uint64_t step = (uint64_t)A4_STEP * semitone[k % 12] >> 16;
    step = octave >= 0 ? step << octave : step >> -octave;
    return (uint32_t)(step * fine[pitch % 32] >> 16);
}

static int32_t per_ms(int ms)
{
    int32_t frames = (int32_t)((int64_t)ms * SYNTH_RATE / 1000);
    return frames > 0 ? frames : 1;
}

/* ---- voices ---- */

static void update_gain(Voice *v)
{
    const Slot    *s = &slots[v->slot];
    const Channel *c = &s->ch[v->channel];
    /* Velocity, channel volume and expression act on loudness as squares
     * (as General MIDI recommends); slot and master volume linearly. */
    int64_t g = (int64_t)v->velocity * v->velocity * c->volume * c->volume / (127 * 127);
    g = g * c->expression * c->expression / (127 * 127);
    g = g * s->volume * master * 65536 / (127LL * 127 * 100 * 100);
    int left = 2 * (127 - c->pan), right = 2 * c->pan;
    v->gain_l = (int32_t)(g * (left < 127 ? left : 127) / 127);
    v->gain_r = (int32_t)(g * (right < 127 ? right : 127) / 127);
}

static void release(Voice *v, int ms)
{
    v->held = false;
    v->sustained = false;
    v->stage = RELEASE;
    if (ms)
        v->release = ENV_MAX / per_ms(ms);
}

static Voice *allocate(void)
{
    Voice *best = NULL;
    for (int i = 0; i < VOICES; i++) {
        Voice *v = &voices[i];
        if (!v->on)
            return v;
        /* Otherwise the quietest releasing voice, or else the oldest. */
        if (!best || (v->stage == RELEASE && (best->stage != RELEASE || v->env < best->env))
            || (best->stage != RELEASE && v->stage != RELEASE && v->age < best->age))
            best = v;
    }
    return best;
}

static void drum_on(Voice *v, int note)
{
    int decay;
    v->drum = true;
    v->wave = W_NOISE;
    v->step = 0;
    if (note == 35 || note == 36) {   /* bass drum: a falling sine */
        v->wave = W_SINE;
        v->step = step_of(43 * 32);
        v->sweep = 11;
        decay = 200;
    } else if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) {
        v->wave = W_SINE;   /* toms */
        v->step = step_of((note + 12) * 32);
        v->sweep = 12;
        decay = 250;
    } else if (note == 42 || note == 44) {
        decay = 45;    /* closed hi-hat */
    } else if (note == 46) {
        decay = 250;   /* open hi-hat */
    } else if (note == 49 || note == 51 || note == 52 || note == 53 || note == 55 || note == 57
               || note == 59) {
        decay = 700;   /* cymbals */
    } else if (note >= 37 && note <= 40) {
        decay = 160;   /* snare, clap */
    } else {
        decay = 120;
    }
    v->attack = ENV_MAX;
    v->decay = ENV_MAX / per_ms(decay);
    v->sustain = 0;
    v->release = ENV_MAX / per_ms(30);
}

static void note_on(int slot, int ch, int note, int velocity)
{
    Slot *s = &slots[slot];
    for (int i = 0; i < VOICES; i++) {   /* the same key again: release it first */
        Voice *v = &voices[i];
        if (v->on && v->held && v->slot == slot && v->channel == ch && v->note == note)
            release(v, 0);
    }
    Voice *v = allocate();
    memset(v, 0, sizeof *v);
    v->on = true;
    v->held = true;
    v->slot = (uint8_t)slot;
    v->channel = (uint8_t)ch;
    v->note = (uint8_t)note;
    v->velocity = (uint8_t)velocity;
    v->lfsr = 0xACE1;
    v->age = ++ages;
    v->stage = ATTACK;
    if (ch == DRUMS) {
        drum_on(v, note);
    } else {
        const Patch *p = &families[s->ch[ch].program >> 3];
        v->wave = p->wave;
        v->step = step_of(note * 32 + s->ch[ch].bend / 128);
        v->attack = ENV_MAX / per_ms(p->attack);
        v->sustain = (int32_t)((int64_t)ENV_MAX * p->sustain / 100);
        v->decay = p->decay ? (ENV_MAX - v->sustain) / per_ms(p->decay) : ENV_MAX;
        v->release = ENV_MAX / per_ms(p->release);
    }
    update_gain(v);
}

static void note_off(int slot, int ch, int note)
{
    if (ch == DRUMS)
        return;   /* drums ring out by themselves */
    for (int i = 0; i < VOICES; i++) {
        Voice *v = &voices[i];
        if (v->on && v->held && v->slot == slot && v->channel == ch && v->note == note) {
            if (slots[slot].ch[ch].sustain) {
                v->held = false;
                v->sustained = true;
            } else {
                release(v, 0);
            }
        }
    }
}

static void each_voice(int slot, int ch, void (*fn)(Voice *))
{
    for (int i = 0; i < VOICES; i++)
        if (voices[i].on && voices[i].slot == slot && (ch < 0 || voices[i].channel == ch))
            fn(&voices[i]);
}

static void regain(Voice *v)
{
    update_gain(v);
}

static void rebend(Voice *v)
{
    if (!v->drum)
        v->step = step_of(v->note * 32 + slots[v->slot].ch[v->channel].bend / 128);
}

static void let_go(Voice *v)
{
    if (v->held || v->sustained)
        release(v, 0);
}

static void unpedal(Voice *v)
{
    if (v->sustained)
        release(v, 0);
}

static void cut(Voice *v)
{
    release(v, FAST_MS);
}

static void controller(int slot, int ch, int cc, int value)
{
    Channel *c = &slots[slot].ch[ch];
    switch (cc) {
    case 7:  c->volume = (uint8_t)value; each_voice(slot, ch, regain); break;
    case 10: c->pan = (uint8_t)value; each_voice(slot, ch, regain); break;
    case 11: c->expression = (uint8_t)value; each_voice(slot, ch, regain); break;
    case 64:
        c->sustain = value >= 64;
        if (!c->sustain)
            each_voice(slot, ch, unpedal);
        break;
    case 120: each_voice(slot, ch, cut); break;      /* all sound off */
    case 121:                                         /* reset controllers */
        c->expression = 127;
        c->bend = 0;
        c->sustain = false;
        each_voice(slot, ch, unpedal);
        each_voice(slot, ch, regain);
        each_voice(slot, ch, rebend);
        break;
    case 123: each_voice(slot, ch, let_go); break;   /* all notes off */
    default: break;
    }
}

/* Applies a channel message; with sounding false, only what changes state
 * (to start a sequence part-way). */
static void message(int slot, const SongEvent *e, bool sounding)
{
    int ch = e->status & 15;
    switch (e->status & 0xF0) {
    case 0x90:
        if (sounding && e->d2)
            note_on(slot, ch, e->d1, e->d2);
        else if (sounding)
            note_off(slot, ch, e->d1);
        break;
    case 0x80:
        if (sounding)
            note_off(slot, ch, e->d1);
        break;
    case 0xB0:
        controller(slot, ch, e->d1, e->d2);
        break;
    case 0xC0:
        slots[slot].ch[ch].program = e->d1;
        break;
    case 0xE0:
        slots[slot].ch[ch].bend = (int16_t)((e->d1 | (e->d2 << 7)) - 8192);
        each_voice(slot, ch, rebend);
        break;
    default:
        break;
    }
}

/* One sample of a voice, enveloped (before gain). */
static int32_t voice_sample(Voice *v)
{
    int32_t osc;
    if (v->wave == W_NOISE) {
        if ((v->phase += 0x80000000u) < 0x80000000u) {   /* a new noise value every other frame */
            v->lfsr = (uint16_t)((v->lfsr >> 1) ^ (-(v->lfsr & 1) & 0xB400u));
            v->noise = (v->lfsr & 1) ? 20000 : -20000;
        }
        osc = v->noise;
    } else {
        const int16_t *w = waves[v->wave];
        uint32_t       i = v->phase >> 24, frac = (v->phase >> 8) & 0xFFFF;
        osc = w[i] + (int32_t)(((int64_t)(w[(i + 1) & 255] - w[i]) * frac) >> 16);
        v->phase += v->step;
        if (v->sweep)
            v->step -= v->step >> v->sweep;
    }
    switch (v->stage) {
    case ATTACK:
        v->env += v->attack;
        if (v->env >= ENV_MAX) {
            v->env = ENV_MAX;
            v->stage = DECAY;
        }
        break;
    case DECAY:
        v->env -= v->decay;
        if (v->env <= v->sustain) {
            v->env = v->sustain;
            v->stage = SUSTAIN;
            if (v->sustain == 0)
                v->on = false;
        }
        break;
    case SUSTAIN:
        break;
    default:
        v->env -= v->release;
        if (v->env <= 0) {
            v->env = 0;
            v->on = false;
        }
        break;
    }
    return (int32_t)(((int64_t)osc * (v->env >> 8)) >> 16);
}

/* ---- the interface ---- */

static void clear_slot(int slot)
{
    Slot *s = &slots[slot];
    int   volume = s->volume;
    memset(s, 0, sizeof *s);
    s->volume = volume;
    for (int c = 0; c < 16; c++) {
        s->ch[c].volume = 100;
        s->ch[c].expression = 127;
        s->ch[c].pan = 64;
    }
}

void synth_reset(void)
{
    if (!ready)
        build_tables();
    memset(voices, 0, sizeof voices);
    for (int i = 0; i < SYNTH_SLOTS; i++) {
        slots[i].volume = 100;
        clear_slot(i);
    }
    master = 100;
    clock_ms = 0;
    ages = 0;
}

void synth_stop(int slot)
{
    if (slot < 0 || slot >= SYNTH_SLOTS)
        return;
    each_voice(slot, -1, cut);
    clear_slot(slot);
}

void synth_song(int slot, const SongEvent *ev, uint32_t n, uint32_t start_ms, int volume)
{
    if (slot < 0 || slot >= SYNTH_SLOTS)
        return;
    if (!ready)
        synth_reset();
    synth_stop(slot);
    Slot *s = &slots[slot];
    s->kind = SONG;
    s->volume = volume;
    s->ev = ev;
    s->n = n;
    s->ms = start_ms;
    while (s->next < n && ev[s->next].ms < start_ms)
        message(slot, &ev[s->next++], false);
}

void synth_pcm(int slot, const int16_t *pcm, uint32_t samples, uint32_t rate, uint32_t start,
               int volume)
{
    if (slot < 0 || slot >= SYNTH_SLOTS)
        return;
    if (!ready)
        synth_reset();
    synth_stop(slot);
    Slot *s = &slots[slot];
    s->kind = PCM;
    s->volume = volume;
    s->pcm = pcm;
    s->samples = samples;
    s->pos = (uint64_t)start << 32;
    s->step = ((uint64_t)rate << 32) / SYNTH_RATE;
}

void synth_volume(int slot, int volume)
{
    if (slot < 0 || slot >= SYNTH_SLOTS)
        return;
    slots[slot].volume = volume;
    each_voice(slot, -1, regain);
}

void synth_master(int volume)
{
    master = volume;
    for (int i = 0; i < VOICES; i++)
        if (voices[i].on)
            update_gain(&voices[i]);
}

bool synth_playing(int slot)
{
    if (slot < 0 || slot >= SYNTH_SLOTS)
        return false;
    const Slot *s = &slots[slot];
    if (s->kind == PCM)
        return (s->pos >> 32) < s->samples;
    if (s->kind == SONG && s->next < s->n)
        return true;
    for (int i = 0; i < VOICES; i++)
        if (voices[i].on && voices[i].slot == slot)
            return true;
    return false;
}

/* Above a knee, peaks are compressed 4:1 before the hard limit, so rare
 * loud moments round off instead of clipping. */
static int16_t limit(int64_t x)
{
    const int64_t knee = 20000;
    if (x > knee)
        x = knee + (x - knee) / 4;
    else if (x < -knee)
        x = -knee + (x + knee) / 4;
    return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}

int synth_render_ms(int16_t out[2 * SYNTH_MS_FRAMES])
{
    if (!ready)
        synth_reset();
    /* This millisecond's events, in each sequence. */
    for (int i = 0; i < SYNTH_SLOTS; i++) {
        Slot *s = &slots[i];
        if (s->kind != SONG)
            continue;
        while (s->next < s->n && s->ev[s->next].ms <= s->ms)
            message(i, &s->ev[s->next++], true);
        s->ms++;
    }
    int frames = (int)((clock_ms + 1) * SYNTH_RATE / 1000 - clock_ms * SYNTH_RATE / 1000);
    clock_ms++;

    for (int f = 0; f < frames; f++) {
        int64_t left = 0, right = 0;
        for (int i = 0; i < VOICES; i++) {
            Voice *v = &voices[i];
            if (!v->on)
                continue;
            int32_t x = voice_sample(v);
            left += ((int64_t)x * v->gain_l) >> 16;
            right += ((int64_t)x * v->gain_r) >> 16;
        }
        for (int i = 0; i < SYNTH_SLOTS; i++) {
            Slot *s = &slots[i];
            if (s->kind != PCM)
                continue;
            uint32_t at = (uint32_t)(s->pos >> 32);
            if (at >= s->samples)
                continue;
            /* Linear interpolation between neighbouring samples. */
            int32_t a = s->pcm[at], b = at + 1 < s->samples ? s->pcm[at + 1] : a;
            int32_t x = a + (int32_t)(((int64_t)(b - a) * (int64_t)((s->pos >> 16) & 0xFFFF)) >> 16);
            int64_t g = (int64_t)s->volume * master * 65536 / (100 * 100);
            left += (x * g) >> 16;
            right += (x * g) >> 16;
            s->pos += s->step;
        }
        out[2 * f] = limit(left / 4);   /* headroom for many voices at once */
        out[2 * f + 1] = limit(right / 4);
    }
    return frames;
}
