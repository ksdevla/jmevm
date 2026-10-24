/*
 * midp_jme.h - the MIDP natives (javax.microedition.lcdui, jmevm.midp.*) on
 * jmevm's host API. They reach the machine through the platform layer
 * (src/pal/pal.h) and keep their state (input queue, record stores, sound)
 * in jmevm snapshots.
 *
 * The rules of jmevm natives apply: objects move when anything is
 * allocated, so a native reads its arguments again after allocating and
 * roots any other reference it keeps.
 */
#ifndef JMEVM_MIDP_MIDP_JME_H
#define JMEVM_MIDP_MIDP_JME_H

#include <stdbool.h>

#include "jmevm/jmevm.h"

typedef struct MidpSetup {
    const char       *jad_text;      /* contents of the .jad, or NULL */
    JmeResourceSource resources;     /* files of the application jar */
    void             *resources_user;
    bool              trace_audio;
} MidpSetup;

/* Registers the natives, their state and the system properties on vm.
 * Call once, before jme_start(). */
bool midp_jme_init(JmeVM *vm, const MidpSetup *setup);

/* Moves platform input into the event queue. Call between jme_run_until()
 * calls. Returns false when the platform asked to quit. */
bool midp_jme_poll(JmeVM *vm);

/* Stops the sound and frees what the natives allocated. */
void midp_jme_shutdown(void);

/* ---- internal: one part per file ---- */
void midp_jme_events_init(JmeVM *vm);
void midp_jme_lcdui_init(JmeVM *vm);
bool midp_jme_storage_init(JmeVM *vm);
void midp_jme_storage_shutdown(void);
void midp_jme_audio_init(JmeVM *vm, bool trace);
void midp_jme_audio_shutdown(void);
void midp_jme_audio_resync(JmeVM *vm);
void midp_jme_screen_resync(void);
void midp_jme_app_init(JmeVM *vm, const MidpSetup *setup);

/* Helpers shared by the parts (midp_jme.c). */
int midp_npe(JmeCall *c);
int midp_throw(JmeCall *c, const char *cls, const char *msg);

/* Little-endian state writing and reading for snapshots. */
typedef struct StateOut {
    JmeWriteFn write;
    void      *user;
    JmeStatus  status;
} StateOut;
void state_put(StateOut *o, const void *data, size_t n);
void state_put_u32(StateOut *o, uint32_t v);

typedef struct StateIn {
    const uint8_t *p, *end;
    bool           bad;
} StateIn;
const uint8_t *state_get(StateIn *in, size_t n);
uint32_t       state_get_u32(StateIn *in);

#endif
