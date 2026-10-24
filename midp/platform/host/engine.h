/*
 * engine.h - the virtual machine behind the desktop runner and the embedding
 * library: jmevm with the MIDP natives of src/midp_jme (engine_jmevm.c).
 */
#ifndef JMEVM_MIDP_ENGINE_H
#define JMEVM_MIDP_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct EngineOptions {
    uint32_t heap_bytes;
    uint32_t ticks_per_ms;
    int64_t  epoch_ms;
    bool     gc_stress;
    bool     trace_classes;
    bool     trace_exceptions;
    bool     trace_audio;
} EngineOptions;

/* What to run: a MIDlet from a jar, or a class's main() from -cp. */
typedef struct EngineApp {
    const char *classlib;     /* class library jar */
    const char *cp;           /* -cp directory or jar, or NULL */
    const char *main_class;   /* run main(String[]) of this class, or NULL */
    const char *jar;          /* MIDlet jar, or NULL */
    const char *midlet;       /* MIDlet class, or NULL for the manifest's */
    const char *jad_text;     /* contents of the .jad, or NULL */
    int         argc;         /* arguments for main_class */
    char      **argv;
} EngineApp;

/* Starts the program; false (after printing why) if it cannot. */
bool engine_start(const EngineOptions *opt, const EngineApp *app);

/* Feeds pending input, then runs until the emulated clock reaches ticks.
 * Returns false once the program has ended. */
bool engine_run_until(uint64_t ticks);

/* System.exit() status, or 1 after an uncaught exception in main. */
int      engine_exit_code(void);
uint32_t engine_ticks_per_ms(void);
uint64_t engine_ticks(void);
int64_t  engine_time_ms(void);        /* emulated milliseconds since start */
double   engine_busy_percent(void);   /* share of ticks spent running code */

/* Shuts the VM down (between --runs). */
void engine_stop(void);
void engine_dump_threads(void);

/* Save states: the whole machine in a file. False when the engine has no
 * save states or the file cannot be written or loaded. */
bool engine_save_state(const char *path);
bool engine_load_state(const char *path);

/* Rewind: push the current state on a history kept in memory; go back to
 * the one before the newest (dropping the newest). False when there is
 * nothing to go back to or the engine has no rewind. Loading a state
 * empties the history. */
bool engine_rewind_push(void);
bool engine_rewind_back(void);

#endif
