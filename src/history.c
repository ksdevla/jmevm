/*
 * The history: every-frame rewind.
 *
 * The host records the machine after each step it shows (a frame), and can
 * then go back one record at a time. A record holds only the old contents of
 * what changed since the record before it, so its cost follows the change,
 * not the size of the heap: a step back writes those contents back.
 *
 * The machine is two regions, each compared in BLOCK-byte blocks with a copy
 * of it as it was at the newest record (its shadow):
 *   - the heap, up to the highest heap_top seen (span);
 *   - the rest: the snapshot's sections after HEAP (classes, strings,
 *     monitors, threads, host state), as snapshot_rest() writes them.
 * Going back restores the heap blocks in place and the rest with
 * snapshot_rest_restore(); after a class load only a full, checked load of
 * the restored parts can go back past it (snapshot_load_parts()).
 *
 * Records live in one ring of budget bytes, oldest first; a new record that
 * does not fit drops the oldest. Nothing is allocated per record.
 *
 * A record: u32 heap_top, u32 rest length, u32 heap blocks, u32 rest blocks,
 * then per block u32 offset and BLOCK bytes, heap blocks first; everything
 * as it was before the record's step.
 */
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

#define BLOCK CARD   /* a block of the heap is a card of the card table */
#define RECORD_HEADER 16u

typedef struct Rec {
	size_t at, size;   /* where in the ring */
} Rec;

struct JmeHistory {
	JmeVM *vm;

	u8 *heap;           /* shadow of vm->heap[0, span) */
	u32 span;           /* a multiple of BLOCK, at most heap_size */
	u32 heap_top;
	u8 *rest;           /* shadow of the rest, zero after rest_len */
	size_t rest_len, rest_cap;
	u8 *now;            /* the rest as it is now, zero after its length */
	size_t now_cap;
	u32 *changed;       /* offsets of changed blocks, heap ones then rest ones */
	u32 changed_cap;

	u8 *ring;
	size_t ring_size, write_at, stored;
	Rec *recs;          /* circular, recs[first] the oldest */
	u32 rec_cap, first, count;
};

static size_t round_up(size_t n) {
	return (n + BLOCK - 1) / BLOCK * BLOCK;
}

static void put32(u8 *p, u32 v) {
	memcpy(p, &v, 4);
}

static u32 get32(const u8 *p) {
	u32 v;
	memcpy(&v, p, 4);
	return v;
}

/* Grows a zero-filled buffer to at least n bytes. */
static int reserve(u8 **buf, size_t *cap, size_t n) {
	if (n <= *cap) return 1;
	size_t c = *cap ? *cap : 64 * 1024;
	while (c < n) c *= 2;
	u8 *p = realloc(*buf, c);
	if (p == NULL) return 0;
	memset(p + *cap, 0, c - *cap);
	*buf = p;
	*cap = c;
	return 1;
}

/* Takes the machine as it is now as the base: no records. */
static JmeStatus take_base(JmeHistory *h) {
	VM *vm = h->vm;
	h->count = 0;
	h->first = 0;
	h->write_at = 0;
	h->stored = 0;
	h->heap_top = vm->heap_top;
	h->span = (u32)round_up(vm->heap_top);
	if (h->span > vm->heap_size) h->span = vm->heap_size;
	memcpy(h->heap, vm->heap, h->span);
	memset(vm->cards, 0, ((size_t)vm->heap_size >> CARD_SHIFT) + 1);
	size_t len;
	JmeStatus st = snapshot_rest(vm, &h->now, &h->now_cap, &len);
	if (st != JME_OK) return st;
	if (!reserve(&h->rest, &h->rest_cap, round_up(len) + BLOCK)) return JME_E_NOMEM;
	memset(h->rest, 0, h->rest_cap);
	memcpy(h->rest, h->now, len);
	h->rest_len = len;
	return JME_OK;
}

