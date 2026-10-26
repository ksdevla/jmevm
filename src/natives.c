/*
 * Natives of the class library (java.lang and the jmevm.* glue).
 *
 * A native reads its arguments from args (args[0] is this for instance
 * methods), stores its result in *ret and returns N_OK, or makes an
 * exception pending and returns N_THROW. Arguments on the stack stay up to
 * date across allocations; local copies of Refs do not.
 */
#include <math.h>
#include <stdlib.h>

#include "vm.h"

#define NATIVE(name) static NativeResult name(VM *vm, Thread *t, Slot *args, Value *ret)
#define UNUSED() (void)vm, (void)t, (void)args, (void)ret

static NativeResult throw_result(VM *vm, const char *cls, const char *msg) {
	throw_new(vm, cls, msg);
	return N_THROW;
}

static NativeResult string_result(VM *vm, const char *s, Value *ret) {
	ret->r = string_from_cstr(vm, s);
	return ret->r != 0 ? N_OK : N_THROW;
}

/* ---- java.lang.Object ---- */

NATIVE(object_get_class) {
	(void)t;
	ret->r = class_mirror(vm, class_of(vm, args[0]));
	return ret->r != 0 ? N_OK : N_THROW;
}

NATIVE(object_hash_code) {
	(void)t;
	ret->i = identity_hash(vm, args[0]);
	return N_OK;
}

NATIVE(object_clone) {
	(void)t;
	Class *c = class_of(vm, args[0]);
	if (c->kind != CK_ARRAY) return throw_result(vm, "java/lang/CloneNotSupportedException", NULL);
	Ref copy = new_array(vm, c, (i32)ARRAY_LEN(vm, args[0]));
	if (copy == 0) return N_THROW;
	memcpy(ARRAY_PTR(vm, copy), ARRAY_PTR(vm, args[0]), (size_t)ARRAY_LEN(vm, args[0]) * c->elem_size);
	ret->r = copy;
	return N_OK;
}

NATIVE(object_notify) {
	(void)ret;
	if (!monitor_owned(vm, t, args[0])) return throw_result(vm, "java/lang/IllegalMonitorStateException", NULL);
	monitor_notify(vm, args[0], 0);
	return N_OK;
}

NATIVE(object_notify_all) {
	(void)ret;
	if (!monitor_owned(vm, t, args[0])) return throw_result(vm, "java/lang/IllegalMonitorStateException", NULL);
	monitor_notify(vm, args[0], 1);
	return N_OK;
}

static NativeResult interrupted_exception(VM *vm, Thread *t) {
	t->interrupted = 0;
	return throw_result(vm, "java/lang/InterruptedException", NULL);
}

/*
 * Runs twice: the first call releases the monitor and blocks; when the
 * thread was notified, timed out or interrupted and holds the monitor
 * again, the invoke runs again and returns (or throws).
 */
NATIVE(object_wait) {
	(void)ret;
	if (t->block_phase == 2) {
		if (!monitor_reacquired(vm, t)) return throw_result(vm, "java/lang/InternalError", "wait");
		t->block_phase = 0;
		return t->interrupted ? interrupted_exception(vm, t) : N_OK;
	}
	i64 ms = slot_get_long(&args[1]);
	if (!monitor_owned(vm, t, args[0])) return throw_result(vm, "java/lang/IllegalMonitorStateException", NULL);
	if (ms < 0) return throw_result(vm, "java/lang/IllegalArgumentException", "timeout value is negative");
	if (t->interrupted) return interrupted_exception(vm, t);
	monitor_wait(vm, t, args[0], ms);
	return N_REEXECUTE;
}

/* ---- java.lang.Class ---- */

static char *java_to_internal(VM *vm, Ref name) {
	char *s = string_to_utf8(vm, name);
	if (s != NULL)
		for (char *p = s; *p; p++)
			if (*p == '.') *p = '/';
	return s;
}

NATIVE(class_for_name) {
	if (args[0] == 0) return throw_result(vm, "java/lang/NullPointerException", NULL);
	char *name = java_to_internal(vm, args[0]);
	if (name == NULL) {
		t->exception = vm->oom_error;
		return N_THROW;
	}
	Class *c;
	JmeStatus st = class_load(vm, name, &c);
	if (st != JME_OK) {
		char *java_name = string_to_utf8(vm, args[0]);
		throw_new(vm, st == JME_E_NOTFOUND ? "java/lang/ClassNotFoundException" : "java/lang/NoClassDefFoundError",
				java_name);
		free(java_name);
		free(name);
		return N_THROW;
	}
	free(name);
	int r = class_init(vm, t, c);
	if (r == INIT_THROWN) return N_THROW;
	if (r != INIT_DONE) return N_REEXECUTE;
	ret->r = class_mirror(vm, c);
	return ret->r != 0 ? N_OK : N_THROW;
}

