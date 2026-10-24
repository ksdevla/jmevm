/*
 * embed.c - the library interface of jmevm_midp.h: the platform layer
 * (src/pal/pal.h) for an app that embeds the MIDP layer, on the jmevm engine
 * (platform/host/engine_jmevm.c) and the software mixer.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "jmevm_midp.h"
#include "../host/engine.h"
#include "../../src/audio/mixer.h"
#include "../../src/pal/pal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
typedef SRWLOCK Lock;
#define LOCK_INIT SRWLOCK_INIT
#define lock(l)   AcquireSRWLockExclusive(l)
#define unlock(l) ReleaseSRWLockExclusive(l)
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
typedef pthread_mutex_t Lock;
#define LOCK_INIT PTHREAD_MUTEX_INITIALIZER
#define lock(l)   pthread_mutex_lock(l)
#define unlock(l) pthread_mutex_unlock(l)
#endif

_Static_assert(J2ME_AUDIO_RATE == MIXER_RATE, "jmevm_midp.h and the mixer disagree");

#define QUEUE_SIZE  256
/* Half a second of sound; when the app does not read it in time the
 * oldest is dropped. */
#define AUDIO_SIZE  (J2ME_AUDIO_RATE / 2)

static J2meConfig config;
static char      *data_dir;
static bool       started;
static uint64_t   target;          /* emulated clock the game runs to */

static uint32_t  *frame;
static int        frame_w, frame_h;
static uint32_t   frame_serial;

static Lock       input_lock = LOCK_INIT;
static PalEvent    queue[QUEUE_SIZE];
static int        head, tail;

/* A tap on a touch screen can press and release a key within one step, and
 * games that look at their keys once a frame would miss it: a release waits
 * until its key has been down this long. */
#define MIN_PRESS_MS 50
#define KEY_MIN      (-16)
#define KEY_MAX      127
static int64_t    down_at[KEY_MAX - KEY_MIN + 1];   /* -1: not down */

static Lock       audio_lock = LOCK_INIT;
static int16_t    audio[AUDIO_SIZE][2];
static int        audio_head, audio_count;

static void push_event(int type, int a, int b)
{
    lock(&input_lock);
    int next = (tail + 1) % QUEUE_SIZE;
    if (next != head) {   /* full: drop, as a phone's key buffer would */
        queue[tail].type = type;
        queue[tail].a = a;
        queue[tail].b = b;
        tail = next;
    }
    unlock(&input_lock);
}

static void audio_out(const int16_t f[2], void *user)
{
    (void)user;
    lock(&audio_lock);
    int at = (audio_head + audio_count) % AUDIO_SIZE;
    audio[at][0] = f[0];
    audio[at][1] = f[1];
    if (audio_count < AUDIO_SIZE)
        audio_count++;
    else
        audio_head = (audio_head + 1) % AUDIO_SIZE;
    unlock(&audio_lock);
}

/* After the clock jumped (a state was loaded): the sound continues from the
 * new time; what was mixed but not played yet is dropped. */
static void clock_jumped(void)
{
    for (int i = 0; i <= KEY_MAX - KEY_MIN; i++)
        down_at[i] = -1;
    target = engine_ticks();
    mixer_reset(engine_time_ms());
    lock(&audio_lock);
    audio_head = audio_count = 0;
    unlock(&audio_lock);
}

bool j2me_start(const J2meConfig *c)
{
    if (started)
        j2me_stop();
    config = *c;
    data_dir = malloc(strlen(c->data_dir) + 1);
    if (!data_dir)
        return false;
    strcpy(data_dir, c->data_dir);
    head = tail = 0;

    EngineOptions opt = {0};
    opt.heap_bytes = c->heap_bytes ? c->heap_bytes : 16u << 20;   /* as the runner */
    opt.ticks_per_ms = 10000;
    opt.epoch_ms = c->epoch_ms;
    EngineApp app = {0};
    app.classlib = c->classlib;
    app.jar = c->jar;
    app.midlet = c->midlet;
    app.jad_text = c->jad_text;
    if (!engine_start(&opt, &app)) {
        engine_stop();
        free(data_dir);
        data_dir = NULL;
        return false;
    }
    started = true;
    clock_jumped();
    return true;
}

bool j2me_run(uint32_t ms)
{
    if (!started)
        return false;
    target += (uint64_t)ms * engine_ticks_per_ms();
    bool running = engine_run_until(target);
    int64_t now = engine_time_ms();
    mixer_advance(now, audio_out, NULL);
    return running;
}

