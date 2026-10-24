/*
 * Snapshots: the whole machine state as bytes.
 *
 * Because the VM keeps no machine pointers in the heap and nothing on the C
 * stack between jme_run_until() calls, saving is a copy of the arena plus
 * small tables. Class metadata is not saved: the snapshot records which
 * classes were loaded, in order, with a hash of their bytes, and loading
 * replays those loads, which gives every class and method the same id again.
 *
 * Layout (little-endian; the heap bytes are the host's, and every supported
 * host is little-endian):
 *
 *   header, 32 bytes: "JMEVMSNP", u32 format, u32 VM version,
 *                     u32 heap_bytes, u32 ticks_per_ms, i64 epoch_ms
 *   sections, each u32 tag + u32 length + payload, in this order:
 *     HEAP  u32 heap_top, then the arena from HEAP_START to heap_top. It
 *           comes first, so its bytes always sit at the same offset, which
 *           keeps rewind deltas small.
 *     CLAS  the class load log: name and hash of the class file bytes
 *     CSTA  per class: init state, statics, mirror, resolved string constants
 *     STRS  the interned string table
 *     MONS  the monitor table
 *     VMST  clock, scheduler, hash sequence, exit state, main method
 *     THRD  threads with their stacks and frames
 *     HOST  the host's registered state (jme_register_state): count, then
 *           name and bytes of each
 *   u64 FNV-1a of everything before it
 *
 * Loading checks the structure thoroughly: every reference points at the
 * start of an object, ids are in range, frames are consistent with their
 * methods and stacks, and every pc is an instruction boundary. Like bytecode
 * before a type verifier exists, the types of references on thread stacks
 * are not checked.
 */
#include <stdlib.h>

#include "vm.h"

#define SNAP_FORMAT 2   /* 2: host wait channels and the HOST section */
#define HEADER_SIZE 32
#define SECTION(a, b, c, d) ((u32)(a) | (u32)(b) << 8 | (u32)(c) << 16 | (u32)(d) << 24)

#define M_NONE 0xFFFFFFFFu
#define M_BOOT 0xFFFFFFFEu
#define M_RUN 0xFFFFFFFDu

static const char MAGIC[8] = {'J', 'M', 'E', 'V', 'M', 'S', 'N', 'P'};

u64 fnv1a64(const void *data, size_t len) {
	const u8 *p = data;
	u64 h = 14695981039346656037ull;
	for (size_t i = 0; i < len; i++) h = (h ^ p[i]) * 1099511628211ull;
	return h;
}

static int little_endian(void) {
	u16 x = 1;
	u8 b;
	memcpy(&b, &x, 1);
	return b == 1;
}

/* ---- writing ---- */

typedef struct Buf {
	u8 *p;
	size_t len, cap;
	int oom;
} Buf;

static void put(Buf *b, const void *data, size_t n) {
	if (b->oom) return;
	if (b->len + n > b->cap) {
		size_t cap = b->cap ? b->cap : 64 * 1024;
		while (cap < b->len + n) cap *= 2;
		u8 *p = realloc(b->p, cap);
		if (p == NULL) {
			b->oom = 1;
			return;
		}
		b->p = p;
		b->cap = cap;
	}
	if (n) memcpy(b->p + b->len, data, n);
	b->len += n;
}

static void put_u8(Buf *b, u32 v) {
	u8 x = (u8)v;
	put(b, &x, 1);
}

