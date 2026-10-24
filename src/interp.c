/*
 * The bytecode interpreter.
 *
 * One loop runs every frame of a thread: invokes push a frame and returns pop
 * one, with no C recursion. Instructions that need a class initialized, or a
 * native that needs one, push the <clinit> frame and run again once it has
 * returned. Exceptions unwind frames inside the loop.
 *
 * Emulated time advances by one tick per invoke and per backward branch;
 * those are also the only points where the loop stops when it reaches its
 * tick limit, with all state saved in the thread's frames.
 *
 * Anything that may allocate can move objects, so the code below never keeps
 * a Ref or Object pointer in a C variable across an allocation: it reads it
 * again from the operand stack. SAVE() stores pc and sp in the frame first so
 * the collector sees the whole stack.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "opcodes.h"
#include "vm.h"

/* ---- frames ---- */

int frame_push(VM *vm, Thread *t, Method *m, u32 flags) {
	if (!method_prepare(vm, m)) return 0;
	const CodeAttr *code = m->code;
	u32 base = t->sp - m->arg_slots;
	if (t->frame_count == THREAD_FRAMES || (u64)base + code->max_locals + code->max_stack > THREAD_SLOTS) {
		throw_new(vm, "java/lang/StackOverflowError", NULL);
		return 0;
	}
	Ref lock = 0;
	if (m->access & ACC_SYNCHRONIZED) {
		/* The mirror may need allocating: do it while the arguments still
		 * belong to the caller's stack. */
		lock = (m->access & ACC_STATIC) ? class_mirror(vm, m->owner) : t->slots[base];
		if (lock == 0) return 0;
		if (!monitor_enter(vm, t, lock)) return 1; /* blocked: the invoke runs again */
	}
	for (u32 i = base + m->arg_slots; i < base + code->max_locals; i++) {
		t->slots[i] = 0;
		t->tags[i] = 0;
	}
	if (t->frame_count > 0) t->frames[t->frame_count - 1].sp = base;
	Frame *f = &t->frames[t->frame_count++];
	f->method = m;
	f->pc = 0;
	f->locals = base;
	f->sp = base + code->max_locals;
	f->flags = flags;
	f->init_class = NULL;
	f->lock = lock;
	t->sp = f->sp;
	return 1;
}

static u32 invoke_length(const Frame *f) {
	return f->method->code->code[f->pc] == OP_INVOKEINTERFACE ? 5 : 3;
}

/*
 * Pops the top frame and hands its n result slots (at val) to the caller.
 * Returns 0 when that was the thread's last frame.
 */
static int frame_return(VM *vm, Thread *t, const Slot *val, u32 n) {
	Frame *f = &t->frames[t->frame_count - 1];
	if (f->lock != 0) monitor_exit(vm, t, f->lock);
	if (f->flags & F_CLINIT) {
		f->init_class->state = CS_INITIALIZED;
		class_init_finished(vm, f->init_class);
	}
	u32 flags = f->flags;
	u32 tag = n == 1 ? t->tags[val - t->slots] : 0;
	t->frame_count--;
	if (t->frame_count == 0) {
		thread_end(vm, t);
		return 0;
	}
	Frame *c = &t->frames[t->frame_count - 1];
	memmove(&t->slots[c->sp], val, n * sizeof(Slot));
	for (u32 i = 0; i < n; i++) t->tags[c->sp + i] = (u8)tag;
	c->sp += n;
	if (flags & F_ADVANCE) c->pc += invoke_length(c);
	t->sp = c->sp;
	return 1;
}

/* ---- exceptions ---- */

void vm_printf(VM *vm, int stream, const char *fmt, ...) {
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n < 0) return;
	vm_console(vm, stream, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
}

static int is_throwable_init(VM *vm, Method *m) {
	return m->name == vm->sym.init && class_is_subclass(m->owner, vm->c_throwable);
}

void fill_stack_trace(VM *vm, Thread *t, Ref ex, int skip_init) {
	u32 top = t->frame_count;
	if (skip_init)
		while (top > 0 && is_throwable_init(vm, t->frames[top - 1].method)) top--;
	u32 n = 0;
	for (u32 i = 0; i < top; i++)
		if (t->frames[i].method->owner != vm->boot_class) n++;
	/* Without memory for a trace the throwable goes without one: whatever
	 * exception was pending stays pending. */
	Ref pending = t->exception;
	ROOT(vm, ex);
	ROOT(vm, pending);
	Class *int_array = primitive_array_class(vm, T_INT);
	Ref trace = int_array != NULL ? new_array(vm, int_array, (i32)(n * 2)) : 0;
	UNROOT(vm, 2);
	if (trace == 0) {
		t->exception = pending;
		return;
	}
	i32 *e = ARRAY_PTR(vm, trace);
	for (u32 i = top; i-- > 0;) {
		const Frame *f = &t->frames[i];
		if (f->method->owner == vm->boot_class) continue;
		*e++ = (i32)f->method->id;
		*e++ = (i32)f->pc;
	}
	*(Ref *)FIELD_PTR(vm, ex, vm->f_throwable_trace) = trace;
}

void throw_new(VM *vm, const char *cls, const char *msg) {
	Thread *t = vm->current;
	Class *c;
	if (class_load(vm, cls, &c) != JME_OK) {
		fprintf(stderr, "jmevm: the class library has no %s\n", cls);
		abort();
	}
	Ref ex = new_object(vm, c);
	if (ex == 0) return;
	ROOT(vm, ex);
	if (msg != NULL) {
		Ref s = string_from_cstr(vm, msg);
		if (s == 0) {
			UNROOT(vm, 1);
			return;
		}
		*(Ref *)FIELD_PTR(vm, ex, vm->f_throwable_message) = s;
	}
	fill_stack_trace(vm, t, ex, 0);
	UNROOT(vm, 1);
	t->exception = ex;
}

void throw_npe(VM *vm) {
	throw_new(vm, "java/lang/NullPointerException", NULL);
}

static void throw_index(VM *vm, i32 index) {
	char msg[16];
	snprintf(msg, sizeof msg, "%d", index);
	throw_new(vm, "java/lang/ArrayIndexOutOfBoundsException", msg);
}

static int line_number(const Method *m, u32 pc) {
	const CodeAttr *c = m->code;
	int line = -1;
	u32 best = 0;
	for (u32 i = 0; c != NULL && i < c->line_count; i++)
		if (c->lines[i].start_pc <= pc && c->lines[i].start_pc >= best) {
			best = c->lines[i].start_pc;
			line = c->lines[i].line;
		}
	return line;
}

