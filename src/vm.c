/* Public API: VM lifetime, startup and the scheduler. */
#include <stdio.h>
#include <stdlib.h>

#include "opcodes.h"
#include "vm.h"

/* Ticks a thread runs before the scheduler looks at the others. */
#define TIME_SLICE 2000u

const char *jme_status_name(JmeStatus status) {
	switch (status) {
	case JME_OK: return "JME_OK";
	case JME_E_NOMEM: return "JME_E_NOMEM";
	case JME_E_FORMAT: return "JME_E_FORMAT";
	case JME_E_UNSUPPORTED: return "JME_E_UNSUPPORTED";
	case JME_E_NOTFOUND: return "JME_E_NOTFOUND";
	case JME_E_ARGUMENT: return "JME_E_ARGUMENT";
	case JME_E_STATE: return "JME_E_STATE";
	case JME_E_UNIMPLEMENTED: return "JME_E_UNIMPLEMENTED";
	}
	return "unknown status";
}

const char *jme_version(void) {
	return JME_VERSION_STRING;
}

void jme_options_default(JmeOptions *options) {
	options->heap_bytes = 16u << 20;
	options->ticks_per_ms = 10000;
	options->epoch_ms = 0;
	options->gc_stress = 0;
}

JmeStatus jme_create(const JmeOptions *options, JmeVM **out) {
	*out = NULL;
	JmeOptions o;
	if (options != NULL) {
		o = *options;
	} else {
		jme_options_default(&o);
	}
	if (o.heap_bytes < (1u << 20) || o.heap_bytes > (1u << 30) || o.ticks_per_ms == 0)
		return JME_E_ARGUMENT;

	JmeVM *vm = calloc(1, sizeof(JmeVM));
	if (vm == NULL) return JME_E_NOMEM;
	vm->options = o;
	pool_init(&vm->pool);
	if (heap_init(vm) != JME_OK) {
		free(vm);
		return JME_E_NOMEM;
	}
	vm->sym.init = sym_cstr(vm, "<init>");
	vm->sym.clinit = sym_cstr(vm, "<clinit>");
	vm->sym.main = sym_cstr(vm, "main");
	vm->sym.main_desc = sym_cstr(vm, "([Ljava/lang/String;)V");
	vm->sym.void_desc = sym_cstr(vm, "()V");
	*out = vm;
	return JME_OK;
}

void jme_destroy(JmeVM *vm) {
	if (vm == NULL) return;
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *t = vm->threads[i];
		free(t->slots);
		free(t->tags);
		free(t->frames);
		free(t);
	}
	free(vm->threads);
	class_free_all(vm);
	heap_free(vm);
	strings_free(vm);
	monitors_free(vm);
	free(vm->syms.slots);
	host_setup_free(vm);
	pool_free_all(&vm->pool);
	free(vm);
}

void jme_set_class_source(JmeVM *vm, JmeClassSource source, void *user) {
	vm->class_source = source;
	vm->class_source_user = user;
}

void jme_set_console(JmeVM *vm, JmeConsoleFn console, void *user) {
	vm->console = console;
	vm->console_user = user;
}

void vm_console(VM *vm, int stream, const char *data, size_t len) {
	if (vm->console != NULL && len > 0) vm->console(vm->console_user, stream, data, len);
}

Thread *thread_new(VM *vm) {
	Thread **list = realloc(vm->threads, (vm->thread_count + 1) * sizeof(Thread *));
	if (list == NULL) return NULL;
	vm->threads = list;
	Thread *t = calloc(1, sizeof(Thread));
	if (t == NULL) return NULL;
	t->slots = calloc(THREAD_SLOTS, sizeof(Slot));
	t->tags = calloc(THREAD_SLOTS, 1);
	t->frames = calloc(THREAD_FRAMES, sizeof(Frame));
	if (t->slots == NULL || t->tags == NULL || t->frames == NULL) {
		free(t->slots);
		free(t->tags);
		free(t->frames);
		free(t);
		return NULL;
	}
	t->id = vm->thread_count + 1;
	t->state = TS_RUNNABLE;
	vm->threads[vm->thread_count++] = t;
	return t;
}