static void put_u32(Buf *b, u32 v) {
	u8 x[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
	put(b, x, 4);
}

static void put_u64(Buf *b, u64 v) {
	put_u32(b, (u32)v);
	put_u32(b, (u32)(v >> 32));
}

/* A JmeWriteFn appending to a Buf, for host state. */
static JmeStatus collect_into(void *user, const void *data, size_t len) {
	Buf *b = user;
	put(b, data, len);
	return b->oom ? JME_E_NOMEM : JME_OK;
}

/* Starts a section; section_end() fills in its length. */
static size_t section_begin(Buf *b, u32 tag) {
	put_u32(b, tag);
	put_u32(b, 0);
	return b->len;
}

static void section_end(Buf *b, size_t start) {
	if (b->oom) return;
	u32 n = (u32)(b->len - start);
	u8 x[4] = {(u8)n, (u8)(n >> 8), (u8)(n >> 16), (u8)(n >> 24)};
	memcpy(b->p + start - 4, x, 4);
}

static u32 method_code(VM *vm, const Method *m) {
	if (m == NULL) return M_NONE;
	if (m == vm->boot_method) return M_BOOT;
	if (m == vm->run_method) return M_RUN;
	return m->id;
}

static u32 class_code(const Class *c) {
	return c != NULL ? c->id + 1 : 0;
}

JmeStatus snapshot_build(VM *vm, u8 **out, size_t *len) {
	*out = NULL;
	*len = 0;
	if (!little_endian()) return JME_E_UNSUPPORTED;
	Buf b = {0};
	put(&b, MAGIC, 8);
	put_u32(&b, SNAP_FORMAT);
	put_u32(&b, JME_VERSION_MAJOR << 16 | JME_VERSION_MINOR << 8 | JME_VERSION_PATCH);
	put_u32(&b, vm->options.heap_bytes);
	put_u32(&b, vm->options.ticks_per_ms);
	put_u64(&b, (u64)vm->options.epoch_ms);

	size_t s = section_begin(&b, SECTION('H', 'E', 'A', 'P'));
	put_u32(&b, vm->heap_top);
	put(&b, vm->heap + HEAP_START, vm->heap_top - HEAP_START);
	section_end(&b, s);

	s = section_begin(&b, SECTION('C', 'L', 'A', 'S'));
	put_u32(&b, vm->class_count);
	for (u32 i = 0; i < vm->class_count; i++) {
		const Class *c = vm->classes[i];
		u32 n = (u32)strlen(c->name);
		put_u32(&b, n);
		put(&b, c->name, n);
		put_u64(&b, c->bytes_hash);
	}
	section_end(&b, s);

	s = section_begin(&b, SECTION('C', 'S', 'T', 'A'));
	for (u32 i = 0; i < vm->class_count; i++) {
		const Class *c = vm->classes[i];
		put_u8(&b, c->state);
		put_u32(&b, c->init_thread);
		put_u32(&b, c->mirror);
		put_u32(&b, c->static_count);
		for (u32 k = 0; k < c->static_count; k++) put_u32(&b, c->statics[k]);
		u32 strings = 0;
		u32 cp_count = c->cf != NULL ? c->cf->cp_count : 0;
		for (u32 k = 1; k < cp_count; k++)
			if (c->resolved_flag[k] && c->cf->cp[k].tag == CP_STRING) strings++;
		put_u32(&b, strings);
		for (u32 k = 1; k < cp_count; k++) {
			if (!c->resolved_flag[k] || c->cf->cp[k].tag != CP_STRING) continue;
			put_u32(&b, k);
			put_u32(&b, c->resolved[k].str);
		}
	}
	section_end(&b, s);

	s = section_begin(&b, SECTION('S', 'T', 'R', 'S'));
	put_u32(&b, vm->interned_cap);
	put_u32(&b, vm->interned_count);
	for (u32 i = 0; i < vm->interned_cap; i++) put_u32(&b, vm->interned[i]);
	section_end(&b, s);

	s = section_begin(&b, SECTION('M', 'O', 'N', 'S'));
	put_u32(&b, vm->monitor_count);
	put_u32(&b, vm->monitor_free);
	for (u32 i = 1; i < vm->monitor_count; i++) {
		const Monitor *m = &vm->monitors[i];
		put_u32(&b, m->owner);
		put_u32(&b, m->count);
		put_u32(&b, m->entry_head);
		put_u32(&b, m->entry_tail);
		put_u32(&b, m->wait_head);
		put_u32(&b, m->wait_tail);
	}
	section_end(&b, s);

	s = section_begin(&b, SECTION('V', 'M', 'S', 'T'));
	put_u64(&b, vm->ticks);
	put_u32(&b, vm->sched_next);
	put_u32(&b, vm->turn);
	put_u64(&b, vm->turn_end);
	put_u32(&b, vm->hash_state);
	put_u32(&b, (u32)vm->finished);
	put_u32(&b, (u32)vm->exit_code);
	put_u32(&b, vm->oom_error);
	put_u32(&b, method_code(vm, vm->main_method));
	section_end(&b, s);

	s = section_begin(&b, SECTION('T', 'H', 'R', 'D'));
	put_u32(&b, vm->thread_count);
	for (u32 i = 0; i < vm->thread_count; i++) {
		const Thread *t = vm->threads[i];
		put_u8(&b, t->state);
		put_u8(&b, t->interrupted);
		put_u8(&b, t->timed);
		put_u8(&b, t->block_phase);
		put_u8(&b, t->granted);
		put_u32(&b, t->jthread);
		put_u32(&b, t->exception);
		put_u32(&b, t->invoke_arg);
		put_u32(&b, t->blocked_on);
		put_u32(&b, method_code(vm, t->invoke_after));
		put_u64(&b, t->wake_tick);
		put_u32(&b, t->enter_count);
		put_u32(&b, t->queue_next);
		put_u32(&b, t->join_target);
		put_u32(&b, class_code(t->init_wait));
		put_u32(&b, t->wait_channel);
		put_u32(&b, t->sp);
		for (u32 k = 0; k < t->sp; k++) put_u32(&b, t->slots[k]);
		put(&b, t->tags, t->sp);
		put_u32(&b, t->frame_count);
		for (u32 k = 0; k < t->frame_count; k++) {
			const Frame *f = &t->frames[k];
			put_u32(&b, method_code(vm, f->method));
			put_u32(&b, f->pc);
			put_u32(&b, f->locals);
			put_u32(&b, f->sp);
			put_u32(&b, f->flags);
			put_u32(&b, class_code(f->init_class));
			put_u32(&b, f->lock);
		}
	}
	section_end(&b, s);

	s = section_begin(&b, SECTION('H', 'O', 'S', 'T'));
	put_u32(&b, vm->host.state_count);
	for (u32 i = 0; i < vm->host.state_count && !b.oom; i++) {
		const HostState *h = &vm->host.states[i];
		u32 n = (u32)strlen(h->name);
		put_u32(&b, n);
		put(&b, h->name, n);
		size_t at = section_begin(&b, 0);
		if (h->save(h->user, collect_into, &b) != JME_OK) {
			free(b.p);
			return JME_E_STATE;
		}
		section_end(&b, at);
	}
	section_end(&b, s);

	put_u64(&b, fnv1a64(b.p, b.len));
	if (b.oom) {
		free(b.p);
		return JME_E_NOMEM;
	}
	*out = b.p;
	*len = b.len;
	return JME_OK;
}

JmeStatus jme_snapshot_save(JmeVM *vm, JmeWriteFn write, void *user) {
	if (!vm->started) return JME_E_STATE;
	u8 *data;
	size_t len;
	JmeStatus st = snapshot_build(vm, &data, &len);
	if (st != JME_OK) return st;
	st = write(user, data, len);
	free(data);
	return st;
}

/* ---- reading ---- */

typedef struct Rd {
	const u8 *p, *end;
	int bad;
} Rd;

static const u8 *get(Rd *r, size_t n) {
	if (r->bad || (size_t)(r->end - r->p) < n) {
		r->bad = 1;
		return NULL;
	}
	const u8 *p = r->p;
	r->p += n;
	return p;
}

static u32 get_u8(Rd *r) {
	const u8 *p = get(r, 1);
	return p != NULL ? p[0] : 0;
}

static u32 get_u32(Rd *r) {
	const u8 *p = get(r, 4);
	return p != NULL ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : 0;
}

static u64 get_u64(Rd *r) {
	u64 lo = get_u32(r);
	return lo | (u64)get_u32(r) << 32;
}

/* A reader over the payload of the next section, which must have this tag. */
static Rd section(Rd *r, u32 tag) {
	Rd s = {NULL, NULL, 1};
	if (get_u32(r) != tag) {
		r->bad = 1;
		return s;
	}
	u32 n = get_u32(r);
	const u8 *p = get(r, n);
	if (p == NULL) return s;
	s.p = p;
	s.end = p + n;
	s.bad = 0;
	return s;
}

static int section_done(Rd *s) {
	return !s->bad && s->p == s->end;
}

static Method *method_from(VM *vm, u32 code, int *ok) {
	if (code == M_NONE) return NULL;
	if (code == M_BOOT) return vm->boot_method;
	if (code == M_RUN) return vm->run_method;
	if (code >= vm->method_count) {
		*ok = 0;
		return NULL;
	}
	return vm->methods[code];
}

static Class *class_from(VM *vm, u32 code, int *ok) {
	if (code == 0) return NULL;
	if (code > vm->class_count) {
		*ok = 0;
		return NULL;
	}
	return vm->classes[code - 1];
}

/* ---- validation ---- */

typedef struct Check {
	VM *vm;
	u8 *starts; /* one bit per 8 heap bytes: an object starts there */
} Check;

static int is_start(const Check *k, Ref r) {
	return (k->starts[r >> 6] >> ((r >> 3) & 7)) & 1;
}

static int ref_ok(const Check *k, Ref r) {
	return r == 0 || (r >= HEAP_START && r < k->vm->heap_top && (r & 7) == 0 && is_start(k, r));
}

/* A reference that must be null or an instance of exactly class c. */
static int ref_of(const Check *k, Ref r, Class *c) {
	return ref_ok(k, r) && (r == 0 || class_of(k->vm, r) == c);
}

static int check_heap(Check *k) {
	VM *vm = k->vm;
	for (u32 r = HEAP_START; r < vm->heap_top;) {
		Object *o = OBJ(vm, r);
		if (o->cls >= vm->class_count || (o->hash & MARK_BIT) || o->forward != 0) return 0;
		if (o->monitor != 0 && o->monitor >= vm->monitor_count) return 0;
		Class *c = vm->classes[o->cls];
		u64 size;
		if (c->kind == CK_ARRAY) {
			if ((u64)r + ARRAY_DATA > vm->heap_top) return 0;
			size = ((u64)ARRAY_DATA + (u64)ARRAY_LEN(vm, r) * c->elem_size + 7) & ~7ull;
		} else {
			if (c->access & ACC_INTERFACE) return 0;
			size = c->instance_size;
		}
		if ((u64)r + size > vm->heap_top) return 0;
		k->starts[r >> 6] |= (u8)(1u << ((r >> 3) & 7));
		r += (u32)size;
	}
	for (u32 r = HEAP_START; r < vm->heap_top; r += object_size(vm, r)) {
		Class *c = class_of(vm, r);
		if (c->kind == CK_ARRAY) {
			if (c->elem_type != T_OBJECT) continue;
			for (u32 i = 0; i < ARRAY_LEN(vm, r); i++)
				if (!ref_ok(k, REF_ELEMS(vm, r)[i])) return 0;
			continue;
		}
		for (u32 i = 0; i < c->ref_count; i++)
			if (!ref_ok(k, *(Ref *)FIELD_PTR(vm, r, c->ref_offsets[i]))) return 0;
		if (c->is_reference && !ref_ok(k, *(Ref *)FIELD_PTR(vm, r, vm->referent_offset))) return 0;
		if (c == vm->c_string) {
			/* C code reads strings directly: their bounds must hold. */
			Ref value = *(Ref *)FIELD_PTR(vm, r, vm->f_string_value);
			i64 off = *(i32 *)FIELD_PTR(vm, r, vm->f_string_offset);
			i64 count = *(i32 *)FIELD_PTR(vm, r, vm->f_string_count);
			if (!ref_of(k, value, vm->c_char_array)) return 0;
			if (value == 0 ? count != 0 : off < 0 || count < 0 || off + count > ARRAY_LEN(vm, value)) return 0;
		} else if (c == vm->c_class) {
			if ((u32)*(i32 *)FIELD_PTR(vm, r, vm->f_class_id) >= vm->class_count) return 0;
		}
	}
	return 1;
}

static int check_queue(VM *vm, u32 head, u32 tail) {
	u32 n = 0, last = 0;
	for (u32 id = head; id != 0; id = vm->threads[id - 1]->queue_next) {
		if (id > vm->thread_count || ++n > vm->thread_count) return 0;
		last = id;
	}
	return last == tail;
}

static int check_frames(Check *k, Thread *t) {
	VM *vm = k->vm;
	if (t->state == TS_DONE) return t->frame_count == 0 && t->sp == 0;
	if (t->frame_count == 0 || t->frames[0].locals != 0) return 0;
	for (u32 i = 0; i < t->frame_count; i++) {
		const Frame *f = &t->frames[i];
		const Method *m = f->method;
		if (m == NULL || (m->access & ACC_NATIVE) || m->code == NULL) return 0;
		const CodeAttr *c = m->code;
		if ((u64)f->locals + c->max_locals > f->sp || f->sp > (u64)f->locals + c->max_locals + c->max_stack)
			return 0;
		if (i + 1 < t->frame_count ? t->frames[i + 1].locals != f->sp : t->sp != f->sp) return 0;
		if (f->flags & ~(u32)(F_ADVANCE | F_CLINIT)) return 0;
		if ((f->flags & F_CLINIT) && (f->init_class == NULL || f->init_class->state != CS_INITIALIZING)) return 0;
		if (!ref_ok(k, f->lock)) return 0;
		if (m->owner != vm->boot_class && !method_check_pc(vm, f->method, f->pc)) return 0;
		if (m->owner == vm->boot_class && f->pc >= c->code_len) return 0;
	}
	return 1;
}

static int check_all(Check *k) {
	VM *vm = k->vm;
	if (!check_heap(k)) return 0;
	for (u32 i = 0; i < vm->class_count; i++) {
		Class *c = vm->classes[i];
		for (u32 s = 0; s < c->static_count; s++)
			if (c->static_tags[s] && !ref_ok(k, c->statics[s])) return 0;
		if (!ref_of(k, c->mirror, vm->c_class)) return 0;
		if (c->mirror != 0 && (u32)*(i32 *)FIELD_PTR(vm, c->mirror, vm->f_class_id) != c->id) return 0;
		if (c->state == CS_INITIALIZING && (c->init_thread == 0 || c->init_thread > vm->thread_count)) return 0;
		u32 cp_count = c->cf != NULL ? c->cf->cp_count : 0;
		for (u32 s = 1; s < cp_count; s++)
			if (c->resolved_flag[s] && !ref_of(k, c->resolved[s].str, vm->c_string)) return 0;
	}
	u32 interned = 0;
	for (u32 i = 0; i < vm->interned_cap; i++) {
		if (!ref_of(k, vm->interned[i], vm->c_string)) return 0;
		if (vm->interned[i] != 0) interned++;
	}
	if (interned != vm->interned_count) return 0;
	if (vm->oom_error == 0 || !ref_ok(k, vm->oom_error)) return 0;

	/* Free monitors link through owner; the others belong to threads. */
	u8 *is_free = calloc(vm->monitor_count + 1, 1);
	if (is_free == NULL) return 0;
	int ok = 1;
	for (u32 id = vm->monitor_free; id != 0;) {
		if (id >= vm->monitor_count || is_free[id]) {
			ok = 0;
			break;
		}
		is_free[id] = 1;
		id = vm->monitors[id].owner;
	}
	for (u32 i = 1; i < vm->monitor_count && ok; i++) {
		const Monitor *m = &vm->monitors[i];
		if (is_free[i]) {
			ok = m->count == 0 && m->entry_head == 0 && m->wait_head == 0;
		} else {
			ok = m->owner <= vm->thread_count && (m->owner != 0 || m->count == 0)
					&& check_queue(vm, m->entry_head, m->entry_tail) && check_queue(vm, m->wait_head, m->wait_tail);
		}
	}
	free(is_free);
	if (!ok) return 0;

	if (vm->turn > vm->thread_count || vm->sched_next > vm->thread_count) return 0;
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *t = vm->threads[i];
		if (t->state > TS_DONE || t->queue_next > vm->thread_count || t->join_target > vm->thread_count) return 0;
		if (!ref_ok(k, t->jthread) || !ref_ok(k, t->exception) || !ref_ok(k, t->invoke_arg) || !ref_ok(k, t->blocked_on)) return 0;
		if ((t->state == TS_BLOCKED || t->state == TS_WAITING)
				&& (t->blocked_on == 0 || OBJ(vm, t->blocked_on)->monitor == 0))
			return 0;
		if (t->state == TS_INIT_WAIT && t->init_wait == NULL) return 0;
		if (t->invoke_after != NULL && (t->invoke_after->name != vm->sym.init || t->invoke_after->arg_slots != 1))
			return 0;
		for (u32 s = 0; s < t->sp; s++) {
			if (t->tags[s] > 1) return 0;
			if (t->tags[s] && !ref_ok(k, t->slots[s])) return 0;
		}
		if (!check_frames(k, t)) return 0;
	}
	return 1;
}

