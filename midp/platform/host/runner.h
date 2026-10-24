/*
 * runner.h - the desktop runner (jmevm-midp): the parts it is made of and
 * what they share. The program runs in frames of 1/60 s of emulated time;
 * input enters only between frames.
 */
#ifndef MIDP_RUNNER_H
#define MIDP_RUNNER_H

#include "../../src/pal/pal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Runner {
    int         screen_w, screen_h;
    const char *data_dir;
    FILE       *frame_log;          /* --frame-log: "ms hash" per new frame */
    int         frames_presented;
    int64_t     idle_ms;            /* real time spent waiting (--realtime) */
    /* Set by the window's F5, F9 and held Backspace; handled between frames. */
    bool        save_request, load_request, rewinding;
} Runner;

extern Runner runner;

/* ---- desktop.c: clock, screen, storage, console ---- */

int64_t         wall_ms(void);     /* real milliseconds since the first call */
int64_t         epoch_ms(void);    /* real time: milliseconds since 1970 */
void            sleep_until(int64_t wall);
const uint32_t *last_frame(int *w, int *h);   /* NULL before the first */
bool            write_screenshot(const char *path);   /* PPM */

/* ---- input.c: one queue for the window, --keys, and --replay ---- */

/* Adds a --keys script: ms:key[:up] and ms:@x:y[:drag|:up], comma
 * separated. False if it is malformed. */
bool input_script(const char *spec);
void input_push(const PalEvent *ev);
/* Before frame f runs: scripted input due by now, replayed input of f. */
void input_begin_frame(uint64_t frame);
void input_reset(void);
/* After a state load: frames up to `frame` are done and the script was
 * delivered up to until_ms. */
void input_seek(uint64_t frame, int64_t until_ms);
bool input_record(const char *path);
bool input_replay(const char *path);
void input_close(void);

/* ---- sound.c: the mix to a WAV file and to the sound device ---- */

bool sound_record(const char *path);
bool sound_play(void);
void sound_update(void);   /* makes the sound up to the emulated now */
void sound_close(void);

/* sound_win32.c, sound_none.c: the sound device */
bool sound_device_open(uint32_t rate);
void sound_device_write(const int16_t frame[2]);
void sound_device_close(void);

/* ---- window_win32.c, window_none.c ---- */

/* A window showing the screen scaled by `scale`, its keys turned into MIDP
 * key events. */
bool window_open(int w, int h, int scale);
bool window_pump(void);    /* handles messages; false once it was closed */
void window_show(void);    /* draws the latest frame */

#endif