int64_t j2me_time_ms(void)
{
    return started ? engine_time_ms() : 0;
}

const uint32_t *j2me_frame(int *w, int *h, uint32_t *serial)
{
    *w = frame_w;
    *h = frame_h;
    *serial = frame_serial;
    return frame;
}

void j2me_key(bool down, int key)
{
    push_event(down ? EV_KEY_DOWN : EV_KEY_UP, key, 0);
}

void j2me_pointer(int type, int x, int y)
{
    static const int types[3] = {EV_POINTER_DOWN, EV_POINTER_UP, EV_POINTER_DRAG};
    if (type >= 0 && type < 3)
        push_event(types[type], x, y);
}

int j2me_audio_read(int16_t *out, int max_frames)
{
    lock(&audio_lock);
    int n = audio_count < max_frames ? audio_count : max_frames;
    for (int i = 0; i < n; i++) {
        out[i * 2] = audio[audio_head][0];
        out[i * 2 + 1] = audio[audio_head][1];
        audio_head = (audio_head + 1) % AUDIO_SIZE;
    }
    audio_count -= n;
    unlock(&audio_lock);
    return n;
}

bool j2me_save_state(const char *path)
{
    return started && engine_save_state(path);
}

bool j2me_load_state(const char *path)
{
    if (!started || !engine_load_state(path))
        return false;
    clock_jumped();
    return true;
}

bool j2me_rewind_push(void)
{
    return started && engine_rewind_push();
}

bool j2me_rewind_back(void)
{
    if (!started || !engine_rewind_back())
        return false;
    clock_jumped();
    return true;
}

void j2me_stop(void)
{
    if (started)
        engine_stop();
    started = false;
    free(frame);
    frame = NULL;
    frame_w = frame_h = 0;
    free(data_dir);
    data_dir = NULL;
}

/* ------------------------------------------------------------------------ */
/* pal.h                                                                    */

void pal_console_write(const char *s, int len)
{
    if (config.console)
        config.console(config.console_user, s, len);
}

bool pal_poll_event(PalEvent *ev)
{
    lock(&input_lock);
    bool any = head != tail;
    if (any) {
        PalEvent e = queue[head];
        int     k = e.a - KEY_MIN;
        bool    tracked = (e.type == EV_KEY_DOWN || e.type == EV_KEY_UP) && k >= 0 &&
                          k <= KEY_MAX - KEY_MIN;
        int64_t now = engine_time_ms();
        if (tracked && e.type == EV_KEY_UP && down_at[k] >= 0 &&
            now - down_at[k] < MIN_PRESS_MS) {
            any = false;   /* later events wait too, to keep the order */
        } else {
            if (tracked)
                down_at[k] = e.type == EV_KEY_DOWN ? now : -1;
            *ev = e;
            head = (head + 1) % QUEUE_SIZE;
        }
    }
    unlock(&input_lock);
    return any;
}

void pal_screen_size(int *w, int *h)
{
    *w = config.screen_w;
    *h = config.screen_h;
}

void pal_show(const uint32_t *px, int w, int h)
{
    if (w != frame_w || h != frame_h) {
        uint32_t *f = malloc(sizeof(uint32_t) * (size_t)w * h);
        if (!f)
            return;
        free(frame);
        frame = f;
        frame_w = w;
        frame_h = h;
    }
    memcpy(frame, px, sizeof(uint32_t) * (size_t)w * h);
    frame_serial++;
}

void pal_present(const uint32_t *px, int w, int h)
{
    pal_show(px, w, h);
}

void pal_soft_labels(const char *left, const char *right)
{
    (void)left;
    (void)right;
}

const char *pal_data_dir(void)
{
    if (!data_dir)
        return NULL;
#ifdef _WIN32
    _mkdir(data_dir);
#else
    mkdir(data_dir, 0755);
#endif
    return data_dir;
}

void pal_data_list(void (*fn)(const char *name, void *ctx), void *ctx)
{
    if (!data_dir)
        return;
#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof pattern, "%s\\*", data_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            fn(fd.cFileName, ctx);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(data_dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)))
        if (e->d_name[0] != '.')
            fn(e->d_name, ctx);
    closedir(d);
#endif
}

void pal_fatal(const char *msg)
{
    pal_console_write(msg, (int)strlen(msg));
    abort();
}
