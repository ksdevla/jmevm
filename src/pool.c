#include "pool.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define POOL_ALIGN 8
#define CHUNK_MIN (16 * 1024)

struct PoolChunk {
	PoolChunk *next;
	size_t cap;
	size_t used;
};

/* Chunk data starts after the header, rounded up to 16 bytes. */
#define CHUNK_HEADER ((sizeof(PoolChunk) + 15) & ~(size_t)15)

void pool_init(Pool *pool) {
	pool->head = NULL;
}

void *pool_alloc(Pool *pool, size_t size) {
	if (size > SIZE_MAX - CHUNK_HEADER - POOL_ALIGN) return NULL;
	size = (size + POOL_ALIGN - 1) & ~(size_t)(POOL_ALIGN - 1);
	if (size == 0) size = POOL_ALIGN;

	PoolChunk *c = pool->head;
	if (c == NULL || c->cap - c->used < size) {
		size_t cap = size > CHUNK_MIN ? size : CHUNK_MIN;
		c = malloc(CHUNK_HEADER + cap);
		if (c == NULL) return NULL;
		c->next = pool->head;
		c->cap = cap;
		c->used = 0;
		pool->head = c;
	}
	void *p = (char *)c + CHUNK_HEADER + c->used;
	c->used += size;
	memset(p, 0, size);
	return p;
}

void *pool_dup(Pool *pool, const void *data, size_t size) {
	void *p = pool_alloc(pool, size);
	if (p != NULL && size > 0) memcpy(p, data, size);
	return p;
}

void pool_free_all(Pool *pool) {
	PoolChunk *c = pool->head;
	while (c != NULL) {
		PoolChunk *next = c->next;
		free(c);
		c = next;
	}
	pool->head = NULL;
}