Thread *thread_by_id(VM *vm, u32 id) {
	return vm->threads[id - 1];
}

Thread *thread_of(VM *vm, Ref jthread) {
	if (jthread == 0) return NULL;
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *t = vm->threads[i];
		if (t->jthread == jthread && t->state != TS_DONE) return t;
	}
	return NULL;
}

void thread_end(VM *vm, Thread *t) {
	t->state = TS_DONE;
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *w = vm->threads[i];
		if (w->state == TS_JOINING && w->join_target == t->id) w->state = TS_RUNNABLE;
	}
	/* A dead thread keeps its id but nothing else: its stack is freed and
	 * its java.lang.Thread is no longer a root. */
	free(t->slots);
	free(t->tags);
	free(t->frames);
	t->slots = NULL;
	t->tags = NULL;
	t->frames = NULL;
	t->sp = 0;
	t->frame_count = 0;
	t->jthread = 0;
	t->exception = 0;
	t->blocked_on = 0;
	t->invoke_arg = 0;
}

int thread_start(VM *vm, Ref jthread) {
	Thread *t = thread_new(vm);
	if (t == NULL) {
		vm->current->exception = vm->oom_error;
		return 0;
	}
	t->jthread = jthread;
	t->slots[0] = jthread;
	t->tags[0] = 1;
	t->sp = 1;
	return frame_push(vm, t, vm->run_method, 0);
}

/* ---- startup ---- */

static const char *core_names[] = {
	"java/lang/Object", "java/lang/String", "java/lang/Class", "java/lang/Thread",
	"java/lang/Throwable", "java/lang/Error", "[C",
};

JmeStatus vm_bind_core(VM *vm) {
	Class *c[sizeof core_names / sizeof core_names[0]];
	for (size_t i = 0; i < sizeof core_names / sizeof core_names[0]; i++) {
		JmeStatus st = class_load(vm, core_names[i], &c[i]);
		if (st != JME_OK) return st;
		if (i == 0) vm->c_object = c[0];
	}
	vm->c_string = c[1];
	vm->c_class = c[2];
	vm->c_thread = c[3];
	vm->c_throwable = c[4];
	vm->c_error = c[5];
	vm->c_char_array = c[6];
	vm->f_string_value = field_offset(vm, vm->c_string, "value");
	vm->f_string_offset = field_offset(vm, vm->c_string, "offset");
	vm->f_string_count = field_offset(vm, vm->c_string, "count");
	vm->f_class_id = field_offset(vm, vm->c_class, "classId");
	vm->f_throwable_message = field_offset(vm, vm->c_throwable, "detailMessage");
	vm->f_throwable_trace = field_offset(vm, vm->c_throwable, "trace");
	vm->f_thread_name = field_offset(vm, vm->c_thread, "name");
	vm->f_thread_priority = field_offset(vm, vm->c_thread, "priority");
	return JME_OK;
}

/*
 * The bootstrap methods, whose constant pool entries are resolved up front:
 *   main:   aload_0; invokestatic #1 (the main method); return
 *   thread: aload_0; invokevirtual #2 (Thread.run); return
 */
static const u8 boot_bytecode[] = {OP_ALOAD_0, OP_INVOKESTATIC, 0, 1, OP_RETURN};
static const u8 run_bytecode[] = {OP_ALOAD_0, OP_INVOKEVIRTUAL, 0, 2, OP_RETURN};

