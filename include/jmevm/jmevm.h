/*
 * jmevm: a deterministic, snapshot-friendly Java ME (CLDC 1.1) virtual machine.
 *
 * This is the whole public embedding API. The VM never touches files, clocks
 * or threads of the host: class bytes come from a callback, time is the
 * emulated tick counter, and the host advances the machine with
 * jme_run_until(). Everything the VM holds is plain data, so it can be saved
 * and restored at any jme_run_until() boundary.
 */
#ifndef JMEVM_H
#define JMEVM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keep in sync with project(VERSION) in CMakeLists.txt. */
#define JME_VERSION_MAJOR 0
#define JME_VERSION_MINOR 1
#define JME_VERSION_PATCH 0
#define JME_VERSION_STRING "0.1.0"

typedef enum JmeStatus {
	JME_OK = 0,
	JME_E_NOMEM,         /* a host allocation or the heap arena ran out */
	JME_E_FORMAT,        /* malformed input (class file, snapshot) */
	JME_E_UNSUPPORTED,   /* well-formed but outside what the VM supports */
	JME_E_NOTFOUND,      /* a class source has no such class */
	JME_E_ARGUMENT,      /* invalid argument or option */
	JME_E_STATE,         /* call not valid in the VM's current state */
	JME_E_UNIMPLEMENTED  /* not written yet */
} JmeStatus;

/* Short constant name of a status, such as "JME_E_FORMAT". */
const char *jme_status_name(JmeStatus status);

/* "major.minor.patch" of the library. */
const char *jme_version(void);

typedef struct JmeOptions {
	/* Size of the heap arena in bytes. Every Java object, array and image
	 * pixel buffer lives in it. 1 MB to 1 GB. */
	uint32_t heap_bytes;
	/* Emulated CPU speed: interpreter work units per emulated millisecond. */
	uint32_t ticks_per_ms;
	/* System.currentTimeMillis() at tick 0. */
	int64_t epoch_ms;
	/* Testing aid: collect before every allocation, so a reference the VM
	 * forgot to update makes a test fail. Very slow. */
	int gc_stress;
} JmeOptions;

/* Fills in the defaults: 16 MB heap, 10000 ticks/ms, epoch 0, no stress. */
void jme_options_default(JmeOptions *options);

typedef struct JmeVM JmeVM;

/* Creates a VM. options may be NULL for the defaults. */
JmeStatus jme_create(const JmeOptions *options, JmeVM **out);
void jme_destroy(JmeVM *vm);

/*
 * Supplies the bytes of a class file. name is in internal form
 * ("java/lang/Object"). The bytes must stay valid and unchanged until the VM
 * is destroyed. Return JME_E_NOTFOUND when there is no such class.
 *
 * Classes are loaded in a deterministic order. A snapshot records which
 * classes were loaded and a hash of their bytes, and restoring it asks the
 * source for the same classes again.
 */
typedef JmeStatus (*JmeClassSource)(void *user, const char *name,
		const uint8_t **data, size_t *len);

void jme_set_class_source(JmeVM *vm, JmeClassSource source, void *user);

/* Receives what the program writes to System.out (stream 1) and System.err
 * (stream 2). Without a console the output is dropped. */
typedef void (*JmeConsoleFn)(void *user, int stream, const char *data, size_t len);

void jme_set_console(JmeVM *vm, JmeConsoleFn console, void *user);

/*
 * Starts the main thread at main_class.main(String[]). main_class may use
 * dots or slashes. Returns JME_E_NOTFOUND when the class or its main method
 * does not exist.
 */
JmeStatus jme_start(JmeVM *vm, const char *main_class, int argc, const char *const *argv);

/*
 * Runs until the tick counter reaches target_ticks or the program has
 * finished; jme_run_until(vm, UINT64_MAX) runs it to the end. If every
 * thread waits for something no thread can do (a deadlock, or waiting for
 * input the host has not sent), time passes up to target_ticks and the call
 * returns with jme_finished() still 0; with UINT64_MAX it returns at once.
 */
JmeStatus jme_run_until(JmeVM *vm, uint64_t target_ticks);

/*
 * 1 once the program has finished: every thread ended, System.exit() was
 * called, or main ended with an uncaught exception. *exit_code (if not
 * NULL) is then the System.exit() status, 1 after an uncaught exception,
 * or 0.
 */
int jme_finished(const JmeVM *vm, int *exit_code);

/* Emulated time so far, in ticks. */
uint64_t jme_ticks(const JmeVM *vm);