NATIVE(class_new_instance) {
	Class *c = mirror_class(vm, args[0]);
	if (c->kind == CK_ARRAY || (c->access & (ACC_INTERFACE | ACC_ABSTRACT)))
		return throw_result(vm, "java/lang/InstantiationException", c->name);
	Method *ctor = class_find_own_method(c, vm->sym.init, vm->sym.void_desc);
	if (ctor == NULL) return throw_result(vm, "java/lang/InstantiationException", c->name);
	if (!(ctor->access & ACC_PUBLIC) || !(c->access & ACC_PUBLIC))
		return throw_result(vm, "java/lang/IllegalAccessException", c->name);
	int r = class_init(vm, t, c);
	if (r == INIT_THROWN) return N_THROW;
	if (r != INIT_DONE) return N_REEXECUTE;
	Ref o = new_object(vm, c);
	if (o == 0) return N_THROW;
	ret->r = o;
	t->invoke_after = ctor;
	t->invoke_arg = o;
	return N_OK;
}

NATIVE(class_is_instance) {
	(void)t;
	ret->i = args[1] != 0 && class_is_assignable(class_of(vm, args[1]), mirror_class(vm, args[0]));
	return N_OK;
}

NATIVE(class_is_assignable_from) {
	(void)t;
	if (args[1] == 0) return throw_result(vm, "java/lang/NullPointerException", NULL);
	ret->i = class_is_assignable(mirror_class(vm, args[1]), mirror_class(vm, args[0]));
	return N_OK;
}

NATIVE(class_is_interface) {
	(void)t;
	ret->i = (mirror_class(vm, args[0])->access & ACC_INTERFACE) != 0;
	return N_OK;
}

NATIVE(class_is_array) {
	(void)t;
	ret->i = mirror_class(vm, args[0])->kind == CK_ARRAY;
	return N_OK;
}

NATIVE(class_get_name) {
	(void)t;
	char name[512];
	class_java_name(mirror_class(vm, args[0]), name, sizeof name);
	return string_result(vm, name, ret);
}

/* ---- java.lang.Double, Float, Math ---- */

static char *string_arg(VM *vm, Thread *t, Ref s) {
	if (s == 0) {
		throw_npe(vm);
		return NULL;
	}
	char *c = string_to_utf8(vm, s);
	if (c == NULL) t->exception = vm->oom_error;
	return c;
}

NATIVE(double_to_string) {
	(void)t;
	char buf[40];
	double_to_java_string(slot_get_double(args), buf);
	return string_result(vm, buf, ret);
}

NATIVE(double_parse) {
	char *s = string_arg(vm, t, args[0]);
	if (s == NULL) return N_THROW;
	int ok = java_parse_double(s, &ret->d);
	if (!ok) throw_new(vm, "java/lang/NumberFormatException", s);
	free(s);
	return ok ? N_OK : N_THROW;
}

NATIVE(double_to_long_bits) {
	UNUSED();
	double d = slot_get_double(args);
	ret->j = d != d ? 0x7ff8000000000000LL : slot_get_long(args);
	return N_OK;
}

NATIVE(double_from_long_bits) {
	UNUSED();
	ret->j = slot_get_long(args);
	return N_OK;
}

NATIVE(float_to_string) {
	(void)t;
	char buf[40];
	float_to_java_string(slot_get_float(args), buf);
	return string_result(vm, buf, ret);
}

NATIVE(float_parse) {
	char *s = string_arg(vm, t, args[0]);
	if (s == NULL) return N_THROW;
	int ok = java_parse_float(s, &ret->f);
	if (!ok) throw_new(vm, "java/lang/NumberFormatException", s);
	free(s);
	return ok ? N_OK : N_THROW;
}

NATIVE(float_to_int_bits) {
	UNUSED();
	float f = slot_get_float(args);
	ret->i = f != f ? 0x7fc00000 : (i32)args[0];
	return N_OK;
}

NATIVE(float_from_int_bits) {
	UNUSED();
	ret->i = (i32)args[0];
	return N_OK;
}

/* fdlibm, so replays match across platforms; sqrt, floor and ceil are exact
 * everywhere. */
