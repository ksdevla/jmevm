/*
 * Interned names and descriptors. Two symbols are equal exactly when their
 * pointers are, so member lookups compare pointers. Running out of memory
 * for symbols aborts: the VM cannot continue without its metadata.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

static u32 hash_bytes(const char *s, size_t len) {
	u32 h = 2166136261u;
	for (size_t i = 0; i < len; i++) h = (h ^ (u8)s[i]) * 16777619u;
	return h;
}

static void out_of_memory(void) {
	fputs("jmevm: out of memory for symbols\n", stderr);
	abort();
}

static int grow(VM *vm) {
	SymTable *t = &vm->syms;
	u32 cap = t->cap ? t->cap * 2 : 1024;
	const char **slots = calloc(cap, sizeof(char *));
	if (slots == NULL) return 0;
	for (u32 i = 0; i < t->cap; i++) {
		const char *s = t->slots[i];
		if (s == NULL) continue;
		u32 j = hash_bytes(s, strlen(s)) & (cap - 1);
		while (slots[j] != NULL) j = (j + 1) & (cap - 1);
		slots[j] = s;
	}
	free(t->slots);
	t->slots = slots;
	t->cap = cap;
	return 1;
}

const char *sym_intern(VM *vm, const char *s, size_t len) {
	SymTable *t = &vm->syms;
	if ((t->count + 1) * 2 > t->cap && !grow(vm)) out_of_memory();
	u32 j = hash_bytes(s, len) & (t->cap - 1);
	for (;;) {
		const char *e = t->slots[j];
		if (e == NULL) break;
		if (strncmp(e, s, len) == 0 && e[len] == '\0') return e;
		j = (j + 1) & (t->cap - 1);
	}
	char *copy = pool_alloc(&vm->pool, len + 1);
	if (copy == NULL) out_of_memory();
	memcpy(copy, s, len);
	copy[len] = '\0';
	t->slots[j] = copy;
	t->count++;
	return copy;
}

const char *sym_cstr(VM *vm, const char *s) {
	return sym_intern(vm, s, strlen(s));
}

const char *sym_utf8(VM *vm, Utf8 s) {
	return sym_intern(vm, (const char *)s.bytes, s.len);
}