/* ---- loading ---- */

/*
 * Gives every registered host state its bytes. The snapshot must hold
 * exactly the states the VM has registered (the same host made it); each
 * state checks its own data.
 */
static JmeStatus load_host_states(VM *vm, Rd *s) {
	u32 count = get_u32(s);
	if (s->bad || count != vm->host.state_count) return JME_E_FORMAT;
	const u8 *names[64];
	u32 name_len[64];
	Rd bodies[64];
	if (count > 64) return JME_E_FORMAT;
	for (u32 i = 0; i < count; i++) {
		name_len[i] = get_u32(s);
		names[i] = get(s, name_len[i]);
		bodies[i] = section(s, 0);
		if (s->bad || bodies[i].bad) return JME_E_FORMAT;
	}
	if (!section_done(s)) return JME_E_FORMAT;
	for (u32 i = 0; i < count; i++) {
		const HostState *h = &vm->host.states[i];
		u32 k = 0;
		while (k < count && !(name_len[k] == strlen(h->name) && memcmp(names[k], h->name, name_len[k]) == 0)) k++;
		if (k == count) return JME_E_FORMAT;
		JmeStatus st = h->load(h->user, bodies[k].p, (size_t)(bodies[k].end - bodies[k].p));
		if (st != JME_OK) return st;
	}
	return JME_OK;
}