NATIVE(math_sin) { UNUSED(); ret->d = fdlibm_sin(slot_get_double(args)); return N_OK; }
NATIVE(math_cos) { UNUSED(); ret->d = fdlibm_cos(slot_get_double(args)); return N_OK; }
NATIVE(math_tan) { UNUSED(); ret->d = fdlibm_tan(slot_get_double(args)); return N_OK; }
NATIVE(math_sqrt) { UNUSED(); ret->d = sqrt(slot_get_double(args)); return N_OK; }
NATIVE(math_floor) { UNUSED(); ret->d = floor(slot_get_double(args)); return N_OK; }
NATIVE(math_ceil) { UNUSED(); ret->d = ceil(slot_get_double(args)); return N_OK; }

/* ---- java.lang.Runtime, System, String ---- */

NATIVE(runtime_exit) {
	(void)t;
	(void)ret;
	vm->exit_code = (i32)args[1];
	vm->finished = 1;
	for (u32 i = 0; i < vm->thread_count; i++) vm->threads[i]->state = TS_DONE;
	return N_YIELD;
}

NATIVE(runtime_free_memory) {
	UNUSED();
	ret->j = vm->heap_size - vm->heap_top;
	return N_OK;
}

NATIVE(runtime_total_memory) {
	UNUSED();
	ret->j = vm->heap_size - HEAP_START;
	return N_OK;
}

NATIVE(runtime_gc) {
	UNUSED();
	heap_collect(vm);
	return N_OK;
}

NATIVE(system_current_time) {
	UNUSED();
	ret->j = vm->options.epoch_ms + (i64)(vm->ticks / vm->options.ticks_per_ms);
	return N_OK;
}

NATIVE(system_identity_hash) {
	(void)t;
	ret->i = args[0] == 0 ? 0 : identity_hash(vm, args[0]);
	return N_OK;
}

NATIVE(system_arraycopy) {
	(void)ret;
	Ref src = args[0], dst = args[2];
	i32 sp = (i32)args[1], dp = (i32)args[3], n = (i32)args[4];
	if (src == 0 || dst == 0) return throw_result(vm, "java/lang/NullPointerException", NULL);
	Class *sc = class_of(vm, src), *dc = class_of(vm, dst);
	if (sc->kind != CK_ARRAY || dc->kind != CK_ARRAY) return throw_result(vm, "java/lang/ArrayStoreException", NULL);
	int refs = sc->elem_type == T_OBJECT;
	if (sc->elem_type != dc->elem_type) return throw_result(vm, "java/lang/ArrayStoreException", NULL);
	if (sp < 0 || dp < 0 || n < 0 || (i64)sp + n > ARRAY_LEN(vm, src) || (i64)dp + n > ARRAY_LEN(vm, dst))
		return throw_result(vm, "java/lang/ArrayIndexOutOfBoundsException", NULL);
	u32 size = sc->elem_size;
	heap_mark(vm, dst + ARRAY_DATA + (u32)dp * size, dst + ARRAY_DATA + (u32)(dp + n) * size);
	if (!refs || class_is_assignable(sc->component, dc->component)) {
		memmove((u8 *)ARRAY_PTR(vm, dst) + (size_t)dp * size, (u8 *)ARRAY_PTR(vm, src) + (size_t)sp * size,
				(size_t)n * size);
		return N_OK;
	}
	/* Element by element, stopping at the first one that does not fit. */
	for (i32 i = 0; i < n; i++) {
		Ref e = REF_ELEMS(vm, src)[sp + i];
		if (e != 0 && !class_is_assignable(class_of(vm, e), dc->component))
			return throw_result(vm, "java/lang/ArrayStoreException", NULL);
		REF_ELEMS(vm, dst)[dp + i] = e;
	}
	(void)t;
	return N_OK;
}

NATIVE(system_get_property) {
	static const char *props[][2] = {
		{"microedition.configuration", "CLDC-1.1"},
		{"microedition.encoding", "ISO-8859-1"},
		{"microedition.platform", "jmevm"},
	};
	char *key = string_arg(vm, t, args[0]);
	if (key == NULL) return N_THROW;
	const char *value = host_property(vm, key);
	for (size_t i = 0; value == NULL && i < sizeof props / sizeof props[0]; i++)
		if (strcmp(key, props[i][0]) == 0) value = props[i][1];
	free(key);
	if (value == NULL) {
		ret->r = 0;
		return N_OK;
	}
	return string_result(vm, value, ret);
}

NATIVE(string_intern_native) {
	(void)t;
	ret->r = string_intern(vm, args[0]);
	return ret->r != 0 ? N_OK : N_THROW;
}

/* ---- java.lang.Thread ---- */

