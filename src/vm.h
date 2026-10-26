/*
 * Internal VM structures. Nothing here may hold a machine pointer that a
 * snapshot would need: the heap, stacks, statics and monitors refer to each
 * other by offsets and ids.
 *
 * Memory model:
 *   - Java objects live in one arena (vm->heap). A Ref is a byte offset into
 *     it; 0 is null. The collector compacts, so a Ref or Object pointer held
 *     in a C variable is only valid until the next allocation, unless the
 *     variable is registered with ROOT().
 *   - Thread stacks are C arrays of Slot with a parallel tag array that says
 *     which slots hold references, so the collector is precise.
 *   - Classes, methods and fields are C metadata outside the arena.
 */
#ifndef JMEVM_VM_H
#define JMEVM_VM_H

#include <string.h>

#include "base.h"
#include "classfile.h"
#include "pool.h"

typedef u32 Ref;
typedef u32 Slot;

typedef struct Class Class;
typedef struct Thread Thread;
typedef struct JmeVM VM;

/* ---- Heap objects ---- */

typedef struct Object {
	u32 cls;      /* class id */
	u32 hash;     /* bit 31: GC mark; bits 0-30: identity hash, 0 until asked for */
	u32 monitor;  /* monitor id, 0 = none */
	u32 forward;  /* new location during a collection, 0 otherwise */
} Object;

#define OBJ_HEADER 16u    /* instance fields start here */
#define ARRAY_LENGTH 16u  /* u32 length of an array */
#define ARRAY_DATA 24u    /* array elements start here */
#define MARK_BIT 0x80000000u
#define HEAP_START 16u    /* first object; offsets below are never valid */

#define OBJ(vm, r) ((Object *)((vm)->heap + (r)))
#define FIELD_PTR(vm, r, off) ((void *)((vm)->heap + (r) + (off)))
#define ARRAY_LEN(vm, r) (*(u32 *)((vm)->heap + (r) + ARRAY_LENGTH))
#define ARRAY_PTR(vm, r) ((void *)((vm)->heap + (r) + ARRAY_DATA))
#define REF_ELEMS(vm, r) ((Ref *)ARRAY_PTR(vm, r))

/* newarray type codes (JVMS), also used for array element types */
enum {
	T_BOOLEAN = 4, T_CHAR = 5, T_FLOAT = 6, T_DOUBLE = 7,
	T_BYTE = 8, T_SHORT = 9, T_INT = 10, T_LONG = 11, T_OBJECT = 12
};

/* ---- Natives ---- */

typedef union Value {
	i32 i;
	i64 j;
	float f;
	double d;
	Ref r;
} Value;

typedef enum NativeResult {
	N_OK,         /* returned normally; *ret holds the result */
	N_THROW,      /* t->exception is set */
	N_REEXECUTE,  /* pushed frames (class init) or blocked the thread: run the
	               * invoke again when the thread continues */
	N_YIELD       /* returned normally; stop running this thread for now */
} NativeResult;

/* args points at the arguments on the caller's operand stack (args[0] is
 * this for instance methods). Reference arguments there are kept up to date
 * by the collector, so read them again after allocating. */
typedef NativeResult (*NativeFn)(VM *vm, Thread *t, Slot *args, Value *ret);

typedef struct NativeEntry {
	const char *cls;
	const char *name;
	const char *desc;
	NativeFn fn;
} NativeEntry;

/* ---- Classes ---- */

typedef enum ClassState {
	CS_LINKED,
	CS_INITIALIZING,
	CS_INITIALIZED,
	CS_ERROR
} ClassState;

typedef enum ClassKind {
	CK_INSTANCE,
	CK_ARRAY
} ClassKind;

typedef struct Field {
	Class *owner;
	const char *name;   /* interned */
	const char *desc;   /* interned */
	u16 access;
	u8 type;            /* first descriptor char: B C D F I J S Z L [ */
	u32 offset;         /* instance: byte offset; static: index in owner->statics */
	u16 const_index;    /* ConstantValue, or 0 */
} Field;

typedef struct Method {
	u32 id;
	Class *owner;
	const char *name;   /* interned */
	const char *desc;   /* interned */
	u16 access;
	u16 arg_slots;      /* including this */
	u8 ret_type;        /* V I J F D L (Z B C S return as I) */
	u8 prepared;        /* code checked by method_prepare */
	i32 vtable_index;   /* -1: not dispatched through the vtable */
	const CodeAttr *code;
	NativeFn native;
	JmeNativeFn host_native; /* registered by the host (host.c) */
} Method;