static JmeStatus restore(VM *vm, const u8 *data, size_t len) {
	Rd r = {data + HEADER_SIZE, data + len - 8, 0};
	int ok = 1;

	/* HEAP */
	Rd s = section(&r, SECTION('H', 'E', 'A', 'P'));
	u32 top = get_u32(&s);
	if (s.bad || top < HEAP_START || top > vm->heap_size || (top & 7) != 0) return JME_E_FORMAT;
	const u8 *heap = get(&s, top - HEAP_START);
	if (heap == NULL || !section_done(&s)) return JME_E_FORMAT;
	memcpy(vm->heap + HEAP_START, heap, top - HEAP_START);
	vm->heap_top = top;

	/* CLAS: replay the loads; each class must get its old id and bytes */
	s = section(&r, SECTION('C', 'L', 'A', 'S'));
	u32 classes = get_u32(&s);
	for (u32 i = 0; i < classes && !s.bad; i++) {
		u32 n = get_u32(&s);
		const u8 *name = get(&s, n);
		u64 hash = get_u64(&s);
		if (s.bad || n == 0 || n > 1024 || memchr(name, 0, n) != NULL) return JME_E_FORMAT;
		Class *c;
		JmeStatus st = class_load(vm, sym_intern(vm, (const char *)name, n), &c);
		if (st != JME_OK) return st == JME_E_NOMEM ? st : JME_E_NOTFOUND;
		if (c->id != i || c->bytes_hash != hash) return JME_E_FORMAT;
	}
	if (!section_done(&s) || vm->class_count != classes) return JME_E_FORMAT;
	JmeStatus st = vm_bind_core(vm);
	if (st != JME_OK) return st;
	if (vm->class_count != classes) return JME_E_FORMAT;

	/* CSTA */
	s = section(&r, SECTION('C', 'S', 'T', 'A'));
	for (u32 i = 0; i < classes && !s.bad; i++) {
		Class *c = vm->classes[i];
		u32 state = get_u8(&s);
		if (state > CS_ERROR) return JME_E_FORMAT;
		c->state = (ClassState)state;
		c->init_thread = get_u32(&s);
		c->mirror = get_u32(&s);
		if (get_u32(&s) != c->static_count) return JME_E_FORMAT;
		for (u32 k = 0; k < c->static_count; k++) c->statics[k] = get_u32(&s);
		u32 strings = get_u32(&s);
		u32 cp_count = c->cf != NULL ? c->cf->cp_count : 0;
		for (u32 k = 0; k < strings && !s.bad; k++) {
			u32 index = get_u32(&s);
			Ref str = get_u32(&s);
			if (index == 0 || index >= cp_count || c->cf->cp[index].tag != CP_STRING || c->resolved_flag[index])
				return JME_E_FORMAT;
			c->resolved[index].str = str;
			c->resolved_flag[index] = 1;
		}
		if (c->kind == CK_ARRAY && c->state != CS_INITIALIZED) return JME_E_FORMAT;
	}
	if (!section_done(&s)) return JME_E_FORMAT;

	/* STRS */
	s = section(&r, SECTION('S', 'T', 'R', 'S'));
	u32 cap = get_u32(&s);
	vm->interned_count = get_u32(&s);
	if (s.bad || (cap & (cap - 1)) != 0 || cap > (1u << 24) || (size_t)(s.end - s.p) != (size_t)cap * 4)
		return JME_E_FORMAT;
	if (cap) {
		vm->interned = calloc(cap, sizeof(Ref));
		if (vm->interned == NULL) return JME_E_NOMEM;
		vm->interned_cap = cap;
		for (u32 i = 0; i < cap; i++) vm->interned[i] = get_u32(&s);
	}
	/* lookups probe until an empty slot: one must exist */
	if (!section_done(&s) || (cap != 0 && vm->interned_count >= cap) || (cap == 0 && vm->interned_count != 0))
		return JME_E_FORMAT;

	/* MONS */
	s = section(&r, SECTION('M', 'O', 'N', 'S'));
	u32 monitors = get_u32(&s);
	vm->monitor_free = get_u32(&s);
	if (s.bad || monitors == 1 || monitors > (1u << 24)
			|| (size_t)(s.end - s.p) != (size_t)(monitors ? monitors - 1 : 0) * 24)
		return JME_E_FORMAT;
	if (monitors) {
		vm->monitors = calloc(monitors, sizeof(Monitor));
		if (vm->monitors == NULL) return JME_E_NOMEM;
		vm->monitor_cap = vm->monitor_count = monitors;
		for (u32 i = 1; i < monitors; i++) {
			Monitor *m = &vm->monitors[i];
			m->owner = get_u32(&s);
			m->count = get_u32(&s);
			m->entry_head = get_u32(&s);
			m->entry_tail = get_u32(&s);
			m->wait_head = get_u32(&s);
			m->wait_tail = get_u32(&s);
		}
	} else if (vm->monitor_free != 0) {
		return JME_E_FORMAT;
	}
	if (!section_done(&s)) return JME_E_FORMAT;

	/* VMST */
	s = section(&r, SECTION('V', 'M', 'S', 'T'));
	vm->ticks = get_u64(&s);
	vm->sched_next = get_u32(&s);
	vm->turn = get_u32(&s);
	vm->turn_end = get_u64(&s);
	vm->hash_state = get_u32(&s);
	vm->finished = get_u32(&s) != 0;
	vm->exit_code = (int)get_u32(&s);
	vm->oom_error = get_u32(&s);
	Method *main = method_from(vm, get_u32(&s), &ok);
	if (!section_done(&s) || !ok || vm->hash_state == 0) return JME_E_FORMAT;
	if ((st = vm_make_boot(vm, main)) != JME_OK) return st == JME_E_NOMEM ? st : JME_E_FORMAT;

	/* THRD */
	s = section(&r, SECTION('T', 'H', 'R', 'D'));
	u32 threads = get_u32(&s);
	/* Each thread record takes at least THREAD_RECORD bytes; checking that
	 * first stops a damaged count from allocating many stacks. */
	enum { THREAD_RECORD = 5 + 4 * 5 + 8 + 4 * 5 + 4 + 4 };
	if (s.bad || threads == 0 || (size_t)threads > (size_t)(s.end - s.p) / THREAD_RECORD) return JME_E_FORMAT;
	for (u32 i = 0; i < threads; i++) {
		Thread *t = thread_new(vm);
		if (t == NULL) return JME_E_NOMEM;
		u32 state = get_u8(&s);
		t->state = state <= TS_DONE ? (ThreadState)state : TS_DONE;
		t->interrupted = (u8)get_u8(&s);
		t->timed = (u8)get_u8(&s);
		t->block_phase = (u8)get_u8(&s);
		t->granted = (u8)get_u8(&s);
		t->jthread = get_u32(&s);
		t->exception = get_u32(&s);
		t->invoke_arg = get_u32(&s);
		t->blocked_on = get_u32(&s);
		t->invoke_after = method_from(vm, get_u32(&s), &ok);
		t->wake_tick = get_u64(&s);
		t->enter_count = get_u32(&s);
		t->queue_next = get_u32(&s);
		t->join_target = get_u32(&s);
		t->init_wait = class_from(vm, get_u32(&s), &ok);
		t->wait_channel = get_u32(&s);
		u32 sp = get_u32(&s);
		if (s.bad || state > TS_DONE || sp > THREAD_SLOTS) return JME_E_FORMAT;
		t->sp = sp;
		for (u32 k = 0; k < sp; k++) t->slots[k] = get_u32(&s);
		const u8 *tags = get(&s, sp);
		if (tags == NULL) return JME_E_FORMAT;
		memcpy(t->tags, tags, sp);
		u32 frames = get_u32(&s);
		if (s.bad || frames > THREAD_FRAMES) return JME_E_FORMAT;
		t->frame_count = frames;
		for (u32 k = 0; k < frames; k++) {
			Frame *f = &t->frames[k];
			f->method = method_from(vm, get_u32(&s), &ok);
			f->pc = get_u32(&s);
			f->locals = get_u32(&s);
			f->sp = get_u32(&s);
			f->flags = get_u32(&s);
			f->init_class = class_from(vm, get_u32(&s), &ok);
			f->lock = get_u32(&s);
		}
		if (s.bad || !ok) return JME_E_FORMAT;
		if (t->state == TS_DONE) {
			/* a finished thread has no stack (see thread_end) */
			if (sp != 0 || frames != 0) return JME_E_FORMAT;
			free(t->slots);
			free(t->tags);
			free(t->frames);
			t->slots = NULL;
			t->tags = NULL;
			t->frames = NULL;
		}
	}
	if (!section_done(&s)) return JME_E_FORMAT;

	/* HOST: read now, handed to the host only once the VM state is valid */
	Rd host = section(&r, SECTION('H', 'O', 'S', 'T'));
	if (host.bad || r.p != r.end) return JME_E_FORMAT;

	Check k = {vm, calloc((vm->heap_top >> 6) + 1, 1)};
	if (k.starts == NULL) return JME_E_NOMEM;
	int valid = check_all(&k);
	free(k.starts);
	if (!valid) return JME_E_FORMAT;
	if ((st = load_host_states(vm, &host)) != JME_OK) return st;
	vm->current = vm->threads[0];
	vm->started = 1;
	return JME_OK;
}

