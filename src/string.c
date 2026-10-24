/*
 * java.lang.String helpers. A String is { char[] value; int offset; int
 * count; int hash; }; the VM builds strings without running a constructor.
 */
#include <stdlib.h>

#include "vm.h"

Ref string_from_chars(VM *vm, const u16 *chars, u32 len) {
	Ref value = new_array(vm, vm->c_char_array, (i32)len);
	if (value == 0) return 0;
	if (len) memcpy(ARRAY_PTR(vm, value), chars, (size_t)len * 2);
	ROOT(vm, value);
	Ref s = new_object(vm, vm->c_string);
	UNROOT(vm, 1);
	if (s == 0) return 0;
	*(Ref *)FIELD_PTR(vm, s, vm->f_string_value) = value;
	*(i32 *)FIELD_PTR(vm, s, vm->f_string_count) = (i32)len;
	return s;
}

/* Decodes modified UTF-8; malformed bytes become U+FFFD. */
static u32 decode_utf8(const u8 *s, u32 len, u16 *out) {
	u32 n = 0;
	for (u32 i = 0; i < len;) {
		u8 b = s[i];
		if (b < 0x80) {
			out[n++] = b;
			i++;
		} else if ((b & 0xE0) == 0xC0 && i + 1 < len && (s[i + 1] & 0xC0) == 0x80) {
			out[n++] = (u16)((b & 0x1F) << 6 | (s[i + 1] & 0x3F));
			i += 2;
		} else if ((b & 0xF0) == 0xE0 && i + 2 < len && (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
			out[n++] = (u16)((b & 0x0F) << 12 | (s[i + 1] & 0x3F) << 6 | (s[i + 2] & 0x3F));
			i += 3;
		} else {
			out[n++] = 0xFFFD;
			i++;
		}
	}
	return n;
}

Ref string_from_utf8(VM *vm, const u8 *s, u32 len) {
	u16 small[128];
	u16 *buf = len <= 128 ? small : malloc((size_t)len * 2);
	if (buf == NULL) {
		vm->current->exception = vm->oom_error;
		return 0;
	}
	u32 n = decode_utf8(s, len, buf);
	Ref r = string_from_chars(vm, buf, n);
	if (buf != small) free(buf);
	return r;
}

Ref string_from_cstr(VM *vm, const char *s) {
	return string_from_utf8(vm, (const u8 *)s, (u32)strlen(s));
}

const u16 *string_chars(VM *vm, Ref s, u32 *len) {
	Ref value = *(Ref *)FIELD_PTR(vm, s, vm->f_string_value);
	i32 offset = *(i32 *)FIELD_PTR(vm, s, vm->f_string_offset);
	*len = (u32)*(i32 *)FIELD_PTR(vm, s, vm->f_string_count);
	if (value == 0) {
		*len = 0;
		return NULL;
	}
	return (const u16 *)ARRAY_PTR(vm, value) + offset;
}

char *string_to_utf8(VM *vm, Ref s) {
	u32 len;
	const u16 *c = string_chars(vm, s, &len);
	char *out = malloc((size_t)len * 3 + 1);
	if (out == NULL) return NULL;
	size_t n = 0;
	for (u32 i = 0; i < len; i++) {
		u16 ch = c[i];
		if (ch < 0x80) {
			out[n++] = (char)ch;
		} else if (ch < 0x800) {
			out[n++] = (char)(0xC0 | ch >> 6);
			out[n++] = (char)(0x80 | (ch & 0x3F));
		} else {
			out[n++] = (char)(0xE0 | ch >> 12);
			out[n++] = (char)(0x80 | (ch >> 6 & 0x3F));
			out[n++] = (char)(0x80 | (ch & 0x3F));
		}
	}
	out[n] = '\0';
	return out;
}

/* ---- intern table: open addressing keyed by content ---- */

static u32 chars_hash(const u16 *c, u32 len) {
	u32 h = 0;
	for (u32 i = 0; i < len; i++) h = 31 * h + c[i];
	return h ^ (h >> 16);
}

static int string_equals(VM *vm, Ref a, const u16 *c, u32 len) {
	u32 n;
	const u16 *ac = string_chars(vm, a, &n);
	return n == len && (len == 0 || memcmp(ac, c, (size_t)len * 2) == 0);
}

static int intern_grow(VM *vm) {
	u32 cap = vm->interned_cap ? vm->interned_cap * 2 : 1024;
	Ref *slots = calloc(cap, sizeof(Ref));
	if (slots == NULL) return 0;
	for (u32 i = 0; i < vm->interned_cap; i++) {
		Ref s = vm->interned[i];
		if (s == 0) continue;
		u32 len;
		const u16 *c = string_chars(vm, s, &len);
		u32 j = chars_hash(c, len) & (cap - 1);
		while (slots[j] != 0) j = (j + 1) & (cap - 1);
		slots[j] = s;
	}
	free(vm->interned);
	vm->interned = slots;
	vm->interned_cap = cap;
	return 1;
}

Ref string_intern(VM *vm, Ref s) {
	if ((vm->interned_count + 1) * 2 > vm->interned_cap && !intern_grow(vm)) {
		vm->current->exception = vm->oom_error;
		return 0;
	}
	u32 len;
	const u16 *c = string_chars(vm, s, &len);
	u32 j = chars_hash(c, len) & (vm->interned_cap - 1);
	for (;;) {
		Ref e = vm->interned[j];
		if (e == 0) break;
		if (string_equals(vm, e, c, len)) return e;
		j = (j + 1) & (vm->interned_cap - 1);
	}
	vm->interned[j] = s;
	vm->interned_count++;
	return s;
}

void strings_free(VM *vm) {
	free(vm->interned);
	vm->interned = NULL;
	vm->interned_cap = vm->interned_count = 0;
}