NATIVE(thread_current) {
	(void)vm;
	(void)args;
	ret->r = t->jthread;
	return N_OK;
}

NATIVE(thread_yield) {
	UNUSED();
	return N_YIELD;
}

/* Runs twice like wait: blocks, then returns once woken (or throws). */
NATIVE(thread_sleep) {
	(void)ret;
	if (t->block_phase == 1) {
		t->block_phase = 0;
		return t->interrupted ? interrupted_exception(vm, t) : N_OK;
	}
	i64 ms = slot_get_long(args);
	if (ms < 0) return throw_result(vm, "java/lang/IllegalArgumentException", "timeout value is negative");
	if (t->interrupted) return interrupted_exception(vm, t);
	if (ms == 0) return N_YIELD;
	t->state = TS_SLEEPING;
	t->timed = 1;
	t->wake_tick = vm->ticks + (u64)ms * vm->options.ticks_per_ms;
	t->block_phase = 1;
	return N_REEXECUTE;
}

NATIVE(thread_start_native) {
	UNUSED();
	if (thread_of(vm, args[0]) != NULL) return throw_result(vm, "java/lang/IllegalThreadStateException", NULL);
	return thread_start(vm, args[0]) ? N_OK : N_THROW;
}

NATIVE(thread_is_alive) {
	(void)t;
	ret->i = thread_of(vm, args[0]) != NULL;
	return N_OK;
}

/* Blocks until the thread has ended; runs again each time it is woken. */
NATIVE(thread_join) {
	(void)ret;
	if (t->interrupted) return interrupted_exception(vm, t);
	Thread *target = thread_of(vm, args[0]);
	if (target == NULL) return N_OK;
	t->state = TS_JOINING;
	t->join_target = target->id;
	return N_REEXECUTE;
}

NATIVE(thread_interrupt) {
	(void)t;
	(void)ret;
	Thread *target = thread_of(vm, args[0]);
	if (target == NULL) return N_OK;
	target->interrupted = 1;
	switch (target->state) {
	case TS_SLEEPING:
	case TS_JOINING:
		target->state = TS_RUNNABLE;
		break;
	case TS_WAITING:
		monitor_wake_waiter(vm, target);
		break;
	default:
		break;
	}
	return N_OK;
}

NATIVE(thread_active_count) {
	(void)t;
	(void)args;
	i32 n = 0;
	for (u32 i = 0; i < vm->thread_count; i++)
		if (vm->threads[i]->state != TS_DONE) n++;
	ret->i = n;
	return N_OK;
}

NATIVE(thread_set_priority) {
	UNUSED();
	return N_OK;
}

/* ---- java.lang.Throwable ---- */

NATIVE(throwable_fill) {
	(void)ret;
	fill_stack_trace(vm, t, args[0], 1);
	return N_OK;
}

NATIVE(throwable_print) {
	UNUSED();
	print_stack_trace(vm, args[0], "");
	return N_OK;
}

/* ---- jmevm.ConsoleOutputStream, jmevm.Resources ---- */

NATIVE(console_write) {
	UNUSED();
	char b = (char)args[1];
	vm_console(vm, (i32)args[0], &b, 1);
	return N_OK;
}

NATIVE(console_write_bytes) {
	(void)ret;
	(void)t;
	Ref a = args[1];
	i32 off = (i32)args[2], len = (i32)args[3];
	if (a == 0) return throw_result(vm, "java/lang/NullPointerException", NULL);
	if (off < 0 || len < 0 || (i64)off + len > ARRAY_LEN(vm, a))
		return throw_result(vm, "java/lang/ArrayIndexOutOfBoundsException", NULL);
	vm_console(vm, (i32)args[0], (const char *)ARRAY_PTR(vm, a) + off, (size_t)len);
	return N_OK;
}

/* The host's resource source (jme_set_resource_source); null without one. */
NATIVE(resources_read) {
	ret->r = 0;
	if (vm->host.resources == NULL) return N_OK;
	char *name = string_arg(vm, t, args[0]);
	if (name == NULL) return N_THROW;
	const u8 *data;
	size_t len;
	JmeStatus st = vm->host.resources(vm->host.resources_user, name, &data, &len);
	free(name);
	if (st != JME_OK) return N_OK;
	if (len > INT32_MAX) return throw_result(vm, "java/lang/OutOfMemoryError", NULL);
	Class *bytes = primitive_array_class(vm, T_BYTE);
	Ref a = bytes != NULL ? new_array(vm, bytes, (i32)len) : 0;
	if (a == 0) return N_THROW;
	memcpy(ARRAY_PTR(vm, a), data, len);
	ret->r = a;
	return N_OK;
}

