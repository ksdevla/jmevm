/*
 * Bump allocator for metadata that lives and dies together (one class file,
 * one loaded class). Metadata stays outside the heap arena: it is rebuilt
 * from the class bytes on restore, so it is never part of a snapshot.
 */
#ifndef JMEVM_POOL_H
#define JMEVM_POOL_H

#include <stddef.h>

typedef struct PoolChunk PoolChunk;

typedef struct Pool {
	PoolChunk *head;
} Pool;

void pool_init(Pool *pool);

/* Zeroed, 8-byte aligned memory, or NULL when out of memory. */
void *pool_alloc(Pool *pool, size_t size);

/* Copies size bytes into the pool. */
void *pool_dup(Pool *pool, const void *data, size_t size);

/* Frees everything allocated from the pool. */
void pool_free_all(Pool *pool);

#endif
