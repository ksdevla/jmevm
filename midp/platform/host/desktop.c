/*
 * desktop.c - the platform layer on a desktop (Windows or POSIX): wall
 * clock, frames (kept, hashed into the frame log, saved as screenshots),
 * the record store directory, and the console.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "runner.h"
#include "engine.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

Runner runner;

static uint32_t *frame;
static int       frame_w, frame_h;

/* ---- clock ---- */

static int64_t monotonic_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart / freq.QuadPart * 1000 + now.QuadPart % freq.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

int64_t wall_ms(void)
{
    static int64_t start = -1;
    if (start < 0)
        start = monotonic_ms();
    return monotonic_ms() - start;
}

int64_t epoch_ms(void)
{
    struct timespec ts;
#ifdef _WIN32
    timespec_get(&ts, TIME_UTC);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void sleep_until(int64_t wall)
{
    int64_t wait = wall - wall_ms();
    if (wait <= 0)
        return;
    runner.idle_ms += wait;
#ifdef _WIN32
    Sleep((DWORD)wait);
#else
    struct timespec ts = {(time_t)(wait / 1000), (long)(wait % 1000) * 1000000L};
    nanosleep(&ts, NULL);
#endif
}

/* ---- screen ---- */

void pal_screen_size(int *w, int *h)
{
    *w = runner.screen_w;
    *h = runner.screen_h;
}

void pal_show(const uint32_t *px, int w, int h)
{
    if (w != frame_w || h != frame_h) {
        uint32_t *grown = realloc(frame, sizeof(uint32_t) * (size_t)w * h);
        if (!grown)
            return;
        frame = grown;
        frame_w = w;
        frame_h = h;
    }
    memcpy(frame, px, sizeof(uint32_t) * (size_t)w * h);
}

void pal_present(const uint32_t *px, int w, int h)
{
    pal_show(px, w, h);
    runner.frames_presented++;
    if (runner.frame_log) {
        uint32_t hash = 2166136261u;   /* FNV-1a over the red, green and blue bytes */
        for (int i = 0; i < w * h; i++) {
            hash = (hash ^ ((px[i] >> 16) & 0xFF)) * 16777619u;
            hash = (hash ^ ((px[i] >> 8) & 0xFF)) * 16777619u;
            hash = (hash ^ (px[i] & 0xFF)) * 16777619u;
        }
        fprintf(runner.frame_log, "%lld %08x\n", (long long)engine_time_ms(), hash);
    }
}

void pal_soft_labels(const char *left, const char *right)
{
    (void)left;
    (void)right;
}

const uint32_t *last_frame(int *w, int *h)
{
    *w = frame_w;
    *h = frame_h;
    return frame;
}

bool write_screenshot(const char *path)
{
    if (!frame)
        return false;
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    fprintf(f, "P6\n%d %d\n255\n", frame_w, frame_h);
    for (int i = 0; i < frame_w * frame_h; i++) {
        uint8_t rgb[3] = {(uint8_t)(frame[i] >> 16), (uint8_t)(frame[i] >> 8), (uint8_t)frame[i]};
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
}

/* ---- storage, console ---- */

const char *pal_data_dir(void)
{
#ifdef _WIN32
    _mkdir(runner.data_dir);
#else
    mkdir(runner.data_dir, 0755);
#endif
    return runner.data_dir;
}

void pal_data_list(void (*fn)(const char *name, void *ctx), void *ctx)
{
#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof pattern, "%s\\*", runner.data_dir);
    WIN32_FIND_DATAA found;
    HANDLE           h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            fn(found.cFileName, ctx);
    } while (FindNextFileA(h, &found));
    FindClose(h);
#else
    DIR *d = opendir(runner.data_dir);
    if (!d)
        return;
    for (struct dirent *e; (e = readdir(d));)
        if (e->d_name[0] != '.')
            fn(e->d_name, ctx);
    closedir(d);
#endif
}

void pal_console_write(const char *s, int len)
{
    fwrite(s, 1, (size_t)len, stdout);
    fflush(stdout);
}

void pal_fatal(const char *msg)
{
    fprintf(stderr, "jmevm-midp: %s\n", msg);
    exit(1);
}