typedef union Resolved {
	Class *cls;
	Field *field;
	Method *method;
	Ref str;            /* a GC root */
} Resolved;

struct Class {
	u32 id;
	const char *name;          /* interned, internal form */
	ClassKind kind;
	ClassState state;
	u16 access;
	u32 init_thread;           /* thread running <clinit> */
	ClassFile *cf;             /* NULL for arrays */
	u64 bytes_hash;            /* FNV-1a of the class file; snapshots check it */
	Class *super;
	u16 interface_count;
	Class **interfaces;        /* direct superinterfaces */
	u32 all_interface_count;
	Class **all_interfaces;    /* every interface this class implements */

	u32 instance_size;         /* bytes including the header, multiple of 8 */
	u32 ref_count;
	u32 *ref_offsets;          /* reference fields, inherited ones included */
	u8 is_reference;           /* java/lang/ref/Reference or a subclass */

	Class *component;          /* arrays: element class, NULL for primitives */
	u8 elem_type;              /* arrays: T_* */
	u8 elem_size;              /* arrays: bytes per element */

	u16 field_count;
	Field *fields;
	u16 method_count;
	Method *methods;
	u32 vtable_len;
	Method **vtable;

	u32 static_count;
	Slot *statics;
	u8 *static_tags;           /* 1 for reference slots */

	Resolved *resolved;        /* per constant pool index */
	u8 *resolved_flag;

	Ref mirror;                /* java.lang.Class instance, 0 until needed */
	Class *array_class;        /* [this, once loaded */
	Pool pool;
};

/* ---- Threads ---- */

/*
 * A thread that cannot continue leaves its current instruction to run again:
 * monitorenter, a synchronized call, sleep, wait, join, or a class another
 * thread is initializing. Whatever wakes it makes it runnable again.
 */
typedef enum ThreadState {
	TS_RUNNABLE,
	TS_SLEEPING,    /* Thread.sleep until wake_tick */
	TS_WAITING,     /* in the wait set of blocked_on; until wake_tick if timed */
	TS_BLOCKED,     /* in the entry queue of blocked_on's monitor */
	TS_JOINING,     /* until thread join_target ends */
	TS_INIT_WAIT,   /* until another thread finishes initializing init_wait */
	TS_HOST_WAIT,   /* a host native blocked it until jme_signal(wait_channel) */
	TS_DONE
} ThreadState;

enum {
	F_ADVANCE = 1,    /* on return, move the caller past its invoke */
	F_CLINIT = 2      /* runs <clinit> of frame->init_class */
};

typedef struct Frame {
	Method *method;
	u32 pc;            /* current instruction */
	u32 locals;        /* index of local 0 in the thread's slots */
	u32 sp;            /* operand stack top, saved while the frame is not running */
	u32 flags;
	Class *init_class;
	Ref lock;          /* monitor held by a synchronized method (a GC root) */
} Frame;

#define THREAD_SLOTS (64u * 1024u)
#define THREAD_FRAMES 4096u

struct Thread {
	u32 id;
	ThreadState state;
	Ref jthread;          /* java.lang.Thread (a GC root) */
	Slot *slots;
	u8 *tags;             /* 1 for reference slots */
	u32 sp;               /* top of the live slots, synced before anything can collect */
	Frame *frames;
	u32 frame_count;
	Ref exception;        /* pending exception (a GC root) */
	u8 interrupted;
	/* A native asked for a Java call after it returns (Class.newInstance). */
	Method *invoke_after;
	Ref invoke_arg;       /* a GC root */

	/* blocking */
	u64 wake_tick;
	u8 timed;             /* wake_tick applies (sleep, wait with a timeout) */
	u8 block_phase;       /* sleep and wait: 0 first call, 1 blocked, 2 woken */
	u8 granted;           /* a monitor was handed over while blocked */
	Ref blocked_on;       /* object whose monitor it waits for (a GC root) */
	u32 enter_count;      /* monitor count to restore when granted */
	u32 queue_next;       /* next thread id in a monitor queue, 0 = last */
	u32 join_target;      /* thread id */
	Class *init_wait;
	u32 wait_channel;     /* TS_HOST_WAIT */
};

/* ---- Monitors ---- */

/* Queues are FIFO lists of thread ids linked through Thread.queue_next. */
typedef struct Monitor {
	u32 owner;   /* thread id, 0 = free; also links free monitors */
	u32 count;
	u32 entry_head, entry_tail;  /* threads waiting to enter */
	u32 wait_head, wait_tail;    /* threads in wait() */
} Monitor;

