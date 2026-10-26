/*
 * The heap arena and its mark-compact collector.
 *
 * Objects are allocated by bumping heap_top. A collection marks everything
 * reachable from the roots, then slides the live objects down to the start of
 * the arena (LISP2: compute new addresses, update every reference, move).
 * Collections only happen inside heap_alloc(), so they occur at the same
 * points in every run.
 */
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

#define ALIGN8(n) (((n) + 7u) & ~7u)

JmeStatus heap_init(VM *vm) {
	vm->heap_size = vm->options.heap_bytes & ~7u;
	vm->heap = malloc(vm->heap_size);
	vm->cards = calloc(((size_t)vm->heap_size >> CARD_SHIFT) + 1, 1);
	if (vm->heap == NULL || vm->cards == NULL) return JME_E_NOMEM;
	memset(vm->heap, 0, HEAP_START);
	vm->heap_top = HEAP_START;
	vm->hash_state = 0x2545F491u;
	return JME_OK;
}

void heap_free(VM *vm) {
	free(vm->heap);
	vm->heap = NULL;
	free(vm->cards);
	vm->cards = NULL;
	free(vm->c_roots);
	vm->c_roots = NULL;
}

void heap_mark(VM *vm, u32 from, u32 to) {
	if (to > from) memset(vm->cards + (from >> CARD_SHIFT), 1, ((to - 1) >> CARD_SHIFT) - (from >> CARD_SHIFT) + 1);
}

u32 heap_used(const VM *vm) {
	return vm->heap_top - HEAP_START;
}

void vm_root(VM *vm, Ref *ref) {
	if (vm->c_root_count == vm->c_root_cap) {
		u32 cap = vm->c_root_cap ? vm->c_root_cap * 2 : 64;
		Ref **roots = realloc(vm->c_roots, cap * sizeof(Ref *));
		if (roots == NULL) {
			fputs("jmevm: out of memory for roots\n", stderr);
			abort();
		}
		vm->c_roots = roots;
		vm->c_root_cap = cap;
	}
	vm->c_roots[vm->c_root_count++] = ref;
}

void vm_unroot(VM *vm, u32 n) {
	vm->c_root_count -= n;
}

Class *class_of(VM *vm, Ref r) {
	return vm->classes[OBJ(vm, r)->cls];
}

u32 object_size(VM *vm, Ref r) {
	Class *c = class_of(vm, r);
	if (c->kind == CK_ARRAY) return ALIGN8(ARRAY_DATA + ARRAY_LEN(vm, r) * c->elem_size);
	return c->instance_size;
}

i32 identity_hash(VM *vm, Ref r) {
	Object *o = OBJ(vm, r);
	if ((o->hash & ~MARK_BIT) == 0) {
		/* xorshift32: the sequence depends only on how many hashes were taken */
		u32 h;
		do {
			u32 x = vm->hash_state;
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x << 5;
			vm->hash_state = x;
			h = x & ~MARK_BIT;
		} while (h == 0);
		o->hash = (o->hash & MARK_BIT) | h;
		CARD_MARK(vm, r);
	}
	return (i32)(o->hash & ~MARK_BIT);
}

/* ---- roots ---- */

typedef void (*RootFn)(VM *vm, Ref *ref, void *ctx);

