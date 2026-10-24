/*
 * audio.c - jmevm.midp.Audio on jmevm: decoded clips and the playback slots
 * they use. Java (javax.microedition.media) keeps track of timing, looping
 * and events; this side starts, stops and mixes, like src/midp.
 *
 * For snapshots every clip keeps its original bytes, and every slot the
 * media time and emulated time it started at. Loading a snapshot rebuilds
 * the clips and, at the next poll (when the VM's clock is the loaded one),
 * restarts what was playing at the position it has reached.
 */
#include "midp_jme.h"
#include "../audio/media.h"
#include "../pal/pal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIPS 48
#define TONE -1

typedef struct {
    bool     used;
    MediaClip  clip;
    int      slot;      /* playback slot while started, else -1 */
    int      volume;
    char     type[64];
    uint8_t *data;      /* the original bytes, for snapshots */
    uint32_t len;
} Clip;

typedef struct {
    int      owner;     /* clip handle, TONE, or 0 if free */
    uint32_t started;   /* start order, to take over the oldest slot */
    uint32_t start_ms;  /* media time it started at */
    int64_t  started_at; /* jme_time_ms() when it started */
    int      tone_note, tone_ms, tone_volume;
} Slot;

static Clip     clips[MAX_CLIPS + 1];    /* handles are 1..MAX_CLIPS */
static Slot     slots[SYNTH_SLOTS];
static uint32_t start_counter;
static SongEvent tone_events[SYNTH_SLOTS][3];
static bool     trace;
static bool     resync;                  /* restart sounds at the next poll */

static Clip *clip_of(int h)
{
    return h >= 1 && h <= MAX_CLIPS && clips[h].used ? &clips[h] : NULL;
}

static void release_slot(int slot)
{
    pal_audio_stop(slot);
    int h = slots[slot].owner;
    if (h > 0 && clips[h].slot == slot)
        clips[h].slot = -1;
    slots[slot].owner = 0;
}

/* A free slot, or the one started longest ago. */
static int grab_slot(void)
{
    int best = 0;
    for (int s = 0; s < SYNTH_SLOTS; s++) {
        if (slots[s].owner == 0)
            return s;
        if (slots[s].started < slots[best].started)
            best = s;
    }
    release_slot(best);
    return best;
}

/* Starts clip h on its slot at media time ms. */
static void play_clip(int h, uint32_t ms)
{
    Clip *c = &clips[h];
    if (c->clip.kind == CLIP_SONG) {
        pal_audio_song(c->slot, c->clip.ev, c->clip.n, ms, c->volume);
    } else {
        uint32_t start = (uint32_t)((uint64_t)ms * c->clip.rate / 1000);
        if (start < c->clip.samples)
            pal_audio_pcm(c->slot, c->clip.pcm, c->clip.samples, c->clip.rate, start, c->volume);
    }
}

static void play_tone(int slot, uint32_t elapsed_ms)
{
    Slot     *s = &slots[slot];
    SongEvent *e = tone_events[slot];
    if (elapsed_ms >= (uint32_t)s->tone_ms)
        return;
    e[0] = (SongEvent){0, 0xC0, 80, 0, 0};
    e[1] = (SongEvent){0, 0x90, (uint8_t)s->tone_note, 127, 0};
    e[2] = (SongEvent){(uint32_t)s->tone_ms, 0x80, (uint8_t)s->tone_note, 0, 0};
    pal_audio_song(slot, e, 3, elapsed_ms, s->tone_volume);
}

static bool open_clip(int h, const uint8_t *data, uint32_t len, const char *type)
{
    Clip *c = &clips[h];
    c->data = malloc(len ? len : 1);
    if (!c->data)
        return false;
    memcpy(c->data, data, len);
    if (!media_decode(c->data, len, type, &c->clip)) {
        free(c->data);
        c->data = NULL;
        return false;
    }
    c->len = len;
    snprintf(c->type, sizeof c->type, "%s", type);
    c->used = true;
    c->slot = -1;
    c->volume = 100;
    return true;
}

static void close_clip(int h)
{
    Clip *c = &clips[h];
    if (c->slot >= 0)
        release_slot(c->slot);
    media_free(&c->clip);
    free(c->data);
    memset(c, 0, sizeof *c);
}

/* ---- natives ---- */

static int audio_available(JmeCall *c)
{
    jme_return_int(c, pal_audio_available());
    return JME_NATIVE_RETURN;
}