/* ---- Host extensions (host.c) ---- */

typedef struct HostNatives {
	const JmeNative *table;
	size_t count;
} HostNatives;

typedef struct HostState {
	char *name;
	JmeStateSave save;
	JmeStateLoad load;
	void *user;
} HostState;

typedef struct HostProperty {
	char *key, *value;
} HostProperty;

/* What the host registered; jme_snapshot_load() hands it to the new VM. */
typedef struct HostSetup {
	HostNatives *natives;
	u32 native_count;
	HostState *states;
	u32 state_count;
	HostProperty *props;
	u32 prop_count;
	JmeResourceSource resources;
	void *resources_user;
} HostSetup;

/* ---- Symbols and class map ---- */

typedef struct SymTable {
	const char **slots;
	u32 cap, count;
} SymTable;

typedef struct ClassMap {
	Class **slots;
	u32 cap, count;
} ClassMap;

/* ---- The VM ---- */

struct JmeVM {
	JmeOptions options;
	JmeClassSource class_source;
	void *class_source_user;
	JmeConsoleFn console;
	void *console_user;
	HostSetup host;
	u64 ticks;

	/* heap */
	u8 *heap;
	u32 heap_size;
	u32 heap_top;
	u8 *cards;                 /* one per CARD bytes of heap: written since the history looked */
	u32 hash_state;
	u32 gc_count;
	Ref **c_roots;
	u32 c_root_count, c_root_cap;

	/* classes and methods, by id */
	Class **classes;
	u32 class_count, class_cap;
	Method **methods;
	u32 method_count, method_cap;
	ClassMap class_map;
	SymTable syms;
	const char **loading;      /* names being loaded, to catch circularity */
	u32 loading_count, loading_cap;

	/* well-known classes and fields */
	Class *c_object, *c_string, *c_class, *c_thread, *c_throwable, *c_error;
	Class *c_char_array;
	u32 f_string_value, f_string_offset, f_string_count;
	u32 f_class_id;
	u32 f_throwable_message, f_throwable_trace;
	u32 f_thread_name, f_thread_priority;
	u32 referent_offset;
	Ref oom_error;             /* preallocated OutOfMemoryError (a GC root) */
	struct {
		const char *init, *clinit, *main, *main_desc, *void_desc;
	} sym;

	/* interned strings: open addressing on string content, 0 = empty */
	Ref *interned;
	u32 interned_cap, interned_count;

	Monitor *monitors;
	u32 monitor_count, monitor_cap, monitor_free;

	Thread **threads;
	u32 thread_count;
	Thread *current;

	/* Scheduler. The current turn is part of the machine state, so where
	 * jme_run_until() stops does not change what runs next. */
	u32 sched_next;            /* index where the round-robin search starts */
	u32 turn;                  /* index + 1 of the thread whose turn it is, 0 = none */
	u64 turn_end;              /* tick at which that turn ends */

	/* Bootstrap methods, allocated in boot_class's pool so their addresses
	 * survive jme_snapshot_load() swapping VM contents. */
	Class *boot_class;         /* holds the bootstrap methods' constants */
	Method *boot_method;       /* main(args) */
	Method *run_method;        /* thread.run() for started threads */
	Method *main_method;

	int started, finished, exit_code;
	Pool pool;                 /* lives as long as the VM */
};

/* ---- C roots ---- */

/* Registers a C variable holding a Ref; the collector updates it. */
void vm_root(VM *vm, Ref *ref);
void vm_unroot(VM *vm, u32 n);
#define ROOT(vm, var) vm_root((vm), &(var))
#define UNROOT(vm, n) vm_unroot((vm), (n))

/* ---- sym.c ---- */
const char *sym_intern(VM *vm, const char *s, size_t len);
const char *sym_cstr(VM *vm, const char *s);
const char *sym_utf8(VM *vm, Utf8 s);

/* ---- heap.c ---- */
JmeStatus heap_init(VM *vm);

/* The card table: every write to the heap marks the card (CARD bytes) it
 * falls in, so the history (history.c) compares only those. A write that
 * misses its mark makes rewinding wrong; debug builds check for that. */
#define CARD_SHIFT 9u
#define CARD (1u << CARD_SHIFT)
#define CARD_MARK(vm, off) ((vm)->cards[(u32)(off) >> CARD_SHIFT] = 1)
/* Marks the cards of heap bytes [from, to). */
void heap_mark(VM *vm, u32 from, u32 to);
void heap_free(VM *vm);
/* Zeroed object of size bytes with its header set, or 0 when the heap is full
 * even after a collection. Throws nothing. */