static void visit_roots(VM *vm, RootFn fn, void *ctx) {
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *t = vm->threads[i];
		for (u32 s = 0; s < t->sp; s++)
			if (t->tags[s]) fn(vm, &t->slots[s], ctx);
		for (u32 f = 0; f < t->frame_count; f++) fn(vm, &t->frames[f].lock, ctx);
		fn(vm, &t->jthread, ctx);
		fn(vm, &t->exception, ctx);
		fn(vm, &t->invoke_arg, ctx);
		fn(vm, &t->blocked_on, ctx);
	}
	for (u32 i = 0; i < vm->class_count; i++) {
		Class *c = vm->classes[i];
		for (u32 s = 0; s < c->static_count; s++)
			if (c->static_tags[s]) fn(vm, &c->statics[s], ctx);
		if (c->cf != NULL) {
			for (u32 k = 1; k < c->cf->cp_count; k++)
				if (c->resolved_flag[k] && c->cf->cp[k].tag == CP_STRING) fn(vm, &c->resolved[k].str, ctx);
		}
		fn(vm, &c->mirror, ctx);
	}
	for (u32 i = 0; i < vm->interned_cap; i++)
		if (vm->interned[i]) fn(vm, &vm->interned[i], ctx);
	for (u32 i = 0; i < vm->c_root_count; i++) fn(vm, vm->c_roots[i], ctx);
	fn(vm, &vm->oom_error, ctx);
}

/* ---- mark ---- */

typedef struct MarkState {
	Ref *stack;
	u32 count, cap;
	Ref *weak;     /* live Reference objects */
	u32 weak_count, weak_cap;
} MarkState;

static void check_ref(VM *vm, Ref r) {
	if (r < HEAP_START || r >= vm->heap_top || (r & 7u) != 0 || OBJ(vm, r)->cls >= vm->class_count) {
		fprintf(stderr, "jmevm: corrupt reference %u (heap top %u)\n", r, vm->heap_top);
		abort();
	}
}

static void push_ref(Ref **stack, u32 *count, u32 *cap, Ref r) {
	if (*count == *cap) {
		u32 n = *cap ? *cap * 2 : 1024;
		Ref *s = realloc(*stack, n * sizeof(Ref));
		if (s == NULL) {
			fputs("jmevm: out of memory while collecting\n", stderr);
			abort();
		}
		*stack = s;
		*cap = n;
	}
	(*stack)[(*count)++] = r;
}

static void mark_ref(VM *vm, MarkState *ms, Ref r) {
	if (r == 0) return;
	check_ref(vm, r);
	Object *o = OBJ(vm, r);
	if (o->hash & MARK_BIT) return;
	o->hash |= MARK_BIT;
	push_ref(&ms->stack, &ms->count, &ms->cap, r);
}

static void mark_root(VM *vm, Ref *ref, void *ctx) {
	mark_ref(vm, ctx, *ref);
}

static void mark_all(VM *vm, MarkState *ms) {
	visit_roots(vm, mark_root, ms);
	while (ms->count > 0) {
		Ref r = ms->stack[--ms->count];
		Class *c = class_of(vm, r);
		if (c->kind == CK_ARRAY) {
			if (c->elem_type != T_OBJECT) continue;
			u32 n = ARRAY_LEN(vm, r);
			for (u32 i = 0; i < n; i++) mark_ref(vm, ms, REF_ELEMS(vm, r)[i]);
		} else {
			for (u32 i = 0; i < c->ref_count; i++)
				mark_ref(vm, ms, *(Ref *)FIELD_PTR(vm, r, c->ref_offsets[i]));
			if (c->is_reference) push_ref(&ms->weak, &ms->weak_count, &ms->weak_cap, r);
		}
	}
	/* Weak references to objects nothing else keeps alive are cleared. */
	for (u32 i = 0; i < ms->weak_count; i++) {
		Ref *referent = FIELD_PTR(vm, ms->weak[i], vm->referent_offset);
		if (*referent != 0 && !(OBJ(vm, *referent)->hash & MARK_BIT)) *referent = 0;
	}
}

/* ---- compact ---- */

static Ref forwarded(VM *vm, Ref r) {
	return r == 0 ? 0 : OBJ(vm, r)->forward;
}

static void update_root(VM *vm, Ref *ref, void *ctx) {
	(void)ctx;
	*ref = forwarded(vm, *ref);
}

