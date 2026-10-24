/*
 * jmevm_midp.h - MIDlets on jmevm as a library, for apps that show the
 * game themselves (NextJ2ME on Android). The app drives the emulated clock,
 * takes the frames and the mixed sound, and feeds keys and touches.
 *
 * One game at a time. Every function except the input and sound ones is
 * called from one thread (the app's emulation thread). j2me_key and
 * j2me_pointer may be called from any thread, and j2me_audio_read from the
 * app's sound thread.
 */
#ifndef JMEVM_MIDP_H
#define JMEVM_MIDP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct J2meConfig {
    const char *classlib;     /* classlib.jar */
    const char *jar;          /* the game */
    const char *jad_text;     /* contents of the .jad, or NULL */
    const char *midlet;       /* MIDlet class, or NULL for the manifest's first */
    const char *data_dir;     /* record stores; created if missing */
    int         screen_w, screen_h;
    uint32_t    heap_bytes;   /* 0: 16 MB, as the desktop runner */
    int64_t     epoch_ms;     /* System.currentTimeMillis() at start */
    /* System.out, System.err and VM messages; may be NULL. */
    void      (*console)(void *user, const char *text, int len);
    void       *console_user;
} J2meConfig;

/* Starts the game. False if it cannot (the reason went to the console). */
bool j2me_start(const J2meConfig *config);

/* Runs the game for ms emulated milliseconds. False once it has ended. */
bool j2me_run(uint32_t ms);

/* Emulated milliseconds since start. */
int64_t j2me_time_ms(void);

/* The newest frame (0xAARRGGBB pixels), or NULL before the first one.
 * serial changes whenever a frame is shown. Valid until the next j2me_run,
 * j2me_load_state or j2me_rewind_back. */
const uint32_t *j2me_frame(int *w, int *h, uint32_t *serial);

/* Input, delivered at the start of the next j2me_run. key is a MIDP key
 * code (KEY_NUM0..9, '*', '#', or the negative codes of the game keys and
 * soft keys); x and y are in screen pixels. */
void j2me_key(bool down, int key);
void j2me_pointer(int type, int x, int y);   /* 0 down, 1 up, 2 drag */

/* Sound: 16-bit stereo at J2ME_AUDIO_RATE. Copies up to max_frames frames
 * mixed so far into out and returns how many. */
#define J2ME_AUDIO_RATE 32768
int j2me_audio_read(int16_t *out, int max_frames);

/* Save states: the whole machine, record stores and sound included. */
bool j2me_save_state(const char *path);
bool j2me_load_state(const char *path);

/* Rewind: keep the current state in a history in memory; go back one
 * entry. False when there is nothing to go back to. Loading a state
 * empties the history. */
bool j2me_rewind_push(void);
bool j2me_rewind_back(void);

/* Stops the game and frees everything. */
void j2me_stop(void);

#endif