static Method *boot_method(VM *vm, const u8 *bytes, u32 len, const char *name, const char *desc) {
	Pool *pool = &vm->boot_class->pool;
	CodeAttr *code = pool_alloc(pool, sizeof(CodeAttr));
	Method *m = pool_alloc(pool, sizeof(Method));
	if (code == NULL || m == NULL) return NULL;
	code->max_stack = 1;
	code->max_locals = 1;
	code->code_len = len;
	code->code = bytes;
	m->id = UINT32_MAX;
	m->owner = vm->boot_class;
	m->name = sym_cstr(vm, name);
	m->desc = sym_cstr(vm, desc);
	m->access = ACC_STATIC;
	m->arg_slots = 1;
	m->ret_type = 'V';
	m->prepared = 1;
	m->vtable_index = -1;
	m->code = code;
	return m;
}

JmeStatus vm_make_boot(VM *vm, Method *main) {
	Method *run = class_find_own_method(vm->c_thread, sym_cstr(vm, "run"), vm->sym.void_desc);
	if (run == NULL || main == NULL || !(main->access & ACC_STATIC) || main->name != vm->sym.main
			|| main->desc != vm->sym.main_desc)
		return JME_E_NOTFOUND;
	Class *b = calloc(1, sizeof(Class));
	if (b == NULL) return JME_E_NOMEM;
	pool_init(&b->pool);
	vm->boot_class = b;
	b->name = sym_cstr(vm, "jmevm/Boot");
	b->kind = CK_INSTANCE;
	b->state = CS_INITIALIZED;
	b->access = ACC_PUBLIC | ACC_SUPER;
	b->resolved = pool_alloc(&b->pool, 3 * sizeof(Resolved));
	b->resolved_flag = pool_alloc(&b->pool, 3);
	if (b->resolved == NULL || b->resolved_flag == NULL) return JME_E_NOMEM;
	b->resolved[1].method = main;
	b->resolved[2].method = run;
	b->resolved_flag[1] = b->resolved_flag[2] = 1;
	vm->main_method = main;
	vm->boot_method = boot_method(vm, boot_bytecode, sizeof boot_bytecode, "<main>", "([Ljava/lang/String;)V");
	vm->run_method = boot_method(vm, run_bytecode, sizeof run_bytecode, "<thread>", "(Ljava/lang/Thread;)V");
	return vm->boot_method != NULL && vm->run_method != NULL ? JME_OK : JME_E_NOMEM;
}

JmeStatus jme_start(JmeVM *vm, const char *main_class, int argc, const char *const *argv) {
	if (vm->started || vm->class_source == NULL) return JME_E_STATE;
	Thread *t = thread_new(vm);
	if (t == NULL) return JME_E_NOMEM;
	vm->current = t;
	JmeStatus st = vm_bind_core(vm);
	if (st != JME_OK) return st;
	Class *oom;
	if ((st = class_load(vm, "java/lang/OutOfMemoryError", &oom)) != JME_OK) return st;
	vm->oom_error = new_object(vm, oom);
	if (vm->oom_error == 0) return JME_E_NOMEM;

	/* java.lang.Thread for main, made without running a constructor */
	t->jthread = new_object(vm, vm->c_thread);
	if (t->jthread == 0) return JME_E_NOMEM;
	*(i32 *)FIELD_PTR(vm, t->jthread, vm->f_thread_priority) = 5;
	Ref name = string_from_cstr(vm, "main");
	if (name == 0) return JME_E_NOMEM;
	*(Ref *)FIELD_PTR(vm, t->jthread, vm->f_thread_name) = name;

	size_t n = strlen(main_class);
	char *internal = malloc(n + 1);
	if (internal == NULL) return JME_E_NOMEM;
	for (size_t i = 0; i <= n; i++) internal[i] = main_class[i] == '.' ? '/' : main_class[i];
	Class *c;
	st = class_load(vm, internal, &c);
	free(internal);
	if (st != JME_OK) return st;
	if ((st = vm_make_boot(vm, class_find_own_method(c, vm->sym.main, vm->sym.main_desc))) != JME_OK) return st;

	/* String[] args, kept on the stack where the collector sees it */
	Class *string_array = array_class_of(vm, vm->c_string);
	Ref array = string_array != NULL ? new_array(vm, string_array, argc) : 0;
	if (array == 0) return JME_E_NOMEM;
	t->slots[0] = array;
	t->tags[0] = 1;
	t->sp = 1;
	for (int i = 0; i < argc; i++) {
		Ref s = string_from_cstr(vm, argv[i]);
		if (s == 0) return JME_E_NOMEM;
		REF_ELEMS(vm, t->slots[0])[i] = s;
	}
	if (!frame_push(vm, t, vm->boot_method, 0)) return JME_E_NOMEM;
	vm->started = 1;
	return JME_OK;
}