/* static int open(byte[] data, String contentType): a handle, or 0 */
static int audio_open(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    JmeRef data = jme_arg_ref(c, 0);
    char   type[64] = "";
    if (!data)
        return JME_NATIVE_RETURN;
    if (jme_arg_ref(c, 1)) {
        char *t = jme_string_utf8(vm, jme_arg_ref(c, 1));
        if (t)
            snprintf(type, sizeof type, "%s", t);
        free(t);
    }
    int h = 1;
    while (h <= MAX_CLIPS && clips[h].used)
        h++;
    if (h > MAX_CLIPS)
        return JME_NATIVE_RETURN;
    data = jme_arg_ref(c, 0);
    uint32_t len = jme_array_length(vm, data);
    bool     ok = open_clip(h, jme_array_data(vm, data), len, type);
    if (trace)
        fprintf(stderr, "[audio open %s, %u bytes: %s]\n", type, (unsigned)len,
                ok ? (clips[h].clip.kind == CLIP_SONG ? "song" : "pcm") : "unsupported");
    if (ok)
        jme_return_int(c, h);
    return JME_NATIVE_RETURN;
}

/* static long duration(int handle): microseconds */
static int audio_duration(JmeCall *c)
{
    Clip *cl = clip_of(jme_arg_int(c, 0));
    jme_return_long(c, cl ? (int64_t)cl->clip.length_ms * 1000 : -1);
    return JME_NATIVE_RETURN;
}

/* static void start(int handle, long mediaTime) */
static int audio_start(JmeCall *c)
{
    int   h = jme_arg_int(c, 0);
    Clip *cl = clip_of(h);
    if (!cl)
        return JME_NATIVE_RETURN;
    int64_t  us = jme_arg_long(c, 1);
    uint32_t ms = us > 0 ? (uint32_t)(us / 1000) : 0;
    int      slot = cl->slot >= 0 ? cl->slot : grab_slot();
    if (trace)
        fprintf(stderr, "[audio start %d at %u ms on slot %d]\n", h, (unsigned)ms, slot);
    cl->slot = slot;
    slots[slot].owner = h;
    slots[slot].started = ++start_counter;
    slots[slot].start_ms = ms;
    slots[slot].started_at = jme_time_ms(jme_call_vm(c));
    play_clip(h, ms);
    return JME_NATIVE_RETURN;
}

static int audio_stop(JmeCall *c)
{
    Clip *cl = clip_of(jme_arg_int(c, 0));
    if (cl && cl->slot >= 0)
        release_slot(cl->slot);
    return JME_NATIVE_RETURN;
}

/* static void setVolume(int handle, int level) */
static int audio_set_volume(JmeCall *c)
{
    Clip *cl = clip_of(jme_arg_int(c, 0));
    if (!cl)
        return JME_NATIVE_RETURN;
    int v = jme_arg_int(c, 1);
    cl->volume = v < 0 ? 0 : v > 100 ? 100 : v;
    if (cl->slot >= 0)
        pal_audio_volume(cl->slot, cl->volume);
    return JME_NATIVE_RETURN;
}

static int audio_close(JmeCall *c)
{
    int h = jme_arg_int(c, 0);
    if (clip_of(h))
        close_clip(h);
    return JME_NATIVE_RETURN;
}

/* static void playTone(int note, int durationMs, int volume) */
static int audio_play_tone(JmeCall *c)
{
    int note = jme_arg_int(c, 0), ms = jme_arg_int(c, 1), vol = jme_arg_int(c, 2);
    if (note < 0 || note > 127 || ms <= 0)
        return JME_NATIVE_RETURN;
    int   slot = grab_slot();
    Slot *s = &slots[slot];
    s->owner = TONE;
    s->started = ++start_counter;
    s->start_ms = 0;
    s->started_at = jme_time_ms(jme_call_vm(c));
    s->tone_note = note;
    s->tone_ms = ms;
    s->tone_volume = vol < 0 ? 0 : vol > 100 ? 100 : vol;
    play_tone(slot, 0);
    return JME_NATIVE_RETURN;
}

static const JmeNative natives[] = {
    {"jmevm/midp/Audio", "available", NULL, audio_available},
    {"jmevm/midp/Audio", "open", NULL, audio_open},
    {"jmevm/midp/Audio", "duration", NULL, audio_duration},
    {"jmevm/midp/Audio", "start", NULL, audio_start},
    {"jmevm/midp/Audio", "stop", NULL, audio_stop},
    {"jmevm/midp/Audio", "setVolume", NULL, audio_set_volume},
    {"jmevm/midp/Audio", "close", NULL, audio_close},
    {"jmevm/midp/Audio", "playTone", NULL, audio_play_tone},
};

/* ---- state ---- */