JmeStatus jme_history_create(JmeVM *vm, size_t budget_bytes, JmeHistory **out) {
	*out = NULL;
	if (!vm->started) return JME_E_STATE;
	JmeHistory *h = calloc(1, sizeof(JmeHistory));
	if (h == NULL) return JME_E_NOMEM;
	h->vm = vm;
	h->heap = malloc(vm->heap_size);
	h->ring = budget_bytes ? malloc(budget_bytes) : NULL;
	h->ring_size = h->ring ? budget_bytes : 0;
	JmeStatus st = h->heap == NULL || (budget_bytes && h->ring == NULL) ? JME_E_NOMEM : take_base(h);
	if (st != JME_OK) {
		jme_history_destroy(h);
		return st;
	}
	*out = h;
	return JME_OK;
}

void jme_history_destroy(JmeHistory *h) {
	if (h == NULL) return;
	free(h->heap);
	free(h->rest);
	free(h->now);
	free(h->changed);
	free(h->ring);
	free(h->recs);
	free(h);
}

JmeStatus jme_history_reset(JmeHistory *h) {
	return take_base(h);
}

uint32_t jme_history_count(const JmeHistory *h) {
	return h->count;
}

size_t jme_history_size(const JmeHistory *h) {
	return h->stored;
}

static Rec *rec(JmeHistory *h, u32 i) {   /* i-th oldest */
	return &h->recs[(h->first + i) % h->rec_cap];
}

static void drop_oldest(JmeHistory *h) {
	h->stored -= rec(h, 0)->size;
	h->first = (h->first + 1) % h->rec_cap;
	h->count--;
}

/* Room for a record of n bytes, the oldest records making way; NULL if it
 * cannot fit at all (then there are no records left). */
static u8 *make_room(JmeHistory *h, size_t n) {
	if (n > h->ring_size) {
		while (h->count) drop_oldest(h);
		return NULL;
	}
	if (h->write_at + n > h->ring_size) {
		/* Wrap: everything after the write position is older than the
		 * records at the start of the ring. */
		while (h->count && rec(h, 0)->at >= h->write_at) drop_oldest(h);
		h->write_at = 0;
	}
	while (h->count) {
		Rec *o = rec(h, 0);
		if (o->at >= h->write_at + n || o->at + o->size <= h->write_at) break;
		drop_oldest(h);
	}
	if (h->count == h->rec_cap) {
		u32 cap = h->rec_cap ? h->rec_cap * 2 : 1024;
		Rec *a = malloc(cap * sizeof(Rec));
		if (a == NULL) return NULL;
		for (u32 i = 0; i < h->count; i++) a[i] = *rec(h, i);
		free(h->recs);
		h->recs = a;
		h->rec_cap = cap;
		h->first = 0;
	}
	return h->ring + h->write_at;
}

/*
 * Debug builds (JMEVM_HISTORY_VERIFY) check the card table: a heap block
 * that changed although its card is clear is a write that forgot its mark,
 * which would make going back wrong without a sound.
 */
static void check_cards(JmeHistory *h) {
#ifdef JMEVM_HISTORY_VERIFY
	VM *vm = h->vm;
	for (u32 b = 0; b < h->span / BLOCK; b++) {
		if (vm->cards[b] || memcmp(vm->heap + b * BLOCK, h->heap + b * BLOCK, BLOCK) == 0) continue;
		fprintf(stderr, "jmevm: heap bytes %u to %u changed without their card marked\n", b * BLOCK,
				(b + 1) * BLOCK);
		abort();
	}
#else
	(void)h;
#endif
}

static int note_change(JmeHistory *h, u32 *n, u32 off) {
	if (*n == h->changed_cap) {
		u32 cap = h->changed_cap ? h->changed_cap * 2 : 4096;
		u32 *a = realloc(h->changed, cap * sizeof(u32));
		if (a == NULL) return 0;
		h->changed = a;
		h->changed_cap = cap;
	}
	h->changed[(*n)++] = off;
	return 1;
}