Ref heap_alloc(VM *vm, u32 cls_id, u32 size);
void heap_collect(VM *vm);
u32 heap_used(const VM *vm);
u32 object_size(VM *vm, Ref r);
i32 identity_hash(VM *vm, Ref r);
Class *class_of(VM *vm, Ref r);

/* These throw OutOfMemoryError or NegativeArraySizeException and return 0. */
Ref new_object(VM *vm, Class *c);
Ref new_array(VM *vm, Class *array_class, i32 length);

/* ---- class.c ---- */
JmeStatus class_load(VM *vm, const char *name, Class **out);
/* Loads or throws NoClassDefFoundError/ClassFormatError; NULL on failure. */
Class *class_load_or_throw(VM *vm, const char *name);
Class *array_class_of(VM *vm, Class *component);
Class *primitive_array_class(VM *vm, u8 elem_type);
int class_is_assignable(Class *from, Class *to);
int class_is_subclass(Class *c, Class *super);
Field *class_find_field(Class *c, const char *name, const char *desc);
Method *class_find_method(Class *c, const char *name, const char *desc);
Method *class_find_own_method(Class *c, const char *name, const char *desc);
Method *class_find_virtual(Class *c, const char *name, const char *desc);
u32 field_offset(VM *vm, Class *c, const char *name);
Ref class_mirror(VM *vm, Class *c);
Class *mirror_class(VM *vm, Ref mirror);
void class_free_all(VM *vm);
/* "java.lang.String" style name of a class, into buf. */
void class_java_name(Class *c, char *buf, size_t cap);
Class *resolve_class(VM *vm, Class *cur, u16 index);
Field *resolve_field(VM *vm, Class *cur, u16 index, int is_static);
Method *resolve_method(VM *vm, Class *cur, u16 index, int interface_call);
Ref resolve_string(VM *vm, Class *cur, u16 index);

enum { INIT_DONE, INIT_PUSHED, INIT_THROWN, INIT_BLOCKED };
/* Makes sure c is initialized, pushing <clinit> frames as needed. Blocks t
 * while another thread is initializing c. */
int class_init(VM *vm, Thread *t, Class *c);
/* c is initialized or failed: wakes the threads waiting for it. */
void class_init_finished(VM *vm, Class *c);

/* ---- string.c ---- */
Ref string_from_utf8(VM *vm, const u8 *s, u32 len);
Ref string_from_cstr(VM *vm, const char *s);
Ref string_from_chars(VM *vm, const u16 *chars, u32 len);
Ref string_intern(VM *vm, Ref s);
/* The characters of s; valid until the next allocation. */
const u16 *string_chars(VM *vm, Ref s, u32 *len);
/* UTF-8 copy of s in a malloc'd NUL-terminated buffer, or NULL. */
char *string_to_utf8(VM *vm, Ref s);
void strings_free(VM *vm);

/* ---- monitor.c ---- */
/* 1 when t holds the monitor; 0 when t is now blocked in its entry queue. */
int monitor_enter(VM *vm, Thread *t, Ref obj);
/* 0 when t does not own the monitor. */
int monitor_exit(VM *vm, Thread *t, Ref obj);
int monitor_owned(VM *vm, Thread *t, Ref obj);
/* Object.wait: t owns obj's monitor; releases it fully and waits. */
void monitor_wait(VM *vm, Thread *t, Ref obj, i64 ms);
/* Object.wait after waking: 1 when t holds the monitor again. */
int monitor_reacquired(VM *vm, Thread *t);
/* Object.notify/notifyAll: t owns obj's monitor. */
void monitor_notify(VM *vm, Ref obj, int all);
/* Ends a wait early (timeout or interrupt). */
void monitor_wake_waiter(VM *vm, Thread *w);
void monitor_release_dead(VM *vm, u32 id);
void monitors_free(VM *vm);

/* ---- verify.c ---- */
/* Checks m's code once; throws VerifyError and returns 0 if it is malformed. */
int method_prepare(VM *vm, Method *m);
/* Without throwing: 1 when m's code is well formed and pc starts an
 * instruction (checks frames of a loaded snapshot). */
int method_check_pc(VM *vm, Method *m, u32 pc);

/* ---- interp.c ---- */
/* Runs t until vm->ticks reaches limit, it blocks, or it ends. */
void interp_run(VM *vm, Thread *t, u64 limit);
/* Pushes a frame for m whose arguments are the top slots of t's stack.
 * Returns 0 with an exception pending. A synchronized method whose monitor
 * is taken pushes nothing and blocks t (it returns 1). */
