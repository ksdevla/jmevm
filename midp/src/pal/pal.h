/*
 * pal.h - the platform layer: what the MIDP layer needs from the machine
 * it runs on. The desktop runner (platform/host) and the embedding library
 * (platform/embed) each implement it; the sound functions come from
 * src/audio/mixer.c on both.
 */
#ifndef MIDP_PAL_H
#define MIDP_PAL_H

#include <stdbool.h>
#include <stdint.h>

#include "../audio/synth.h"

/* Console output: System.out, System.err and messages of the VM. */
void pal_console_write(const char *s, int len);

/* ---- input ---- */

enum {
    EV_NONE, EV_KEY_DOWN, EV_KEY_UP, EV_POINTER_DOWN, EV_POINTER_UP, EV_POINTER_DRAG, EV_QUIT
};

typedef struct PalEvent {
    int type;
    int a, b;   /* the MIDP key code; or x and y in screen pixels */
} PalEvent;

/* The next input event due now, if any. The MIDP layer asks between steps
 * of the emulated clock; an event delivered at the same emulated time in
 * every run makes the runs identical. */
bool pal_poll_event(PalEvent *ev);

/* ---- screen ---- */

void pal_screen_size(int *w, int *h);
/* A new frame (opaque 0xFFRRGGBB pixels). */
void pal_present(const uint32_t *px, int w, int h);
/* The current frame again, not counted as a new one (after a state load). */
void pal_show(const uint32_t *px, int w, int h);
/* The labels of the soft-key commands (NULL for none), for platforms that
 * show them outside the screen. */
void pal_soft_labels(const char *left, const char *right);

/* ---- sound: SYNTH_SLOTS players; volumes 0-100. The data passed in must
 * stay valid until the slot is stopped. ---- */

bool pal_audio_available(void);
void pal_audio_song(int slot, const SongEvent *ev, uint32_t n, uint32_t start_ms, int volume);
void pal_audio_pcm(int slot, const int16_t *pcm, uint32_t samples, uint32_t rate, uint32_t start,
                   int volume);
void pal_audio_stop(int slot);
void pal_audio_volume(int slot, int volume);

/* ---- storage ---- */

/* The directory for record stores (no trailing '/'), created if needed;
 * NULL when there is none. */
const char *pal_data_dir(void);
/* Calls fn with the name of each file in pal_data_dir(). */
void pal_data_list(void (*fn)(const char *name, void *ctx), void *ctx);

/* Reports an error the program cannot continue after, and stops. */
#if defined(__GNUC__)
__attribute__((noreturn))
#endif
void pal_fatal(const char *msg);

#endif
