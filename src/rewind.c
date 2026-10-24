/*
 * The rewind ring: a history of snapshots kept compressed in memory.
 *
 * Entries come in groups: a keyframe (a whole snapshot) followed by deltas
 * (the XOR of a snapshot with that keyframe, mostly zeros because the heap
 * sits at a fixed offset and changes little between frames). Everything is
 * compressed with lz.c. Loading an entry needs at most its keyframe and
 * itself. When the ring goes over its byte budget, the oldest whole group
 * is dropped, so the first entry is always a keyframe.
 */
#include <stdlib.h>

#include "vm.h"

typedef struct Entry {
	u8 *data;          /* compressed */
	size_t size;       /* compressed size */
	size_t raw_size;   /* snapshot size */
	u8 key;
} Entry;

struct JmeRewind {
	Entry *entries;
	u32 count, cap;
	size_t budget, stored;
	u32 key_interval;
	u32 since_key;     /* deltas since the newest keyframe */
	u8 *key_raw;       /* the newest keyframe, uncompressed, for new deltas */
	size_t key_raw_size;
};

JmeStatus jme_rewind_create(size_t budget_bytes, uint32_t keyframe_interval, JmeRewind **out) {
	*out = NULL;
	if (keyframe_interval == 0) return JME_E_ARGUMENT;
	JmeRewind *rw = calloc(1, sizeof(JmeRewind));
	if (rw == NULL) return JME_E_NOMEM;
	rw->budget = budget_bytes;
	rw->key_interval = keyframe_interval;
	*out = rw;
	return JME_OK;
}

static void drop_front(JmeRewind *rw, u32 n) {
	for (u32 i = 0; i < n; i++) {
		rw->stored -= rw->entries[i].size;
		free(rw->entries[i].data);
	}
	memmove(rw->entries, rw->entries + n, (rw->count - n) * sizeof(Entry));
	rw->count -= n;
}

void jme_rewind_truncate(JmeRewind *rw, uint32_t keep) {
	while (rw->count > keep) {
		Entry *e = &rw->entries[--rw->count];
		rw->stored -= e->size;
		free(e->data);
	}
	/* the next push makes a keyframe, so no delta refers to a dropped one */
	free(rw->key_raw);
	rw->key_raw = NULL;
	rw->key_raw_size = 0;
}

void jme_rewind_destroy(JmeRewind *rw) {
	if (rw == NULL) return;
	jme_rewind_truncate(rw, 0);
	free(rw->entries);
	free(rw);
}

uint32_t jme_rewind_count(const JmeRewind *rw) {
	return rw->count;
}

size_t jme_rewind_size(const JmeRewind *rw) {
	return rw->stored;
}

/* dst[i] = a[i] ^ b[i], where a missing byte of b counts as 0 */
static void xor_into(u8 *dst, const u8 *a, size_t n, const u8 *b, size_t bn) {
	size_t m = n < bn ? n : bn;
	for (size_t i = 0; i < m; i++) dst[i] = a[i] ^ b[i];
	if (n > m) memcpy(dst + m, a + m, n - m);
}

JmeStatus jme_rewind_push(JmeRewind *rw, JmeVM *vm) {
	if (!vm->started) return JME_E_STATE;
	u8 *raw;
	size_t raw_size;
	JmeStatus st = snapshot_build(vm, &raw, &raw_size);
	if (st != JME_OK) return st;

	int key = rw->key_raw == NULL || rw->since_key + 1 >= rw->key_interval;
	const u8 *input = raw;
	u8 *delta = NULL;
	if (!key) {
		delta = malloc(raw_size);
		if (delta == NULL) {
			free(raw);
			return JME_E_NOMEM;
		}
		xor_into(delta, raw, raw_size, rw->key_raw, rw->key_raw_size);
		input = delta;
	}
	u8 *packed = malloc(lz_bound(raw_size));
	if (packed == NULL || (rw->count == rw->cap && rw->cap > UINT32_MAX / 2)) {
		free(packed);
		free(delta);
		free(raw);
		return JME_E_NOMEM;
	}
	size_t size = lz_compress(input, raw_size, packed);
	free(delta);
	u8 *shrunk = realloc(packed, size ? size : 1);
	if (shrunk != NULL) packed = shrunk;

	if (rw->count == rw->cap) {
		u32 cap = rw->cap ? rw->cap * 2 : 64;
		Entry *a = realloc(rw->entries, cap * sizeof(Entry));
		if (a == NULL) {
			free(packed);
			free(raw);
			return JME_E_NOMEM;
		}
		rw->entries = a;
		rw->cap = cap;
	}
	Entry *e = &rw->entries[rw->count++];
	e->data = packed;
	e->size = size;
	e->raw_size = raw_size;
	e->key = (u8)key;
	rw->stored += size;
	if (key) {
		free(rw->key_raw);
		rw->key_raw = raw;
		rw->key_raw_size = raw_size;
		rw->since_key = 0;
	} else {
		free(raw);
		rw->since_key++;
	}

	/* Over budget: drop the oldest group while another keyframe follows it. */
	while (rw->stored > rw->budget && rw->count > 1) {
		u32 next = 1;
		while (next < rw->count && !rw->entries[next].key) next++;
		if (next == rw->count) {
			/* One group holds everything: start a new one at the next push. */
			rw->since_key = rw->key_interval;
			break;
		}
		drop_front(rw, next);
	}
	return JME_OK;
}

/* Decompresses entry i into a malloc'd buffer of e->raw_size bytes. */
static u8 *unpack(const Entry *e) {
	u8 *out = malloc(e->raw_size ? e->raw_size : 1);
	if (out != NULL && !lz_decompress(e->data, e->size, out, e->raw_size)) {
		free(out);
		out = NULL;
	}
	return out;
}

JmeStatus jme_rewind_load(JmeRewind *rw, JmeVM *vm, uint32_t back) {
	if (back >= rw->count) return JME_E_ARGUMENT;
	u32 i = rw->count - 1 - back;
	u32 k = i;
	while (!rw->entries[k].key) k--; /* entries[0] is always a keyframe */
	u8 *key = unpack(&rw->entries[k]);
	if (key == NULL) return JME_E_NOMEM;
	JmeStatus st;
	if (k == i) {
		st = jme_snapshot_load(vm, key, rw->entries[k].raw_size);
	} else {
		u8 *snap = unpack(&rw->entries[i]);
		if (snap == NULL) {
			free(key);
			return JME_E_NOMEM;
		}
		xor_into(snap, snap, rw->entries[i].raw_size, key, rw->entries[k].raw_size);
		st = jme_snapshot_load(vm, snap, rw->entries[i].raw_size);
		free(snap);
	}
	free(key);
	return st;
}