JmeStatus jme_history_record(JmeHistory *h) {
	VM *vm = h->vm;
	size_t len;
	JmeStatus st = snapshot_rest(vm, &h->now, &h->now_cap, &len);
	if (st != JME_OK) return st;
	size_t rest_span = round_up(len > h->rest_len ? len : h->rest_len);
	if (!reserve(&h->now, &h->now_cap, rest_span) || !reserve(&h->rest, &h->rest_cap, rest_span))
		return JME_E_NOMEM;
	memset(h->now + len, 0, rest_span - len);

	u32 span = (u32)round_up(vm->heap_top);
	if (span > vm->heap_size) span = vm->heap_size;
	if (span > h->span) {
		/* first use of this part of the heap: it was as the VM left it */
		memcpy(h->heap + h->span, vm->heap + h->span, span - h->span);
		h->span = span;
	}

	/* Only marked cards can differ; a marked card may not differ after all. */
	check_cards(h);
	u32 heap_blocks = 0, n = 0;
	for (u32 b = 0; b < h->span / BLOCK; b++) {
		if (!vm->cards[b]) continue;
		vm->cards[b] = 0;
		u32 off = b * BLOCK;
		if (memcmp(vm->heap + off, h->heap + off, BLOCK) != 0 && !note_change(h, &n, off)) return JME_E_NOMEM;
	}
	heap_blocks = n;
	for (u32 off = 0; off < rest_span; off += BLOCK)
		if (memcmp(h->now + off, h->rest + off, BLOCK) != 0 && !note_change(h, &n, off)) return JME_E_NOMEM;

	size_t size = RECORD_HEADER + (size_t)n * (4 + BLOCK);
	u8 *p = make_room(h, size);
	if (p != NULL) {
		put32(p, h->heap_top);
		put32(p + 4, (u32)h->rest_len);
		put32(p + 8, heap_blocks);
		put32(p + 12, n - heap_blocks);
		u8 *q = p + RECORD_HEADER;
		for (u32 i = 0; i < n; i++, q += 4 + BLOCK) {
			put32(q, h->changed[i]);
			memcpy(q + 4, (i < heap_blocks ? h->heap : h->rest) + h->changed[i], BLOCK);
		}
		Rec *r = &h->recs[(h->first + h->count) % h->rec_cap];
		r->at = h->write_at;
		r->size = size;
		h->count++;
		h->stored += size;
		h->write_at += size;
	}
	for (u32 i = 0; i < n; i++) {
		u32 off = h->changed[i];
		if (i < heap_blocks)
			memcpy(h->heap + off, vm->heap + off, BLOCK);
		else
			memcpy(h->rest + off, h->now + off, BLOCK);
	}
	h->heap_top = vm->heap_top;
	h->rest_len = len;
	return JME_OK;
}

JmeStatus jme_history_back(JmeHistory *h) {
	if (h->count == 0) return JME_E_STATE;
	VM *vm = h->vm;
	/* Changes since the newest record go too: the heap as it was then. */
	check_cards(h);
	for (u32 b = 0; b < h->span / BLOCK; b++) {
		if (!vm->cards[b]) continue;
		vm->cards[b] = 0;
		memcpy(vm->heap + b * BLOCK, h->heap + b * BLOCK, BLOCK);
	}
	Rec *r = rec(h, h->count - 1);
	const u8 *p = h->ring + r->at;
	u32 heap_blocks = get32(p + 8), rest_blocks = get32(p + 12);
	const u8 *q = p + RECORD_HEADER;
	for (u32 i = 0; i < heap_blocks + rest_blocks; i++, q += 4 + BLOCK) {
		u32 off = get32(q);
		if (i < heap_blocks) {
			memcpy(h->heap + off, q + 4, BLOCK);
			memcpy(vm->heap + off, q + 4, BLOCK);
		} else {
			memcpy(h->rest + off, q + 4, BLOCK);
		}
	}
	h->heap_top = get32(p);
	h->rest_len = get32(p + 4);
	vm->heap_top = h->heap_top;

	JmeStatus st = snapshot_rest_restore(vm, h->rest, h->rest_len);
	if (st == JME_E_UNSUPPORTED) {
		/* Classes were loaded since: only a full load goes back past that.
		 * It makes a new heap, zero above heap_top; the shadow keeps what
		 * was there, as the VM had it. */
		st = snapshot_load_parts(vm, h->rest, h->rest_len);
		if (st == JME_OK) memcpy(vm->heap + h->heap_top, h->heap + h->heap_top, h->span - h->heap_top);
	}
	if (st != JME_OK) return st;
	h->write_at = r->at;
	h->stored -= r->size;
	h->count--;
	return JME_OK;
}