void heap_collect(VM *vm) {
	MarkState ms = {0};
	mark_all(vm, &ms);

	/* 1. New addresses; dead objects give back their monitors. */
	u32 free_at = HEAP_START;
	for (u32 r = HEAP_START; r < vm->heap_top;) {
		u32 size = object_size(vm, r);
		Object *o = OBJ(vm, r);
		if (o->hash & MARK_BIT) {
			o->forward = free_at;
			free_at += size;
		} else if (o->monitor != 0) {
			monitor_release_dead(vm, o->monitor);
		}
		r += size;
	}

	/* 2. Every reference now points at the new address. */
	visit_roots(vm, update_root, NULL);
	for (u32 r = HEAP_START; r < vm->heap_top;) {
		u32 size = object_size(vm, r);
		if (OBJ(vm, r)->hash & MARK_BIT) {
			Class *c = class_of(vm, r);
			if (c->kind == CK_ARRAY) {
				if (c->elem_type == T_OBJECT) {
					u32 n = ARRAY_LEN(vm, r);
					Ref *e = REF_ELEMS(vm, r);
					for (u32 i = 0; i < n; i++) e[i] = forwarded(vm, e[i]);
				}
			} else {
				for (u32 i = 0; i < c->ref_count; i++) {
					Ref *f = FIELD_PTR(vm, r, c->ref_offsets[i]);
					*f = forwarded(vm, *f);
				}
				if (c->is_reference) {
					Ref *f = FIELD_PTR(vm, r, vm->referent_offset);
					*f = forwarded(vm, *f);
				}
			}
		}
		r += size;
	}

	/* 3. Slide. New addresses are never above old ones, and objects are
	 * visited in address order, so nothing unvisited is overwritten. */
	for (u32 r = HEAP_START; r < vm->heap_top;) {
		u32 size = object_size(vm, r);
		Object *o = OBJ(vm, r);
		if (o->hash & MARK_BIT) {
			u32 to = o->forward;
			if (to != r) memmove(vm->heap + to, vm->heap + r, size);
			Object *n = OBJ(vm, to);
			n->hash &= ~MARK_BIT;
			n->forward = 0;
		}
		r += size;
	}

	if (vm->options.gc_stress) memset(vm->heap + free_at, 0xDB, vm->heap_top - free_at);
	heap_mark(vm, HEAP_START, vm->heap_top);   /* headers, moves, cleared referents */
	vm->heap_top = free_at;
	vm->gc_count++;
	free(ms.stack);
	free(ms.weak);
}

Ref heap_alloc(VM *vm, u32 cls_id, u32 size) {
	size = ALIGN8(size);
	if (size < OBJ_HEADER || size > vm->heap_size - HEAP_START) return 0;
	if (vm->options.gc_stress || vm->heap_size - vm->heap_top < size) {
		heap_collect(vm);
		if (vm->heap_size - vm->heap_top < size) return 0;
	}
	Ref r = vm->heap_top;
	vm->heap_top += size;
	memset(vm->heap + r, 0, size);
	OBJ(vm, r)->cls = cls_id;
	heap_mark(vm, r, r + size);
	return r;
}

static void throw_oom(VM *vm) {
	vm->current->exception = vm->oom_error;
}

Ref new_object(VM *vm, Class *c) {
	Ref r = heap_alloc(vm, c->id, c->instance_size);
	if (r == 0) throw_oom(vm);
	return r;
}

Ref new_array(VM *vm, Class *array_class, i32 length) {
	if (length < 0) {
		char msg[16];
		snprintf(msg, sizeof msg, "%d", length);
		throw_new(vm, "java/lang/NegativeArraySizeException", msg);
		return 0;
	}
	u64 size = ARRAY_DATA + (u64)length * array_class->elem_size;
	if (size > vm->heap_size) {
		throw_oom(vm);
		return 0;
	}
	Ref r = heap_alloc(vm, array_class->id, (u32)size);
	if (r == 0) {
		throw_oom(vm);
		return 0;
	}
	ARRAY_LEN(vm, r) = (u32)length;
	return r;
}