static const NativeEntry natives[] = {
	{"java/lang/Object", "getClass", "()Ljava/lang/Class;", object_get_class},
	{"java/lang/Object", "hashCode", "()I", object_hash_code},
	{"java/lang/Object", "clone", "()Ljava/lang/Object;", object_clone},
	{"java/lang/Object", "notify", "()V", object_notify},
	{"java/lang/Object", "notifyAll", "()V", object_notify_all},
	{"java/lang/Object", "wait", "(J)V", object_wait},
	{"java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", class_for_name},
	{"java/lang/Class", "newInstance", "()Ljava/lang/Object;", class_new_instance},
	{"java/lang/Class", "isInstance", "(Ljava/lang/Object;)Z", class_is_instance},
	{"java/lang/Class", "isAssignableFrom", "(Ljava/lang/Class;)Z", class_is_assignable_from},
	{"java/lang/Class", "isInterface", "()Z", class_is_interface},
	{"java/lang/Class", "isArray", "()Z", class_is_array},
	{"java/lang/Class", "getName", "()Ljava/lang/String;", class_get_name},
	{"java/lang/Double", "toString", "(D)Ljava/lang/String;", double_to_string},
	{"java/lang/Double", "parseDouble", "(Ljava/lang/String;)D", double_parse},
	{"java/lang/Double", "doubleToLongBits", "(D)J", double_to_long_bits},
	{"java/lang/Double", "longBitsToDouble", "(J)D", double_from_long_bits},
	{"java/lang/Float", "toString", "(F)Ljava/lang/String;", float_to_string},
	{"java/lang/Float", "parseFloat", "(Ljava/lang/String;)F", float_parse},
	{"java/lang/Float", "floatToIntBits", "(F)I", float_to_int_bits},
	{"java/lang/Float", "intBitsToFloat", "(I)F", float_from_int_bits},
	{"java/lang/Math", "sin", "(D)D", math_sin},
	{"java/lang/Math", "cos", "(D)D", math_cos},
	{"java/lang/Math", "tan", "(D)D", math_tan},
	{"java/lang/Math", "sqrt", "(D)D", math_sqrt},
	{"java/lang/Math", "floor", "(D)D", math_floor},
	{"java/lang/Math", "ceil", "(D)D", math_ceil},
	{"java/lang/Runtime", "exit", "(I)V", runtime_exit},
	{"java/lang/Runtime", "freeMemory", "()J", runtime_free_memory},
	{"java/lang/Runtime", "totalMemory", "()J", runtime_total_memory},
	{"java/lang/Runtime", "gc", "()V", runtime_gc},
	{"java/lang/String", "intern", "()Ljava/lang/String;", string_intern_native},
	{"java/lang/System", "currentTimeMillis", "()J", system_current_time},
	{"java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", system_arraycopy},
	{"java/lang/System", "identityHashCode", "(Ljava/lang/Object;)I", system_identity_hash},
	{"java/lang/System", "getProperty0", "(Ljava/lang/String;)Ljava/lang/String;", system_get_property},
	{"java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", thread_current},
	{"java/lang/Thread", "yield", "()V", thread_yield},
	{"java/lang/Thread", "sleep", "(J)V", thread_sleep},
	{"java/lang/Thread", "start0", "()V", thread_start_native},
	{"java/lang/Thread", "isAlive", "()Z", thread_is_alive},
	{"java/lang/Thread", "join", "()V", thread_join},
	{"java/lang/Thread", "interrupt0", "()V", thread_interrupt},
	{"java/lang/Thread", "activeCount", "()I", thread_active_count},
	{"java/lang/Thread", "setPriority0", "(I)V", thread_set_priority},
	{"java/lang/Throwable", "fillInStackTrace", "()V", throwable_fill},
	{"java/lang/Throwable", "printStackTrace0", "()V", throwable_print},
	{"jmevm/ConsoleOutputStream", "write0", "(II)V", console_write},
	{"jmevm/ConsoleOutputStream", "writeBytes0", "(I[BII)V", console_write_bytes},
	{"jmevm/Resources", "read", "(Ljava/lang/String;)[B", resources_read},
};

NativeFn natives_find(Method *m) {
	for (size_t i = 0; i < sizeof natives / sizeof natives[0]; i++) {
		const NativeEntry *e = &natives[i];
		if (strcmp(e->cls, m->owner->name) == 0 && strcmp(e->name, m->name) == 0 && strcmp(e->desc, m->desc) == 0)
			return e->fn;
	}
	return NULL;
}