void print_stack_trace(VM *vm, Ref ex, const char *prefix) {
	char name[256];
	class_java_name(class_of(vm, ex), name, sizeof name);
	Ref msg = *(Ref *)FIELD_PTR(vm, ex, vm->f_throwable_message);
	char *text = msg != 0 ? string_to_utf8(vm, msg) : NULL;
	if (text != NULL) {
		vm_printf(vm, 2, "%s%s: %s\n", prefix, name, text);
		free(text);
	} else {
		vm_printf(vm, 2, "%s%s\n", prefix, name);
	}
	Ref trace = *(Ref *)FIELD_PTR(vm, ex, vm->f_throwable_trace);
	if (trace == 0) return;
	u32 n = ARRAY_LEN(vm, trace) / 2;
	for (u32 i = 0; i < n; i++) {
		const i32 *e = (const i32 *)ARRAY_PTR(vm, trace) + 2 * i;
		if ((u32)e[0] >= vm->method_count) continue;
		const Method *m = vm->methods[e[0]];
		char cname[256];
		class_java_name(m->owner, cname, sizeof cname);
		const ClassFile *cf = m->owner->cf;
		int line = line_number(m, (u32)e[1]);
		if ((m->access & ACC_NATIVE) || cf == NULL || cf->source_file.bytes == NULL) {
			vm_printf(vm, 2, "\tat %s.%s(%s)\n", cname, m->name,
					(m->access & ACC_NATIVE) ? "Native Method" : "Unknown Source");
		} else if (line < 0) {
			vm_printf(vm, 2, "\tat %s.%s(%.*s)\n", cname, m->name, cf->source_file.len, cf->source_file.bytes);
		} else {
			vm_printf(vm, 2, "\tat %s.%s(%.*s:%d)\n", cname, m->name, cf->source_file.len,
					cf->source_file.bytes, line);
		}
	}
}

static void uncaught(VM *vm, Thread *t) {
	Ref ex = t->exception;
	t->exception = 0;
	t->sp = 0;
	char prefix[200];
	Ref name = t->jthread != 0 ? *(Ref *)FIELD_PTR(vm, t->jthread, vm->f_thread_name) : 0;
	char *text = name != 0 ? string_to_utf8(vm, name) : NULL;
	snprintf(prefix, sizeof prefix, "Exception in thread \"%s\" ", text != NULL ? text : "?");
	free(text);
	print_stack_trace(vm, ex, prefix);
	if (t == vm->threads[0]) vm->exit_code = 1;
	thread_end(vm, t);
}

/* Resolves a handler's catch type without throwing; NULL if it can't load. */
static Class *catch_class(VM *vm, Class *cur, u16 index) {
	if (cur->resolved_flag[index]) return cur->resolved[index].cls;
	Utf8 u = cur->cf->cp[cur->cf->cp[index].u.index].u.utf8;
	Class *c;
	if (class_load(vm, sym_utf8(vm, u), &c) != JME_OK) return NULL;
	cur->resolved[index].cls = c;
	cur->resolved_flag[index] = 1;
	return c;
}

/* The exception initializer failed: wrap anything but an Error. */
static void wrap_init_failure(VM *vm, Thread *t) {
	if (class_is_subclass(class_of(vm, t->exception), vm->c_error)) return;
	Class *c;
	if (class_load(vm, "java/lang/ExceptionInInitializerError", &c) != JME_OK) return;
	Ref e = new_object(vm, c);
	if (e == 0) return;
	ROOT(vm, e);
	fill_stack_trace(vm, t, e, 0);
	UNROOT(vm, 1);
	t->exception = e;
}

/*
 * Finds a handler for t->exception, unwinding frames as needed. Returns 1
 * with the handler's frame ready to run, or 0 when the thread has ended.
 */
static int handle_exception(VM *vm, Thread *t) {
	for (;;) {
		Frame *f = &t->frames[t->frame_count - 1];
		const CodeAttr *code = f->method->code;
		for (u32 i = 0; i < code->handler_count; i++) {
			const ExceptionEntry *h = &code->handlers[i];
			if (f->pc < h->start_pc || f->pc >= h->end_pc) continue;
			if (h->catch_type != 0) {
				Class *cc = catch_class(vm, f->method->owner, h->catch_type);
				if (cc == NULL || !class_is_assignable(class_of(vm, t->exception), cc)) continue;
			}
			u32 base = f->locals + code->max_locals;
			t->slots[base] = t->exception;
			t->tags[base] = 1;
			t->exception = 0;
			f->pc = h->handler_pc;
			f->sp = base + 1;
			t->sp = f->sp;
			return 1;
		}
		if (f->lock != 0) monitor_exit(vm, t, f->lock);
		u32 flags = f->flags;
		Class *init_class = f->init_class;
		t->frame_count--;
		if (t->frame_count == 0) {
			uncaught(vm, t);
			return 0;
		}
		Frame *c = &t->frames[t->frame_count - 1];
		t->sp = c->sp;
		if (flags & F_CLINIT) {
			init_class->state = CS_ERROR;
			class_init_finished(vm, init_class);
			wrap_init_failure(vm, t);
		}
	}
}

/* ---- invocation ---- */

enum { ACT_RELOAD, ACT_THROW, ACT_YIELD };

static NativeResult run_native(VM *vm, Thread *t, Method *m, Slot *args, Value *ret) {
	return m->host_native != NULL ? host_native_call(vm, t, m, args, ret) : m->native(vm, t, args, ret);
}

static NativeResult call_native(VM *vm, Thread *t, Method *m, Value *ret) {
	if (m->native == NULL && m->host_native == NULL) {
		/* the host's natives take precedence over the built-in ones */
		m->host_native = host_native_find(vm, m);
		if (m->host_native == NULL) m->native = natives_find(m);
		if (m->host_native == NULL && m->native == NULL) {
			char msg[256];
			snprintf(msg, sizeof msg, "%s.%s%s", m->owner->name, m->name, m->desc);
			throw_new(vm, "java/lang/UnsatisfiedLinkError", msg);
			return N_THROW;
		}
	}
	Slot *args = t->slots + t->sp - m->arg_slots;
	if (!(m->access & ACC_SYNCHRONIZED)) return run_native(vm, t, m, args, ret);
	Ref lock = (m->access & ACC_STATIC) ? class_mirror(vm, m->owner) : args[0];
	if (lock == 0) return N_THROW;
	if (!monitor_enter(vm, t, lock)) return N_REEXECUTE; /* blocked */
	ROOT(vm, lock);
	NativeResult r = run_native(vm, t, m, args, ret);
	monitor_exit(vm, t, lock);
	UNROOT(vm, 1);
	return r;
}