/* ---- scheduler ---- */

static int timed_wait(const Thread *t) {
	return (t->state == TS_SLEEPING || t->state == TS_WAITING || t->state == TS_HOST_WAIT) && t->timed;
}

/*
 * Round robin: threads take turns of up to TIME_SLICE ticks, starting after
 * the one that ran last. A thread that blocks, sleeps or yields ends its turn
 * early. When nobody can run, time jumps to the next timeout. A turn that is
 * still going when target_ticks is reached carries on in the next call, so
 * running in steps behaves exactly like running in one go.
 */
JmeStatus jme_run_until(JmeVM *vm, uint64_t target_ticks) {
	if (!vm->started) return JME_E_STATE;
	while (!vm->finished && vm->ticks < target_ticks) {
		for (u32 i = 0; i < vm->thread_count; i++) {
			Thread *t = vm->threads[i];
			if (!timed_wait(t) || t->wake_tick > vm->ticks) continue;
			t->timed = 0;
			if (t->state != TS_WAITING) t->state = TS_RUNNABLE; /* sleeping or host wait */
			else monitor_wake_waiter(vm, t);
		}
		u32 n = vm->thread_count;
		Thread *run = NULL;
		if (vm->turn != 0) {
			Thread *t = vm->threads[vm->turn - 1];
			if (t->state == TS_RUNNABLE && vm->ticks < vm->turn_end) run = t;
			else vm->turn = 0;
		}
		for (u32 k = 0; k < n && run == NULL; k++) {
			u32 i = (vm->sched_next + k) % n;
			if (vm->threads[i]->state == TS_RUNNABLE) {
				run = vm->threads[i];
				vm->sched_next = i + 1;
				vm->turn = i + 1;
				vm->turn_end = vm->ticks + TIME_SLICE;
			}
		}
		if (run != NULL) {
			u64 limit = vm->turn_end < target_ticks ? vm->turn_end : target_ticks;
			interp_run(vm, run, limit);
			/* Stopping before the limit means the thread blocked, slept or
			 * yielded: its turn is over. */
			if (run->state != TS_RUNNABLE || vm->ticks < limit || vm->ticks >= vm->turn_end) vm->turn = 0;
			continue;
		}
		u64 wake = UINT64_MAX;
		int alive = 0;
		for (u32 i = 0; i < n; i++) {
			Thread *t = vm->threads[i];
			if (t->state != TS_DONE) alive = 1;
			if (timed_wait(t) && t->wake_tick < wake) wake = t->wake_tick;
		}
		if (!alive) {
			vm->finished = 1;
			break;
		}
		if (wake == UINT64_MAX) {
			/* Every live thread waits for something only another thread or
			 * the host could do. Time still passes up to the target. */
			if (target_ticks != UINT64_MAX) vm->ticks = target_ticks;
			break;
		}
		vm->ticks = wake < target_ticks ? wake : target_ticks;
	}
	return JME_OK;
}

int jme_finished(const JmeVM *vm, int *exit_code) {
	int done = vm->finished;
	if (!done && vm->started) {
		done = 1;
		for (u32 i = 0; i < vm->thread_count; i++)
			if (vm->threads[i]->state != TS_DONE) done = 0;
	}
	if (done && exit_code != NULL) *exit_code = vm->exit_code;
	return done;
}

uint64_t jme_ticks(const JmeVM *vm) {
	return vm->ticks;
}