static JmeStatus save(void *user, JmeWriteFn write, void *write_user)
{
    (void)user;
    StateOut o = {write, write_user, JME_OK};
    state_put_u32(&o, start_counter);
    for (int h = 1; h <= MAX_CLIPS; h++) {
        Clip *c = &clips[h];
        state_put_u32(&o, c->used);
        if (!c->used)
            continue;
        state_put(&o, c->type, sizeof c->type);
        state_put_u32(&o, (uint32_t)c->volume);
        state_put_u32(&o, (uint32_t)c->slot);
        state_put_u32(&o, c->len);
        state_put(&o, c->data, c->len);
    }
    for (int s = 0; s < SYNTH_SLOTS; s++) {
        Slot *sl = &slots[s];
        state_put_u32(&o, (uint32_t)sl->owner);
        state_put_u32(&o, sl->started);
        state_put_u32(&o, sl->start_ms);
        state_put_u32(&o, (uint32_t)sl->started_at);
        state_put_u32(&o, (uint32_t)((uint64_t)sl->started_at >> 32));
        state_put_u32(&o, (uint32_t)sl->tone_note);
        state_put_u32(&o, (uint32_t)sl->tone_ms);
        state_put_u32(&o, (uint32_t)sl->tone_volume);
    }
    return o.status;
}

static JmeStatus load(void *user, const void *data, size_t len)
{
    (void)user;
    /* Check the layout first; on an error nothing may change. */
    StateIn in = {data, (const uint8_t *)data + len, false};
    state_get_u32(&in);
    for (int h = 1; h <= MAX_CLIPS && !in.bad; h++) {
        if (!state_get_u32(&in))
            continue;
        const uint8_t *type = state_get(&in, 64);
        if (type && !memchr(type, 0, 64))
            in.bad = true;
        state_get_u32(&in);
        uint32_t slot = state_get_u32(&in);
        if (slot != UINT32_MAX && slot >= SYNTH_SLOTS)
            in.bad = true;
        state_get(&in, state_get_u32(&in));
    }
    for (int s = 0; s < SYNTH_SLOTS && !in.bad; s++)
        for (int k = 0; k < 8; k++)
            state_get_u32(&in);
    if (in.bad || in.p != in.end)
        return JME_E_FORMAT;

    for (int s = 0; s < SYNTH_SLOTS; s++)
        pal_audio_stop(s);
    for (int h = 1; h <= MAX_CLIPS; h++)
        if (clips[h].used)
            close_clip(h);
    in = (StateIn){data, (const uint8_t *)data + len, false};
    start_counter = state_get_u32(&in);
    for (int h = 1; h <= MAX_CLIPS; h++) {
        if (!state_get_u32(&in))
            continue;
        char type[64];
        memcpy(type, state_get(&in, 64), 64);
        int            volume = (int)state_get_u32(&in);
        int            slot = (int)state_get_u32(&in);
        uint32_t       n = state_get_u32(&in);
        const uint8_t *bytes = state_get(&in, n);
        if (!open_clip(h, bytes, n, type))
            continue; /* decoded before, so only out of memory gets here */
        clips[h].volume = volume;
        clips[h].slot = slot;
    }
    for (int s = 0; s < SYNTH_SLOTS; s++) {
        Slot *sl = &slots[s];
        sl->owner = (int)state_get_u32(&in);
        sl->started = state_get_u32(&in);
        sl->start_ms = state_get_u32(&in);
        uint64_t lo = state_get_u32(&in);
        sl->started_at = (int64_t)(lo | (uint64_t)state_get_u32(&in) << 32);
        sl->tone_note = (int)state_get_u32(&in);
        sl->tone_ms = (int)state_get_u32(&in);
        sl->tone_volume = (int)state_get_u32(&in);
        if (sl->owner > 0 && (!clip_of(sl->owner) || clips[sl->owner].slot != s))
            sl->owner = 0;
    }
    resync = true;
    return JME_OK;
}

/* After a snapshot load: restart what was playing where it has got to. */
void midp_jme_audio_resync(JmeVM *vm)
{
    if (!resync)
        return;
    resync = false;
    int64_t now = jme_time_ms(vm);
    for (int s = 0; s < SYNTH_SLOTS; s++) {
        Slot    *sl = &slots[s];
        uint32_t elapsed = now > sl->started_at ? (uint32_t)(now - sl->started_at) : 0;
        if (sl->owner == TONE)
            play_tone(s, elapsed);
        else if (sl->owner > 0)
            play_clip(sl->owner, sl->start_ms + elapsed);
    }
}

void midp_jme_audio_init(JmeVM *vm, bool trace_audio)
{
    trace = trace_audio;
    resync = false;
    start_counter = 0;
    memset(slots, 0, sizeof slots);
    jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
    jme_register_state(vm, "audio", save, load, NULL);
}

void midp_jme_audio_shutdown(void)
{
    for (int s = 0; s < SYNTH_SLOTS; s++)
        pal_audio_stop(s);
    for (int h = 1; h <= MAX_CLIPS; h++)
        if (clips[h].used)
            close_clip(h);
    memset(slots, 0, sizeof slots);
}