/* Receives bytes in order (snapshots, host state); return JME_OK to go on. */
typedef JmeStatus (*JmeWriteFn)(void *user, const void *data, size_t len);

/* ---- Host extensions ----
 *
 * A host (an emulator front end) adds its own natives, resources,
 * properties and state. Register everything before jme_start() or
 * jme_snapshot_load(); a loaded snapshot keeps the VM's registrations.
 */

/* A Java reference: an offset into the VM's heap, 0 is null. The collector
 * moves objects, so a JmeRef held in a C variable is only valid until the
 * next call that can allocate (jme_new_*), unless it is rooted with
 * jme_root(). Arguments read with jme_arg_ref() stay valid: read them again
 * after allocating. */
typedef uint32_t JmeRef;

/* One call of a host native. */
typedef struct JmeCall JmeCall;

/* What a host native returns. */
enum {
	JME_NATIVE_RETURN = 0,  /* returned normally (the result is set) */
	JME_NATIVE_THROW,       /* an exception is pending (see jme_throw) */
	JME_NATIVE_BLOCK        /* blocked (see jme_block): runs again later */
};

typedef int (*JmeNativeFn)(JmeCall *call);

typedef struct JmeNative {
	const char *cls;    /* internal form: "javax/microedition/lcdui/Graphics" */
	const char *name;
	const char *desc;   /* "(II)V", or NULL for any descriptor */
	JmeNativeFn fn;
} JmeNative;

/* Adds natives; the table must stay valid as long as the VM. Later tables
 * take precedence over earlier ones; built-in natives come last. */
JmeStatus jme_register_natives(JmeVM *vm, const JmeNative *natives, size_t count);

JmeVM *jme_call_vm(JmeCall *call);

/* Arguments by slot index: for instance methods slot 0 is `this`; long and
 * double take two slots. */
int32_t jme_arg_int(JmeCall *call, unsigned slot);
int64_t jme_arg_long(JmeCall *call, unsigned slot);
float jme_arg_float(JmeCall *call, unsigned slot);
double jme_arg_double(JmeCall *call, unsigned slot);
JmeRef jme_arg_ref(JmeCall *call, unsigned slot);

void jme_return_int(JmeCall *call, int32_t v);
void jme_return_long(JmeCall *call, int64_t v);
void jme_return_float(JmeCall *call, float v);
void jme_return_double(JmeCall *call, double v);
void jme_return_ref(JmeCall *call, JmeRef v);

/* Makes an exception of class cls (internal form) pending; returns
 * JME_NATIVE_THROW so a native can `return jme_throw(...)`. */
int jme_throw(JmeCall *call, const char *cls, const char *msg);

/*
 * Blocks the calling thread until jme_signal(vm, channel) or until
 * timeout_ms emulated milliseconds pass (timeout_ms < 0: no timeout), and
 * returns JME_NATIVE_BLOCK. The native then runs again from the start, with
 * jme_call_woken() returning 1.
 */
int jme_block(JmeCall *call, uint32_t channel, int64_t timeout_ms);
int jme_call_woken(JmeCall *call);

/* Wakes every thread blocked on channel. Call it from a native or between
 * jme_run_until() calls; input is deterministic when the host signals at the
 * same points. */
void jme_signal(JmeVM *vm, uint32_t channel);

/* Ends the program like System.exit(code). */
int jme_exit(JmeCall *call, int code);

/* System.currentTimeMillis() of the emulated clock. */
int64_t jme_time_ms(const JmeVM *vm);

/* ---- objects, from natives ---- */

/* A field of a class (loading it if needed), or 0 if there is none. Look
 * fields up once and keep the id; it is valid for this VM and for subclasses. */
typedef uint32_t JmeField;
JmeField jme_field(JmeVM *vm, const char *cls, const char *name, const char *desc);

int32_t jme_get_int(JmeVM *vm, JmeRef obj, JmeField f);
void jme_set_int(JmeVM *vm, JmeRef obj, JmeField f, int32_t v);
int64_t jme_get_long(JmeVM *vm, JmeRef obj, JmeField f);
void jme_set_long(JmeVM *vm, JmeRef obj, JmeField f, int64_t v);
JmeRef jme_get_ref(JmeVM *vm, JmeRef obj, JmeField f);
void jme_set_ref(JmeVM *vm, JmeRef obj, JmeField f, JmeRef v);

/* Arrays: the element count and a pointer to the elements, valid until the
 * next allocation. Elements are little-endian Java values: int32_t for int,
 * uint16_t for char, int8_t for byte and boolean, JmeRef for objects. */