int frame_push(VM *vm, Thread *t, Method *m, u32 flags);

/* ---- exceptions (interp.c) ---- */
/* Creates an exception without running Java code and makes it pending. */
void throw_new(VM *vm, const char *cls, const char *msg);
void throw_npe(VM *vm);
/* Captures the current stack into throwable ex. */
void fill_stack_trace(VM *vm, Thread *t, Ref ex, int skip_init);
/* Prints "Exception: message" and the stack trace on stream 2. */
void print_stack_trace(VM *vm, Ref ex, const char *prefix);

/* ---- natives.c ---- */
NativeFn natives_find(Method *m);

/* ---- host.c ---- */
/* Finds a host native for m (NULL if none). */
JmeNativeFn host_native_find(VM *vm, Method *m);
/* Runs a host native; same contract as a NativeFn. */
NativeResult host_native_call(VM *vm, Thread *t, Method *m, Slot *args, Value *ret);
/* Copies the host's registrations into a new VM (for snapshot loads). */
JmeStatus host_setup_copy(VM *to, const VM *from);
void host_setup_free(VM *vm);
const char *host_property(VM *vm, const char *key);

/* ---- vm.c ---- */
/* Finds the well-known classes and fields among the loaded classes. */
JmeStatus vm_bind_core(VM *vm);
/* Creates the bootstrap methods; main is the program's main method. */
JmeStatus vm_make_boot(VM *vm, Method *main);
void vm_console(VM *vm, int stream, const char *data, size_t len);
void vm_printf(VM *vm, int stream, const char *fmt, ...);
Thread *thread_new(VM *vm);
Thread *thread_by_id(VM *vm, u32 id);
/* The running thread for a java.lang.Thread, or NULL if it is not alive. */
Thread *thread_of(VM *vm, Ref jthread);
/* t has ended: wakes its joiners and frees its stack. */
void thread_end(VM *vm, Thread *t);
/* Starts a thread running jthread.run(). */
int thread_start(VM *vm, Ref jthread);

/* ---- fpconv.c ---- */
/* Java's Double.toString / Float.toString, NUL-terminated, into buf[32]. */
void double_to_java_string(double d, char *buf);
void float_to_java_string(float f, char *buf);
/* Java's Double.parseDouble / Float.parseFloat; 0 on a format error. */
int java_parse_double(const char *s, double *out);
int java_parse_float(const char *s, float *out);

/* ---- snapshot.c ---- */
/* The whole machine state as snapshot bytes, in a malloc'd buffer. */
JmeStatus snapshot_build(VM *vm, u8 **out, size_t *len);
u64 fnv1a64(const void *data, size_t len);
/* The sections after HEAP (CLAS to HOST) into *buf, which grows to *cap and
 * is kept for the next call. */
JmeStatus snapshot_rest(VM *vm, u8 **buf, size_t *cap, size_t *len);
/* Restores the rest in place, after the heap was; JME_E_UNSUPPORTED when
 * classes were loaded since, which only snapshot_load_parts can undo. */
JmeStatus snapshot_rest_restore(VM *vm, const u8 *rest, size_t len);
/* A full, checked load of vm's current heap with this rest. */
JmeStatus snapshot_load_parts(VM *vm, const u8 *rest, size_t len);

/* ---- lz.c: byte-oriented LZ77 for snapshots and rewind deltas ---- */
/* Worst-case compressed size of n bytes. */
size_t lz_bound(size_t n);
/* Compresses src into dst (at least lz_bound(n) bytes); returns the size. */
size_t lz_compress(const u8 *src, size_t n, u8 *dst);
/* Decompresses exactly out_len bytes; 0 on malformed input. */
int lz_decompress(const u8 *src, size_t n, u8 *dst, size_t out_len);

/* ---- fdlibm.c: the same bits on every platform ---- */
double fdlibm_sin(double x);
double fdlibm_cos(double x);
double fdlibm_tan(double x);

/* ---- slot helpers ---- */
static inline i64 slot_get_long(const Slot *s) {
	i64 v;
	memcpy(&v, s, 8);
	return v;
}
static inline void slot_set_long(Slot *s, i64 v) {
	memcpy(s, &v, 8);
}
static inline double slot_get_double(const Slot *s) {
	double v;
	memcpy(&v, s, 8);
	return v;
}
static inline float slot_get_float(const Slot *s) {
	float v;
	memcpy(&v, s, 4);
	return v;
}

#endif