JmeStatus jme_snapshot_load(JmeVM *vm, const void *data, size_t len) {
	const u8 *d = data;
	if (vm->class_source == NULL) return JME_E_STATE;
	if (!little_endian()) return JME_E_UNSUPPORTED;
	if (len < HEADER_SIZE + 8 || memcmp(d, MAGIC, 8) != 0) return JME_E_FORMAT;
	u64 sum = 0;
	for (int i = 7; i >= 0; i--) sum = sum << 8 | d[len - 8 + (size_t)i];
	if (fnv1a64(d, len - 8) != sum) return JME_E_FORMAT;
	Rd h = {d + 8, d + HEADER_SIZE, 0};
	if (get_u32(&h) != SNAP_FORMAT) return JME_E_UNSUPPORTED;
	get_u32(&h); /* VM version: informational, the format number decides */
	JmeOptions o = vm->options;
	o.heap_bytes = get_u32(&h);
	o.ticks_per_ms = get_u32(&h);
	o.epoch_ms = (i64)get_u64(&h);

	/* Build the new state in a separate VM, and swap only if it is valid:
	 * a failed load leaves vm as it was. */
	JmeVM *n;
	JmeStatus st = jme_create(&o, &n);
	if (st != JME_OK) return st == JME_E_ARGUMENT ? JME_E_FORMAT : st;
	jme_set_class_source(n, vm->class_source, vm->class_source_user);
	jme_set_console(n, vm->console, vm->console_user);
	st = host_setup_copy(n, vm);
	if (st == JME_OK) st = restore(n, d, len);
	if (st == JME_OK) {
		JmeVM tmp = *vm;
		*vm = *n;
		*n = tmp;
	}
	jme_destroy(n);
	return st;
}