uint32_t jme_array_length(JmeVM *vm, JmeRef array);
void *jme_array_data(JmeVM *vm, JmeRef array);

/* The characters of a String, valid until the next allocation. */
const uint16_t *jme_string_chars(JmeVM *vm, JmeRef s, uint32_t *length);
/* A UTF-8 copy of a String, to free(); NULL when out of memory. */
char *jme_string_utf8(JmeVM *vm, JmeRef s);

/* New objects; 0 with an exception pending (OutOfMemoryError and so on).
 * array_class is a descriptor: "[I", "[B", "[Ljava/lang/String;". */
JmeRef jme_new_array(JmeCall *call, const char *array_class, int32_t length);
JmeRef jme_new_string(JmeCall *call, const char *utf8);
JmeRef jme_new_string_chars(JmeCall *call, const uint16_t *chars, uint32_t length);

/* Keeps a C variable holding a JmeRef up to date across allocations. Roots
 * are a stack: jme_unroot removes the last n. */
void jme_root(JmeVM *vm, JmeRef *ref);
void jme_unroot(JmeVM *vm, unsigned n);

/* ---- resources, properties, state ---- */

/* Files of the application, for Class.getResourceAsStream (name without a
 * leading slash). Same contract as JmeClassSource. */
typedef JmeStatus (*JmeResourceSource)(void *user, const char *name, const uint8_t **data, size_t *len);
void jme_set_resource_source(JmeVM *vm, JmeResourceSource source, void *user);

/* A value System.getProperty(key) returns; the strings are copied. */
JmeStatus jme_set_property(JmeVM *vm, const char *key, const char *value);

/*
 * Host state saved in snapshots under a unique name. save writes the state
 * with write(write_user, ...); load receives the bytes save wrote, after the
 * VM state itself was validated. load must check the data before changing
 * anything: if it fails, jme_snapshot_load fails, but other states may
 * already have been loaded.
 */
typedef JmeStatus (*JmeStateSave)(void *user, JmeWriteFn write, void *write_user);
typedef JmeStatus (*JmeStateLoad)(void *user, const void *data, size_t len);
JmeStatus jme_register_state(JmeVM *vm, const char *name, JmeStateSave save, JmeStateLoad load, void *user);

/* ---- Snapshots ---- */

/*
 * Writes the whole machine state: heap, threads, monitors, class states,
 * clock and scheduler. Call it between jme_run_until() calls on a started
 * VM. Running on from the snapshot gives exactly what running on from here
 * would have given.
 */
JmeStatus jme_snapshot_save(JmeVM *vm, JmeWriteFn write, void *user);

/*
 * Replaces the machine state with a saved one. The VM needs a class source
 * giving the same class files (each is checked by a hash); it may be fresh
 * or already running. The snapshot's heap size, speed and epoch replace the
 * VM's options. On any error the VM is left as it was. Returns
 * JME_E_FORMAT for damaged or inconsistent data, JME_E_NOTFOUND when a class
 * is missing, JME_E_UNSUPPORTED for another snapshot format version.
 */
JmeStatus jme_snapshot_load(JmeVM *vm, const void *data, size_t len);

/* ---- Rewind ---- */

/*
 * A history of snapshots kept compressed in memory: whole snapshots every
 * keyframe_interval entries and deltas against them in between. When the
 * history exceeds budget_bytes, the oldest entries are dropped.
 */
typedef struct JmeRewind JmeRewind;

JmeStatus jme_rewind_create(size_t budget_bytes, uint32_t keyframe_interval, JmeRewind **out);
void jme_rewind_destroy(JmeRewind *rw);

/* Adds the VM's current state as the newest entry. */
JmeStatus jme_rewind_push(JmeRewind *rw, JmeVM *vm);

uint32_t jme_rewind_count(const JmeRewind *rw);

/* Compressed bytes held. */
size_t jme_rewind_size(const JmeRewind *rw);

/* Loads the entry `back` steps before the newest (0 = newest) into vm,
 * leaving the history unchanged. */
JmeStatus jme_rewind_load(JmeRewind *rw, JmeVM *vm, uint32_t back);

/* Keeps the oldest `keep` entries: after rewinding to an entry, drop the
 * ones after it with jme_rewind_truncate(rw, count - back). */
void jme_rewind_truncate(JmeRewind *rw, uint32_t keep);

#ifdef __cplusplus
}
#endif

#endif