/* Invokes m, whose arguments are on top of the current frame's stack. */
static int invoke(VM *vm, Thread *t, Method *m) {
	vm->ticks++;
	if (!(m->access & ACC_NATIVE)) return frame_push(vm, t, m, F_ADVANCE) ? ACT_RELOAD : ACT_THROW;

	Value ret;
	memset(&ret, 0, sizeof ret);
	NativeResult r = call_native(vm, t, m, &ret);
	if (r == N_THROW) return ACT_THROW;
	if (r == N_REEXECUTE) return ACT_RELOAD;

	Frame *c = &t->frames[t->frame_count - 1];
	u32 base = t->sp - m->arg_slots;
	Slot *s = &t->slots[base];
	u8 *tg = &t->tags[base];
	u32 n = 0;
	switch (m->ret_type) {
	case 'V': break;
	case 'J': case 'D':
		memcpy(s, &ret.j, 8);
		tg[0] = tg[1] = 0;
		n = 2;
		break;
	case 'L':
		s[0] = ret.r;
		tg[0] = 1;
		n = 1;
		break;
	default: /* I and F share the 32 bits */
		memcpy(s, &ret.i, 4);
		tg[0] = 0;
		n = 1;
		break;
	}
	c->sp = base + n;
	t->sp = c->sp;
	if (t->invoke_after != NULL) {
		/* Class.newInstance: the result is on the stack; run the constructor
		 * on it. The caller moves past its invoke when the constructor
		 * returns, so an exception from it is caught at the invoke. */
		Method *ctor = t->invoke_after;
		t->invoke_after = NULL;
		t->slots[t->sp] = t->invoke_arg;
		t->tags[t->sp] = 1;
		t->sp++;
		c->sp = t->sp;
		t->invoke_arg = 0;
		return frame_push(vm, t, ctor, F_ADVANCE) ? ACT_RELOAD : ACT_THROW;
	}
	c->pc += invoke_length(c);
	return r == N_YIELD ? ACT_YIELD : ACT_RELOAD;
}

/* ---- conversions with Java semantics ---- */

static i32 f2i(float v) {
	if (v != v) return 0;
	if (v >= 2147483648.0f) return INT32_MAX;
	if (v <= -2147483648.0f) return INT32_MIN;
	return (i32)v;
}

static i64 f2l(float v) {
	if (v != v) return 0;
	if (v >= 9223372036854775808.0f) return INT64_MAX;
	if (v <= -9223372036854775808.0f) return INT64_MIN;
	return (i64)v;
}

static i32 d2i(double v) {
	if (v != v) return 0;
	if (v >= 2147483648.0) return INT32_MAX;
	if (v <= -2147483648.0) return INT32_MIN;
	return (i32)v;
}

static i64 d2l(double v) {
	if (v != v) return 0;
	if (v >= 9223372036854775808.0) return INT64_MAX;
	if (v <= -9223372036854775808.0) return INT64_MIN;
	return (i64)v;
}

static i32 fcmp(float a, float b, i32 nan) {
	if (a > b) return 1;
	if (a < b) return -1;
	if (a == b) return 0;
	return nan;
}

static i32 dcmp(double a, double b, i32 nan) {
	if (a > b) return 1;
	if (a < b) return -1;
	if (a == b) return 0;
	return nan;
}

/* new int[a][b][c]: every count was checked to be >= 0. */
static Ref multi_new(VM *vm, Class *c, const i32 *counts, u32 dims) {
	Ref a = new_array(vm, c, counts[0]);
	if (a == 0 || dims == 1) return a;
	ROOT(vm, a);
	for (i32 i = 0; i < counts[0]; i++) {
		Ref sub = multi_new(vm, c->component, counts + 1, dims - 1);
		if (sub == 0) {
			UNROOT(vm, 1);
			return 0;
		}
		REF_ELEMS(vm, a)[i] = sub;
	}
	UNROOT(vm, 1);
	return a;
}

/* ---- the loop ---- */

#define U1(p) (code[(p)])
#define U2(p) ((u16)(code[(p)] << 8 | code[(p) + 1]))
#define S2(p) ((i16)U2(p))
#define S4(p) ((i32)((u32)code[(p)] << 24 | (u32)code[(p) + 1] << 16 | (u32)code[(p) + 2] << 8 | code[(p) + 3]))

#define TAG(p) (tags[(p) - slots])
#define PUSHI(v) do { *sp = (Slot)(i32)(v); TAG(sp) = 0; sp++; } while (0)
#define PUSHR(v) do { *sp = (v); TAG(sp) = 1; sp++; } while (0)
#define PUSHF(v) do { float f_ = (v); memcpy(sp, &f_, 4); TAG(sp) = 0; sp++; } while (0)
#define PUSHJ(v) do { slot_set_long(sp, (v)); TAG(sp) = 0; TAG(sp + 1) = 0; sp += 2; } while (0)
#define PUSHD(v) do { double d_ = (v); memcpy(sp, &d_, 8); TAG(sp) = 0; TAG(sp + 1) = 0; sp += 2; } while (0)
#define POPI() ((i32)*--sp)
#define POPR() (*--sp)
#define POPF() (sp -= 1, slot_get_float(sp))
#define POPJ() (sp -= 2, slot_get_long(sp))
#define POPD() (sp -= 2, slot_get_double(sp))

#define LOAD_FRAME() do { \
		f = &t->frames[t->frame_count - 1]; \
		m = f->method; \
		cls = m->owner; \
		code = m->code->code; \
		pc = f->pc; \
		locals = slots + f->locals; \
		sp = slots + f->sp; \
	} while (0)
#define SAVE() do { f->pc = pc; f->sp = (u32)(sp - slots); t->sp = f->sp; } while (0)
#define THROW(cls_name, msg) do { SAVE(); throw_new(vm, (cls_name), (msg)); goto exception; } while (0)
#define THROW_NPE() THROW("java/lang/NullPointerException", NULL)
#define BRANCH(off) do { \
		i32 o_ = (off); \
		pc = (u32)((i32)pc + o_); \
		if (o_ <= 0 && ++vm->ticks >= limit) { SAVE(); return; } \
	} while (0)
/* After an invoke, return or class initialization changed the frames. */
#define AFTER(act) do { \
		int a_ = (act); \
		if (a_ == ACT_THROW) goto exception; \
		if (t->frame_count == 0 || t->state != TS_RUNNABLE) return; \
		LOAD_FRAME(); \
		if (a_ == ACT_YIELD || vm->ticks >= limit) return; \
	} while (0)
/* Makes sure class c is initialized; may run <clinit> first. */
#define NEED_INIT(c) do { \
		if ((c)->state != CS_INITIALIZED) { \
			SAVE(); \
			int r_ = class_init(vm, t, (c)); \
			if (r_ == INIT_THROWN) goto exception; \
			if (r_ == INIT_BLOCKED) return; \
			if (r_ == INIT_PUSHED) { LOAD_FRAME(); goto next; } \
		} \
	} while (0)

#define ARRAY_CHECK(arr, index) do { \
		if ((arr) == 0) THROW_NPE(); \
		if ((u32)(index) >= ARRAY_LEN(vm, (arr))) { SAVE(); throw_index(vm, (index)); goto exception; } \
	} while (0)

void interp_run(VM *vm, Thread *t, u64 limit) {
	Slot *const slots = t->slots;
	u8 *const tags = t->tags;
	Frame *f;
	Method *m;
	Class *cls;
	const u8 *code;
	u32 pc;
	Slot *locals, *sp;

	vm->current = t;
	if (t->exception != 0) goto exception_unwind;
	LOAD_FRAME();

	for (;;) {
	next:;
		u8 op = code[pc];
		switch (op) {
		case OP_NOP: pc++; break;
		case OP_ACONST_NULL: PUSHR(0); pc++; break;
		case OP_ICONST_M1: case OP_ICONST_0: case OP_ICONST_1: case OP_ICONST_2:
		case OP_ICONST_3: case OP_ICONST_4: case OP_ICONST_5:
			PUSHI(op - OP_ICONST_0); pc++; break;
		case OP_LCONST_0: case OP_LCONST_1: PUSHJ(op - OP_LCONST_0); pc++; break;
		case OP_FCONST_0: case OP_FCONST_1: case OP_FCONST_2: PUSHF((float)(op - OP_FCONST_0)); pc++; break;
		case OP_DCONST_0: case OP_DCONST_1: PUSHD((double)(op - OP_DCONST_0)); pc++; break;
		case OP_BIPUSH: PUSHI((i8)U1(pc + 1)); pc += 2; break;
		case OP_SIPUSH: PUSHI(S2(pc + 1)); pc += 3; break;

		case OP_LDC: case OP_LDC_W: {
			u16 idx = op == OP_LDC ? U1(pc + 1) : U2(pc + 1);
			const CpEntry *e = &cls->cf->cp[idx];
			if (e->tag == CP_INTEGER || e->tag == CP_FLOAT) {
				*sp = e->u.bits32;
				TAG(sp) = 0;
				sp++;
			} else if (e->tag == CP_STRING) {
				Ref s = cls->resolved_flag[idx] ? cls->resolved[idx].str : 0;
				if (s == 0) {
					SAVE();
					s = resolve_string(vm, cls, idx);
					if (s == 0) goto exception;
				}
				PUSHR(s);
			} else {
				SAVE();
				Class *c = resolve_class(vm, cls, idx);
				if (c == NULL) goto exception;
				Ref mirror = class_mirror(vm, c);
				if (mirror == 0) goto exception;
				PUSHR(mirror);
			}
			pc += op == OP_LDC ? 2 : 3;
			break;
		}
		case OP_LDC2_W: {
			const CpEntry *e = &cls->cf->cp[U2(pc + 1)];
			memcpy(sp, &e->u.bits64, 8);
			TAG(sp) = 0;
			TAG(sp + 1) = 0;
			sp += 2;
			pc += 3;
			break;
		}

		/* ---- locals ---- */
		case OP_ILOAD: case OP_FLOAD: PUSHI(locals[U1(pc + 1)]); pc += 2; break;
		case OP_ALOAD: { u32 i = U1(pc + 1); *sp = locals[i]; TAG(sp) = TAG(locals + i); sp++; pc += 2; break; }
		case OP_LLOAD: case OP_DLOAD: {
			u32 i = U1(pc + 1);
			sp[0] = locals[i];
			sp[1] = locals[i + 1];
			TAG(sp) = 0;
			TAG(sp + 1) = 0;
			sp += 2;
			pc += 2;
			break;
		}
		case OP_ILOAD_0: case OP_ILOAD_1: case OP_ILOAD_2: case OP_ILOAD_3:
			PUSHI(locals[op - OP_ILOAD_0]); pc++; break;
		case OP_FLOAD_0: case OP_FLOAD_1: case OP_FLOAD_2: case OP_FLOAD_3:
			PUSHI(locals[op - OP_FLOAD_0]); pc++; break;
		case OP_ALOAD_0: case OP_ALOAD_1: case OP_ALOAD_2: case OP_ALOAD_3: {
			u32 i = op - OP_ALOAD_0;
			*sp = locals[i];
			TAG(sp) = TAG(locals + i);
			sp++;
			pc++;
			break;
		}
		case OP_LLOAD_0: case OP_LLOAD_1: case OP_LLOAD_2: case OP_LLOAD_3:
		case OP_DLOAD_0: case OP_DLOAD_1: case OP_DLOAD_2: case OP_DLOAD_3: {
			u32 i = (op - OP_LLOAD_0) % 4;
			sp[0] = locals[i];
			sp[1] = locals[i + 1];
			TAG(sp) = 0;
			TAG(sp + 1) = 0;
			sp += 2;
			pc++;
			break;
		}
		case OP_ISTORE: case OP_FSTORE: {
			u32 i = U1(pc + 1);
			locals[i] = *--sp;
			TAG(locals + i) = 0;
			pc += 2;
			break;
		}
		case OP_ASTORE: {
			/* keeps the tag: astore also stores jsr return addresses */
			u32 i = U1(pc + 1);
			sp--;
			locals[i] = *sp;
			TAG(locals + i) = TAG(sp);
			pc += 2;
			break;
		}
		case OP_LSTORE: case OP_DSTORE: {
			u32 i = U1(pc + 1);
			sp -= 2;
			locals[i] = sp[0];
			locals[i + 1] = sp[1];
			TAG(locals + i) = 0;
			TAG(locals + i + 1) = 0;
			pc += 2;
			break;
		}
		case OP_ISTORE_0: case OP_ISTORE_1: case OP_ISTORE_2: case OP_ISTORE_3:
		case OP_FSTORE_0: case OP_FSTORE_1: case OP_FSTORE_2: case OP_FSTORE_3: {
			u32 i = (op - OP_ISTORE_0) % 4;
			locals[i] = *--sp;
			TAG(locals + i) = 0;
			pc++;
			break;
		}
		case OP_ASTORE_0: case OP_ASTORE_1: case OP_ASTORE_2: case OP_ASTORE_3: {
			u32 i = op - OP_ASTORE_0;
			sp--;
			locals[i] = *sp;
			TAG(locals + i) = TAG(sp);
			pc++;
			break;
		}
		case OP_LSTORE_0: case OP_LSTORE_1: case OP_LSTORE_2: case OP_LSTORE_3:
		case OP_DSTORE_0: case OP_DSTORE_1: case OP_DSTORE_2: case OP_DSTORE_3: {
			u32 i = (op - OP_LSTORE_0) % 4;
			sp -= 2;
			locals[i] = sp[0];
			locals[i + 1] = sp[1];
			TAG(locals + i) = 0;
			TAG(locals + i + 1) = 0;
			pc++;
			break;
		}

		/* ---- arrays ---- */
		case OP_IALOAD: case OP_FALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHI(((i32 *)ARRAY_PTR(vm, a))[i]);
			pc++;
			break;
		}
		case OP_LALOAD: case OP_DALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHJ(((i64 *)ARRAY_PTR(vm, a))[i]);
			pc++;
			break;
		}
		case OP_AALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHR(REF_ELEMS(vm, a)[i]);
			pc++;
			break;
		}
		case OP_BALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHI(((i8 *)ARRAY_PTR(vm, a))[i]);
			pc++;
			break;
		}
		case OP_CALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHI(((u16 *)ARRAY_PTR(vm, a))[i]);
			pc++;
			break;
		}
		case OP_SALOAD: {
			i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			PUSHI(((i16 *)ARRAY_PTR(vm, a))[i]);
			pc++;
			break;
		}
		case OP_IASTORE: case OP_FASTORE: {
			Slot v = *--sp; i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			((Slot *)ARRAY_PTR(vm, a))[i] = v;
			pc++;
			break;
		}
		case OP_LASTORE: case OP_DASTORE: {
			i64 v = POPJ(); i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			((i64 *)ARRAY_PTR(vm, a))[i] = v;
			pc++;
			break;
		}
		case OP_AASTORE: {
			Ref v = POPR(); i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			if (v != 0 && !class_is_assignable(class_of(vm, v), class_of(vm, a)->component)) {
				char name[256];
				class_java_name(class_of(vm, v), name, sizeof name);
				THROW("java/lang/ArrayStoreException", name);
			}
			REF_ELEMS(vm, a)[i] = v;
			pc++;
			break;
		}
		case OP_BASTORE: {
			i32 v = POPI(); i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			if (class_of(vm, a)->elem_type == T_BOOLEAN) v &= 1;
			((i8 *)ARRAY_PTR(vm, a))[i] = (i8)v;
			pc++;
			break;
		}
		case OP_CASTORE: case OP_SASTORE: {
			i32 v = POPI(); i32 i = POPI(); Ref a = POPR();
			ARRAY_CHECK(a, i);
			((u16 *)ARRAY_PTR(vm, a))[i] = (u16)v;
			pc++;
			break;
		}

		/* ---- stack ---- */
		case OP_POP: sp--; pc++; break;
		case OP_POP2: sp -= 2; pc++; break;
		case OP_DUP: sp[0] = sp[-1]; TAG(sp) = TAG(sp - 1); sp++; pc++; break;
		case OP_DUP_X1: {
			/* ..., b, a -> ..., a, b, a */
			Slot a = sp[-1], b = sp[-2];
			u8 ta = TAG(sp - 1), tb = TAG(sp - 2);
			sp[-2] = a; TAG(sp - 2) = ta;
			sp[-1] = b; TAG(sp - 1) = tb;
			sp[0] = a; TAG(sp) = ta;
			sp++;
			pc++;
			break;
		}
		case OP_DUP_X2: {
			/* ..., c, b, a -> ..., a, c, b, a */
			Slot a = sp[-1], b = sp[-2], c = sp[-3];
			u8 ta = TAG(sp - 1), tb = TAG(sp - 2), tc = TAG(sp - 3);
			sp[-3] = a; TAG(sp - 3) = ta;
			sp[-2] = c; TAG(sp - 2) = tc;
			sp[-1] = b; TAG(sp - 1) = tb;
			sp[0] = a; TAG(sp) = ta;
			sp++;
			pc++;
			break;
		}
		case OP_DUP2: {
			sp[0] = sp[-2]; TAG(sp) = TAG(sp - 2);
			sp[1] = sp[-1]; TAG(sp + 1) = TAG(sp - 1);
			sp += 2;
			pc++;
			break;
		}
		case OP_DUP2_X1: {
			/* ..., c, b, a -> ..., b, a, c, b, a */
			Slot a = sp[-1], b = sp[-2], c = sp[-3];
			u8 ta = TAG(sp - 1), tb = TAG(sp - 2), tc = TAG(sp - 3);
			sp[-3] = b; TAG(sp - 3) = tb;
			sp[-2] = a; TAG(sp - 2) = ta;
			sp[-1] = c; TAG(sp - 1) = tc;
			sp[0] = b; TAG(sp) = tb;
			sp[1] = a; TAG(sp + 1) = ta;
			sp += 2;
			pc++;
			break;
		}
		case OP_DUP2_X2: {
			/* ..., d, c, b, a -> ..., b, a, d, c, b, a */
			Slot a = sp[-1], b = sp[-2], c = sp[-3], d = sp[-4];
			u8 ta = TAG(sp - 1), tb = TAG(sp - 2), tc = TAG(sp - 3), td = TAG(sp - 4);
			sp[-4] = b; TAG(sp - 4) = tb;
			sp[-3] = a; TAG(sp - 3) = ta;
			sp[-2] = d; TAG(sp - 2) = td;
			sp[-1] = c; TAG(sp - 1) = tc;
			sp[0] = b; TAG(sp) = tb;
			sp[1] = a; TAG(sp + 1) = ta;
			sp += 2;
			pc++;
			break;
		}
		case OP_SWAP: {
			Slot a = sp[-1];
			u8 ta = TAG(sp - 1);
			sp[-1] = sp[-2]; TAG(sp - 1) = TAG(sp - 2);
			sp[-2] = a; TAG(sp - 2) = ta;
			pc++;
			break;
		}

		/* ---- int arithmetic (wrapping) ---- */
		case OP_IADD: { u32 b = (u32)POPI(), a = (u32)POPI(); PUSHI(a + b); pc++; break; }
		case OP_ISUB: { u32 b = (u32)POPI(), a = (u32)POPI(); PUSHI(a - b); pc++; break; }
		case OP_IMUL: { u32 b = (u32)POPI(), a = (u32)POPI(); PUSHI(a * b); pc++; break; }
		case OP_IDIV: case OP_IREM: {
			i32 b = POPI(), a = POPI();
			if (b == 0) THROW("java/lang/ArithmeticException", "/ by zero");
			if (b == -1) PUSHI(op == OP_IDIV ? (i32)(0u - (u32)a) : 0);
			else PUSHI(op == OP_IDIV ? a / b : a % b);
			pc++;
			break;
		}
		case OP_INEG: { u32 a = (u32)POPI(); PUSHI(0u - a); pc++; break; }
		case OP_ISHL: { i32 b = POPI(); u32 a = (u32)POPI(); PUSHI(a << (b & 31)); pc++; break; }
		case OP_ISHR: { i32 b = POPI(); i32 a = POPI(); PUSHI(a >> (b & 31)); pc++; break; }
		case OP_IUSHR: { i32 b = POPI(); u32 a = (u32)POPI(); PUSHI(a >> (b & 31)); pc++; break; }
		case OP_IAND: { i32 b = POPI(), a = POPI(); PUSHI(a & b); pc++; break; }
		case OP_IOR: { i32 b = POPI(), a = POPI(); PUSHI(a | b); pc++; break; }
		case OP_IXOR: { i32 b = POPI(), a = POPI(); PUSHI(a ^ b); pc++; break; }
		case OP_IINC: {
			u32 i = U1(pc + 1);
			locals[i] = (u32)locals[i] + (u32)(i32)(i8)U1(pc + 2);
			TAG(locals + i) = 0;
			pc += 3;
			break;
		}

		/* ---- long arithmetic ---- */
		case OP_LADD: { u64 b = (u64)POPJ(), a = (u64)POPJ(); PUSHJ((i64)(a + b)); pc++; break; }
		case OP_LSUB: { u64 b = (u64)POPJ(), a = (u64)POPJ(); PUSHJ((i64)(a - b)); pc++; break; }
		case OP_LMUL: { u64 b = (u64)POPJ(), a = (u64)POPJ(); PUSHJ((i64)(a * b)); pc++; break; }
		case OP_LDIV: case OP_LREM: {
			i64 b = POPJ(), a = POPJ();
			if (b == 0) THROW("java/lang/ArithmeticException", "/ by zero");
			if (b == -1) PUSHJ(op == OP_LDIV ? (i64)(0u - (u64)a) : 0);
			else PUSHJ(op == OP_LDIV ? a / b : a % b);
			pc++;
			break;
		}
		case OP_LNEG: { u64 a = (u64)POPJ(); PUSHJ((i64)(0u - a)); pc++; break; }
		case OP_LSHL: { i32 b = POPI(); u64 a = (u64)POPJ(); PUSHJ((i64)(a << (b & 63))); pc++; break; }
		case OP_LSHR: { i32 b = POPI(); i64 a = POPJ(); PUSHJ(a >> (b & 63)); pc++; break; }
		case OP_LUSHR: { i32 b = POPI(); u64 a = (u64)POPJ(); PUSHJ((i64)(a >> (b & 63))); pc++; break; }
		case OP_LAND: { i64 b = POPJ(), a = POPJ(); PUSHJ(a & b); pc++; break; }
		case OP_LOR: { i64 b = POPJ(), a = POPJ(); PUSHJ(a | b); pc++; break; }
		case OP_LXOR: { i64 b = POPJ(), a = POPJ(); PUSHJ(a ^ b); pc++; break; }

		/* ---- floating point ---- */
		case OP_FADD: { float b = POPF(), a = POPF(); PUSHF(a + b); pc++; break; }
		case OP_FSUB: { float b = POPF(), a = POPF(); PUSHF(a - b); pc++; break; }
		case OP_FMUL: { float b = POPF(), a = POPF(); PUSHF(a * b); pc++; break; }
		case OP_FDIV: { float b = POPF(), a = POPF(); PUSHF(a / b); pc++; break; }
		case OP_FREM: { float b = POPF(), a = POPF(); PUSHF(fmodf(a, b)); pc++; break; }
		case OP_FNEG: { float a = POPF(); PUSHF(-a); pc++; break; }
		case OP_DADD: { double b = POPD(), a = POPD(); PUSHD(a + b); pc++; break; }
		case OP_DSUB: { double b = POPD(), a = POPD(); PUSHD(a - b); pc++; break; }
		case OP_DMUL: { double b = POPD(), a = POPD(); PUSHD(a * b); pc++; break; }
		case OP_DDIV: { double b = POPD(), a = POPD(); PUSHD(a / b); pc++; break; }
		case OP_DREM: { double b = POPD(), a = POPD(); PUSHD(fmod(a, b)); pc++; break; }
		case OP_DNEG: { double a = POPD(); PUSHD(-a); pc++; break; }

		/* ---- conversions ---- */
		case OP_I2L: { i32 a = POPI(); PUSHJ(a); pc++; break; }
		case OP_I2F: { i32 a = POPI(); PUSHF((float)a); pc++; break; }
		case OP_I2D: { i32 a = POPI(); PUSHD(a); pc++; break; }
		case OP_L2I: { i64 a = POPJ(); PUSHI((i32)(u32)(u64)a); pc++; break; }
		case OP_L2F: { i64 a = POPJ(); PUSHF((float)a); pc++; break; }
		case OP_L2D: { i64 a = POPJ(); PUSHD((double)a); pc++; break; }
		case OP_F2I: { float a = POPF(); PUSHI(f2i(a)); pc++; break; }
		case OP_F2L: { float a = POPF(); PUSHJ(f2l(a)); pc++; break; }
		case OP_F2D: { float a = POPF(); PUSHD(a); pc++; break; }
		case OP_D2I: { double a = POPD(); PUSHI(d2i(a)); pc++; break; }
		case OP_D2L: { double a = POPD(); PUSHJ(d2l(a)); pc++; break; }
		case OP_D2F: { double a = POPD(); PUSHF((float)a); pc++; break; }
		case OP_I2B: { i32 a = POPI(); PUSHI((i8)a); pc++; break; }
		case OP_I2C: { i32 a = POPI(); PUSHI((u16)a); pc++; break; }
		case OP_I2S: { i32 a = POPI(); PUSHI((i16)a); pc++; break; }

		/* ---- comparisons and branches ---- */
		case OP_LCMP: { i64 b = POPJ(), a = POPJ(); PUSHI(a > b ? 1 : a < b ? -1 : 0); pc++; break; }
		case OP_FCMPL: case OP_FCMPG: {
			float b = POPF(), a = POPF();
			PUSHI(fcmp(a, b, op == OP_FCMPL ? -1 : 1));
			pc++;
			break;
		}
		case OP_DCMPL: case OP_DCMPG: {
			double b = POPD(), a = POPD();
			PUSHI(dcmp(a, b, op == OP_DCMPL ? -1 : 1));
			pc++;
			break;
		}
		case OP_IFEQ: case OP_IFNE: case OP_IFLT: case OP_IFGE: case OP_IFGT: case OP_IFLE: {
			i32 a = POPI();
			int taken;
			switch (op) {
			case OP_IFEQ: taken = a == 0; break;
			case OP_IFNE: taken = a != 0; break;
			case OP_IFLT: taken = a < 0; break;
			case OP_IFGE: taken = a >= 0; break;
			case OP_IFGT: taken = a > 0; break;
			default: taken = a <= 0; break;
			}
			if (taken) BRANCH(S2(pc + 1)); else pc += 3;
			break;
		}
		case OP_IF_ICMPEQ: case OP_IF_ICMPNE: case OP_IF_ICMPLT:
		case OP_IF_ICMPGE: case OP_IF_ICMPGT: case OP_IF_ICMPLE: {
			i32 b = POPI(), a = POPI();
			int taken;
			switch (op) {
			case OP_IF_ICMPEQ: taken = a == b; break;
			case OP_IF_ICMPNE: taken = a != b; break;
			case OP_IF_ICMPLT: taken = a < b; break;
			case OP_IF_ICMPGE: taken = a >= b; break;
			case OP_IF_ICMPGT: taken = a > b; break;
			default: taken = a <= b; break;
			}
			if (taken) BRANCH(S2(pc + 1)); else pc += 3;
			break;
		}
		case OP_IF_ACMPEQ: case OP_IF_ACMPNE: {
			Ref b = POPR(), a = POPR();
			if ((a == b) == (op == OP_IF_ACMPEQ)) BRANCH(S2(pc + 1)); else pc += 3;
			break;
		}
		case OP_IFNULL: case OP_IFNONNULL: {
			Ref a = POPR();
			if ((a == 0) == (op == OP_IFNULL)) BRANCH(S2(pc + 1)); else pc += 3;
			break;
		}
		case OP_GOTO: BRANCH(S2(pc + 1)); break;
		case OP_GOTO_W: BRANCH(S4(pc + 1)); break;
		case OP_JSR: PUSHI(pc + 3); BRANCH(S2(pc + 1)); break;
		case OP_JSR_W: PUSHI(pc + 5); BRANCH(S4(pc + 1)); break;
		case OP_RET: {
			u32 target = locals[U1(pc + 1)];
			if (target >= m->code->code_len) THROW("java/lang/VerifyError", "bad return address");
			BRANCH((i32)target - (i32)pc);
			break;
		}
		case OP_TABLESWITCH: {
			i32 key = POPI();
			u32 p = (pc + 4) & ~3u;
			i32 def = S4(p), low = S4(p + 4), high = S4(p + 8);
			i32 off = key < low || key > high ? def : S4(p + 12 + 4 * (u32)(key - low));
			BRANCH(off);
			break;
		}
		case OP_LOOKUPSWITCH: {
			i32 key = POPI();
			u32 p = (pc + 4) & ~3u;
			i32 off = S4(p), n = S4(p + 4);
			/* pairs are sorted by key */
			i32 lo = 0, hi = n - 1;
			while (lo <= hi) {
				i32 mid = lo + (hi - lo) / 2;
				i32 k = S4(p + 8 + 8 * (u32)mid);
				if (k == key) {
					off = S4(p + 12 + 8 * (u32)mid);
					break;
				}
				if (k < key) lo = mid + 1; else hi = mid - 1;
			}
			BRANCH(off);
			break;
		}

		/* ---- returns ---- */
		case OP_IRETURN: case OP_FRETURN: case OP_ARETURN: case OP_LRETURN: case OP_DRETURN: case OP_RETURN: {
			u32 n = op == OP_RETURN ? 0 : (op == OP_LRETURN || op == OP_DRETURN) ? 2 : 1;
			SAVE();
			if (!frame_return(vm, t, sp - n, n)) return;
			LOAD_FRAME();
			break;
		}

		/* ---- fields ---- */
		case OP_GETSTATIC: case OP_PUTSTATIC: {
			u16 idx = U2(pc + 1);
			Field *fl = cls->resolved_flag[idx] ? cls->resolved[idx].field : NULL;
			if (fl == NULL) {
				SAVE();
				fl = resolve_field(vm, cls, idx, 1);
				if (fl == NULL) goto exception;
			}
			NEED_INIT(fl->owner);
			Slot *p = &fl->owner->statics[fl->offset];
			if (op == OP_GETSTATIC) {
				if (fl->type == 'J' || fl->type == 'D') {
					sp[0] = p[0];
					sp[1] = p[1];
					TAG(sp) = TAG(sp + 1) = 0;
					sp += 2;
				} else {
					*sp = *p;
					TAG(sp) = fl->type == 'L' || fl->type == '[';
					sp++;
				}
			} else if (fl->type == 'J' || fl->type == 'D') {
				sp -= 2;
				p[0] = sp[0];
				p[1] = sp[1];
			} else {
				Slot v = *--sp;
				switch (fl->type) {
				case 'Z': v &= 1; break;
				case 'B': v = (Slot)(i32)(i8)v; break;
				case 'C': v = (u16)v; break;
				case 'S': v = (Slot)(i32)(i16)v; break;
				default: break;
				}
				*p = v;
			}
			pc += 3;
			break;
		}
		case OP_GETFIELD: case OP_PUTFIELD: {
			u16 idx = U2(pc + 1);
			Field *fl = cls->resolved_flag[idx] ? cls->resolved[idx].field : NULL;
			if (fl == NULL) {
				SAVE();
				fl = resolve_field(vm, cls, idx, 0);
				if (fl == NULL) goto exception;
			}
			int wide = fl->type == 'J' || fl->type == 'D';
			if (op == OP_GETFIELD) {
				Ref o = POPR();
				if (o == 0) THROW_NPE();
				void *p = FIELD_PTR(vm, o, fl->offset);
				if (wide) PUSHJ(*(i64 *)p);
				else if (fl->type == 'L' || fl->type == '[') PUSHR(*(Ref *)p);
				else PUSHI(*(i32 *)p);
			} else {
				Ref o = sp[wide ? -3 : -2];
				if (o == 0) THROW_NPE();
				void *p = FIELD_PTR(vm, o, fl->offset);
				if (wide) {
					*(i64 *)p = POPJ();
				} else {
					Slot v = *--sp;
					switch (fl->type) {
					case 'Z': v &= 1; break;
					case 'B': v = (Slot)(i32)(i8)v; break;
					case 'C': v = (u16)v; break;
					case 'S': v = (Slot)(i32)(i16)v; break;
					default: break;
					}
					*(Slot *)p = v;
				}
				sp--;
			}
			pc += 3;
			break;
		}

		/* ---- invocation ---- */
		case OP_INVOKEVIRTUAL: case OP_INVOKESPECIAL: case OP_INVOKESTATIC: case OP_INVOKEINTERFACE: {
			u16 idx = U2(pc + 1);
			SAVE();
			Method *rm = cls->resolved_flag[idx] ? cls->resolved[idx].method
					: resolve_method(vm, cls, idx, op == OP_INVOKEINTERFACE);
			if (rm == NULL) goto exception;
			if (((rm->access & ACC_STATIC) != 0) != (op == OP_INVOKESTATIC))
				THROW("java/lang/IncompatibleClassChangeError", rm->name);
			Method *target = rm;
			if (op == OP_INVOKESTATIC) {
				NEED_INIT(rm->owner);
			} else {
				Ref recv = sp[-(i32)rm->arg_slots];
				if (recv == 0) THROW_NPE();
				if (op == OP_INVOKESPECIAL) {
					if (rm->name != vm->sym.init && !(rm->access & ACC_PRIVATE) && (cls->access & ACC_SUPER)
							&& cls->super != NULL && rm->owner != cls && class_is_subclass(cls->super, rm->owner))
						target = class_find_method(cls->super, rm->name, rm->desc);
				} else if (!(rm->access & ACC_PRIVATE)) {
					Class *rc = class_of(vm, recv);
					if (op == OP_INVOKEVIRTUAL && rm->vtable_index >= 0) {
						if ((u32)rm->vtable_index >= rc->vtable_len)
							THROW("java/lang/IncompatibleClassChangeError", rm->name);
						target = rc->vtable[rm->vtable_index];
					} else {
						target = class_find_virtual(rc, rm->name, rm->desc);
					}
				}
				if (target == NULL || (target->access & ACC_ABSTRACT))
					THROW("java/lang/AbstractMethodError", rm->name);
			}
			AFTER(invoke(vm, t, target));
			break;
		}
		case OP_INVOKEDYNAMIC:
			THROW("java/lang/VerifyError", "invokedynamic");

		/* ---- objects ---- */
		case OP_NEW: {
			u16 idx = U2(pc + 1);
			SAVE();
			Class *c = resolve_class(vm, cls, idx);
			if (c == NULL) goto exception;
			if (c->access & (ACC_INTERFACE | ACC_ABSTRACT)) THROW("java/lang/InstantiationError", c->name);
			NEED_INIT(c);
			Ref o = new_object(vm, c);
			if (o == 0) goto exception;
			PUSHR(o);
			pc += 3;
			break;
		}
		case OP_NEWARRAY: {
			SAVE();
			Class *c = primitive_array_class(vm, U1(pc + 1));
			if (c == NULL) goto exception;
			i32 n = (i32)sp[-1];
			Ref a = new_array(vm, c, n);
			if (a == 0) goto exception;
			sp[-1] = a;
			TAG(sp - 1) = 1;
			pc += 2;
			break;
		}
		case OP_ANEWARRAY: {
			SAVE();
			Class *comp = resolve_class(vm, cls, U2(pc + 1));
			if (comp == NULL) goto exception;
			Class *c = array_class_of(vm, comp);
			if (c == NULL) goto exception;
			Ref a = new_array(vm, c, (i32)sp[-1]);
			if (a == 0) goto exception;
			sp[-1] = a;
			TAG(sp - 1) = 1;
			pc += 3;
			break;
		}
		case OP_MULTIANEWARRAY: {
			u32 dims = U1(pc + 3);
			SAVE();
			Class *c = resolve_class(vm, cls, U2(pc + 1));
			if (c == NULL) goto exception;
			i32 counts[255];
			for (u32 i = 0; i < dims; i++) {
				counts[i] = (i32)sp[(i32)i - (i32)dims];
				if (counts[i] < 0) {
					char msg[16];
					snprintf(msg, sizeof msg, "%d", counts[i]);
					THROW("java/lang/NegativeArraySizeException", msg);
				}
			}
			Class *k = c;
			for (u32 i = 1; i < dims && k != NULL; i++) k = k->component;
			if (k == NULL || k->kind != CK_ARRAY) THROW("java/lang/VerifyError", "multianewarray");
			Ref a = multi_new(vm, c, counts, dims);
			if (a == 0) goto exception;
			sp -= dims;
			PUSHR(a);
			pc += 4;
			break;
		}
		case OP_ARRAYLENGTH: {
			Ref a = POPR();
			if (a == 0) THROW_NPE();
			PUSHI(ARRAY_LEN(vm, a));
			pc++;
			break;
		}
		case OP_ATHROW: {
			Ref e = sp[-1];
			if (e == 0) THROW_NPE();
			SAVE();
			t->exception = e;
			goto exception;
		}
		case OP_CHECKCAST: case OP_INSTANCEOF: {
			u16 idx = U2(pc + 1);
			Class *c = cls->resolved_flag[idx] ? cls->resolved[idx].cls : NULL;
			if (c == NULL) {
				SAVE();
				c = resolve_class(vm, cls, idx);
				if (c == NULL) goto exception;
			}
			Ref o = sp[-1];
			int ok = o != 0 && class_is_assignable(class_of(vm, o), c);
			if (op == OP_INSTANCEOF) {
				sp[-1] = (Slot)ok;
				TAG(sp - 1) = 0;
			} else if (o != 0 && !ok) {
				char from[200], to[200], msg[420];
				class_java_name(class_of(vm, o), from, sizeof from);
				class_java_name(c, to, sizeof to);
				snprintf(msg, sizeof msg, "%s cannot be cast to %s", from, to);
				THROW("java/lang/ClassCastException", msg);
			}
			pc += 3;
			break;
		}
		case OP_MONITORENTER: {
			Ref o = sp[-1];
			if (o == 0) THROW_NPE();
			SAVE();
			if (!monitor_enter(vm, t, o)) return; /* blocked: runs again when granted */
			sp--;
			pc++;
			break;
		}
		case OP_MONITOREXIT: {
			Ref o = sp[-1];
			if (o == 0) THROW_NPE();
			if (!monitor_exit(vm, t, o)) THROW("java/lang/IllegalMonitorStateException", NULL);
			sp--;
			pc++;
			break;
		}

		case OP_WIDE: {
			u8 op2 = U1(pc + 1);
			u32 i = U2(pc + 2);
			switch (op2) {
			case OP_ILOAD: case OP_FLOAD: PUSHI(locals[i]); break;
			case OP_ALOAD: *sp = locals[i]; TAG(sp) = TAG(locals + i); sp++; break;
			case OP_LLOAD: case OP_DLOAD:
				sp[0] = locals[i];
				sp[1] = locals[i + 1];
				TAG(sp) = TAG(sp + 1) = 0;
				sp += 2;
				break;
			case OP_ISTORE: case OP_FSTORE: locals[i] = *--sp; TAG(locals + i) = 0; break;
			case OP_ASTORE: sp--; locals[i] = *sp; TAG(locals + i) = TAG(sp); break;
			case OP_LSTORE: case OP_DSTORE:
				sp -= 2;
				locals[i] = sp[0];
				locals[i + 1] = sp[1];
				TAG(locals + i) = TAG(locals + i + 1) = 0;
				break;
			case OP_IINC:
				locals[i] = (u32)locals[i] + (u32)(i32)S2(pc + 4);
				TAG(locals + i) = 0;
				pc += 2;
				break;
			case OP_RET: {
				u32 target = locals[i];
				if (target >= m->code->code_len) THROW("java/lang/VerifyError", "bad return address");
				BRANCH((i32)target - (i32)pc);
				goto next;
			}
			default: break;
			}
			pc += 4;
			break;
		}

		default: {
			char msg[32];
			snprintf(msg, sizeof msg, "opcode 0x%02x", op);
			THROW("java/lang/VerifyError", msg);
		}
		}
		continue;

	exception:
		/* t->exception is set and the frame's pc is the failing instruction. */
	exception_unwind:
		if (!handle_exception(vm, t)) return;
		LOAD_FRAME();
	}
}
