/*
 * Class loading, linking, resolution and initialization.
 *
 * Loading and linking never allocate on the Java heap and never run Java
 * code, so they can happen in the middle of any instruction. Class ids and
 * method ids are handed out in load order, which is deterministic.
 * Initialization runs <clinit> as ordinary frames: class_init() pushes the
 * frame and the instruction that needed the class runs again afterwards.
 */
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

#define ALIGN8(n) (((n) + 7u) & ~7u)

static const char *E_NOCLASS = "java/lang/NoClassDefFoundError";
static const char *E_FORMAT = "java/lang/ClassFormatError";
static const char *E_CHANGE = "java/lang/IncompatibleClassChangeError";
static const char *E_CIRCULAR = "java/lang/ClassCircularityError";
static const char *E_OOM = "java/lang/OutOfMemoryError";

/* ---- class map (interned name -> Class) ---- */

static u32 ptr_hash(const void *p) {
	uintptr_t v = (uintptr_t)p;
	v ^= v >> 17;
	v *= 0xed5ad4bbu;
	v ^= v >> 11;
	return (u32)v;
}

static Class *map_get(VM *vm, const char *name) {
	ClassMap *m = &vm->class_map;
	if (m->cap == 0) return NULL;
	for (u32 j = ptr_hash(name) & (m->cap - 1);; j = (j + 1) & (m->cap - 1)) {
		Class *c = m->slots[j];
		if (c == NULL) return NULL;
		if (c->name == name) return c;
	}
}

static int map_put(VM *vm, Class *c) {
	ClassMap *m = &vm->class_map;
	if ((m->count + 1) * 2 > m->cap) {
		u32 cap = m->cap ? m->cap * 2 : 256;
		Class **slots = calloc(cap, sizeof(Class *));
		if (slots == NULL) return 0;
		for (u32 i = 0; i < m->cap; i++) {
			Class *e = m->slots[i];
			if (e == NULL) continue;
			u32 j = ptr_hash(e->name) & (cap - 1);
			while (slots[j] != NULL) j = (j + 1) & (cap - 1);
			slots[j] = e;
		}
		free(m->slots);
		m->slots = slots;
		m->cap = cap;
	}
	u32 j = ptr_hash(c->name) & (m->cap - 1);
	while (m->slots[j] != NULL) j = (j + 1) & (m->cap - 1);
	m->slots[j] = c;
	m->count++;
	return 1;
}

/* Gives c and its methods their ids and makes it findable by name. */
static int register_class(VM *vm, Class *c) {
	if (vm->class_count == vm->class_cap) {
		u32 cap = vm->class_cap ? vm->class_cap * 2 : 256;
		Class **a = realloc(vm->classes, cap * sizeof(Class *));
		if (a == NULL) return 0;
		vm->classes = a;
		vm->class_cap = cap;
	}
	if (vm->method_count + c->method_count > vm->method_cap) {
		u32 cap = vm->method_cap ? vm->method_cap : 1024;
		while (cap < vm->method_count + c->method_count) cap *= 2;
		Method **a = realloc(vm->methods, cap * sizeof(Method *));
		if (a == NULL) return 0;
		vm->methods = a;
		vm->method_cap = cap;
	}
	if (!map_put(vm, c)) return 0;
	c->id = vm->class_count;
	vm->classes[vm->class_count++] = c;
	for (u32 i = 0; i < c->method_count; i++) {
		c->methods[i].id = vm->method_count;
		vm->methods[vm->method_count++] = &c->methods[i];
	}
	return 1;
}

static void free_class(Class *c) {
	classfile_free(c->cf);
	pool_free_all(&c->pool);
	free(c);
}

void class_free_all(VM *vm) {
	for (u32 i = 0; i < vm->class_count; i++) free_class(vm->classes[i]);
	free(vm->classes);
	free(vm->methods);
	free(vm->class_map.slots);
	free(vm->loading);
	if (vm->boot_class != NULL) free_class(vm->boot_class);
	vm->classes = NULL;
	vm->methods = NULL;
	vm->class_map.slots = NULL;
	vm->loading = NULL;
	vm->boot_class = NULL;
}

/* ---- descriptors ---- */

/* Argument slots of a method descriptor, without this. */
static u16 desc_arg_slots(const char *d) {
	u16 n = 0;
	for (d++; *d && *d != ')'; d++) {
		if (*d == 'J' || *d == 'D') {
			n += 2;
			continue;
		}
		n++;
		while (*d == '[') d++;
		if (*d == 'L') {
			while (*d && *d != ';') d++;
		}
	}
	return n;
}

static u8 desc_ret_type(const char *d) {
	const char *r = strchr(d, ')');
	if (r == NULL) return 'V';
	switch (r[1]) {
	case 'V': case 'J': case 'F': case 'D': return (u8)r[1];
	case 'L': case '[': return 'L';
	default: return 'I';
	}
}

static int is_ref_type(u8 t) {
	return t == 'L' || t == '[';
}

/* ---- linking ---- */

static int add_interface(Class *c, Class *i) {
	for (u32 k = 0; k < c->all_interface_count; k++)
		if (c->all_interfaces[k] == i) return 0;
	c->all_interfaces[c->all_interface_count++] = i;
	return 1;
}

static const char *link_class(VM *vm, Class *c) {
	ClassFile *cf = c->cf;
	Class *super = c->super;
	int is_interface = (c->access & ACC_INTERFACE) != 0;

	/* every interface implemented, directly or through super */
	u32 max = super ? super->all_interface_count : 0;
	for (u32 i = 0; i < c->interface_count; i++) max += 1 + c->interfaces[i]->all_interface_count;
	c->all_interfaces = pool_alloc(&c->pool, (max + 1) * sizeof(Class *));
	if (c->all_interfaces == NULL) return E_OOM;
	if (super != NULL)
		for (u32 k = 0; k < super->all_interface_count; k++) add_interface(c, super->all_interfaces[k]);
	for (u32 i = 0; i < c->interface_count; i++) {
		Class *in = c->interfaces[i];
		add_interface(c, in);
		for (u32 k = 0; k < in->all_interface_count; k++) add_interface(c, in->all_interfaces[k]);
	}

	/* fields: wide ones first so they are 8-byte aligned */
	c->field_count = cf->field_count;
	c->fields = pool_alloc(&c->pool, (size_t)cf->field_count * sizeof(Field));
	if (c->fields == NULL) return E_OOM;
	u32 offset = super ? super->instance_size : OBJ_HEADER;
	u32 statics = 0, own_refs = 0;
	for (int pass = 0; pass < 2; pass++) {
		for (u32 i = 0; i < cf->field_count; i++) {
			const FieldInfo *fi = &cf->fields[i];
			Field *f = &c->fields[i];
			f->owner = c;
			f->name = sym_utf8(vm, fi->name);
			f->desc = sym_utf8(vm, fi->desc);
			f->access = fi->access;
			f->type = (u8)f->desc[0];
			f->const_index = fi->const_index;
			int wide = f->type == 'J' || f->type == 'D';
			if (fi->access & ACC_STATIC) {
				if (pass == 0) {
					f->offset = statics;
					statics += wide ? 2 : 1;
				}
				continue;
			}
			if (wide != (pass == 0)) continue;
			if (wide) offset = ALIGN8(offset);
			f->offset = offset;
			offset += wide ? 8 : 4;
			if (is_ref_type(f->type)) own_refs++;
		}
	}
	c->instance_size = ALIGN8(offset);

	c->is_reference = super != NULL && super->is_reference;
	int defines_referent = c->name == sym_cstr(vm, "java/lang/ref/Reference");
	if (defines_referent) c->is_reference = 1;
	u32 inherited = super ? super->ref_count : 0;
	c->ref_offsets = pool_alloc(&c->pool, (inherited + own_refs + 1) * sizeof(u32));
	if (c->ref_offsets == NULL) return E_OOM;
	if (inherited) memcpy(c->ref_offsets, super->ref_offsets, inherited * sizeof(u32));
	c->ref_count = inherited;
	for (u32 i = 0; i < c->field_count; i++) {
		Field *f = &c->fields[i];
		if ((f->access & ACC_STATIC) || !is_ref_type(f->type)) continue;
		if (defines_referent && f->name == sym_cstr(vm, "referent")) {
			/* traced weakly by the collector */
			vm->referent_offset = f->offset;
			continue;
		}
		c->ref_offsets[c->ref_count++] = f->offset;
	}

	/* static storage; numeric constants are set now, strings at initialization */
	c->static_count = statics;
	c->statics = pool_alloc(&c->pool, (statics + 1) * sizeof(Slot));
	c->static_tags = pool_alloc(&c->pool, statics + 1);
	if (c->statics == NULL || c->static_tags == NULL) return E_OOM;
	for (u32 i = 0; i < c->field_count; i++) {
		Field *f = &c->fields[i];
		if (!(f->access & ACC_STATIC)) continue;
		if (is_ref_type(f->type)) c->static_tags[f->offset] = 1;
		if (f->const_index == 0) continue;
		const CpEntry *k = &cf->cp[f->const_index];
		if (k->tag == CP_INTEGER || k->tag == CP_FLOAT) {
			c->statics[f->offset] = k->u.bits32;
		} else if (k->tag == CP_LONG || k->tag == CP_DOUBLE) {
			memcpy(&c->statics[f->offset], &k->u.bits64, 8);
		}
	}

	/* methods */
	c->method_count = cf->method_count;
	c->methods = pool_alloc(&c->pool, (size_t)cf->method_count * sizeof(Method));
	if (c->methods == NULL) return E_OOM;
	for (u32 i = 0; i < cf->method_count; i++) {
		const MethodInfo *mi = &cf->methods[i];
		Method *m = &c->methods[i];
		m->owner = c;
		m->name = sym_utf8(vm, mi->name);
		m->desc = sym_utf8(vm, mi->desc);
		if (m->desc[0] != '(') return E_FORMAT;
		m->access = mi->access;
		m->arg_slots = (u16)(desc_arg_slots(m->desc) + ((mi->access & ACC_STATIC) ? 0 : 1));
		m->ret_type = desc_ret_type(m->desc);
		m->code = mi->code;
		m->vtable_index = -1;
	}

	/* vtable: super's, with overrides replaced and new methods appended */
	if (!is_interface) {
		u32 base = super ? super->vtable_len : 0;
		c->vtable = pool_alloc(&c->pool, (base + c->method_count + 1) * sizeof(Method *));
		if (c->vtable == NULL) return E_OOM;
		if (base) memcpy(c->vtable, super->vtable, base * sizeof(Method *));
		u32 len = base;
		for (u32 i = 0; i < c->method_count; i++) {
			Method *m = &c->methods[i];
			if ((m->access & (ACC_STATIC | ACC_PRIVATE)) || m->name[0] == '<') continue;
			u32 k = 0;
			while (k < base && !(c->vtable[k]->name == m->name && c->vtable[k]->desc == m->desc)) k++;
			if (k == base) k = len++;
			c->vtable[k] = m;
			m->vtable_index = (i32)k;
		}
		c->vtable_len = len;
	}

	c->resolved = pool_alloc(&c->pool, (size_t)cf->cp_count * sizeof(Resolved));
	c->resolved_flag = pool_alloc(&c->pool, cf->cp_count);
	if (c->resolved == NULL || c->resolved_flag == NULL) return E_OOM;
	c->state = CS_LINKED;
	return NULL;
}

/* ---- loading ---- */

static Class *load(VM *vm, const char *name, const char **err);

static Class *new_class(const char *name) {
	Class *c = calloc(1, sizeof(Class));
	if (c == NULL) return NULL;
	pool_init(&c->pool);
	c->name = name;
	return c;
}

static Class *load_array(VM *vm, const char *name, const char **err) {
	static const struct { char ch; u8 type, size; } prims[] = {
		{'Z', T_BOOLEAN, 1}, {'B', T_BYTE, 1}, {'C', T_CHAR, 2}, {'S', T_SHORT, 2},
		{'I', T_INT, 4}, {'F', T_FLOAT, 4}, {'J', T_LONG, 8}, {'D', T_DOUBLE, 8},
	};
	Class *component = NULL;
	u8 type = T_OBJECT, size = 4;
	const char *e = name + 1;
	size_t len = strlen(e);
	if (e[0] == '[') {
		component = load(vm, sym_cstr(vm, e), err);
		if (component == NULL) return NULL;
	} else if (e[0] == 'L' && len > 2 && e[len - 1] == ';') {
		component = load(vm, sym_intern(vm, e + 1, len - 2), err);
		if (component == NULL) return NULL;
	} else {
		size_t i = 0;
		while (i < sizeof prims / sizeof prims[0] && !(len == 1 && prims[i].ch == e[0])) i++;
		if (i == sizeof prims / sizeof prims[0]) {
			*err = E_NOCLASS;
			return NULL;
		}
		type = prims[i].type;
		size = prims[i].size;
	}
	Class *object = vm->c_object;
	if (object == NULL) {
		object = load(vm, sym_cstr(vm, "java/lang/Object"), err);
		if (object == NULL) return NULL;
	}
	Class *c = new_class(name);
	if (c == NULL) {
		*err = E_OOM;
		return NULL;
	}
	c->kind = CK_ARRAY;
	c->state = CS_INITIALIZED;
	c->access = ACC_PUBLIC | ACC_FINAL;
	c->super = object;
	c->component = component;
	c->elem_type = type;
	c->elem_size = size;
	c->vtable = object->vtable;
	c->vtable_len = object->vtable_len;
	c->instance_size = ARRAY_DATA;
	if (!register_class(vm, c)) {
		free_class(c);
		*err = E_OOM;
		return NULL;
	}
	if (component != NULL) component->array_class = c;
	return c;
}

static int loading_push(VM *vm, const char *name) {
	for (u32 i = 0; i < vm->loading_count; i++)
		if (vm->loading[i] == name) return 0;
	if (vm->loading_count == vm->loading_cap) {
		u32 cap = vm->loading_cap ? vm->loading_cap * 2 : 32;
		const char **a = realloc(vm->loading, cap * sizeof(char *));
		if (a == NULL) return 0;
		vm->loading = a;
		vm->loading_cap = cap;
	}
	vm->loading[vm->loading_count++] = name;
	return 1;
}

static Class *load_instance_class(VM *vm, const char *name, const char **err) {
	const u8 *data;
	size_t len;
	if (vm->class_source == NULL || vm->class_source(vm->class_source_user, name, &data, &len) != JME_OK) {
		*err = E_NOCLASS;
		return NULL;
	}
	ClassFile *cf;
	if (classfile_parse(data, len, &cf, NULL) != JME_OK) {
		*err = E_FORMAT;
		return NULL;
	}
	if (sym_utf8(vm, cf->name) != name) {
		classfile_free(cf);
		*err = E_NOCLASS;
		return NULL;
	}
	Class *c = new_class(name);
	if (c == NULL) {
		classfile_free(cf);
		*err = E_OOM;
		return NULL;
	}
	c->cf = cf;
	c->bytes_hash = fnv1a64(data, len);
	c->access = cf->access;
	c->kind = CK_INSTANCE;

	if (!loading_push(vm, name)) {
		free_class(c);
		*err = E_CIRCULAR;
		return NULL;
	}
	const char *e = NULL;
	if (cf->super_name.bytes != NULL) {
		c->super = load(vm, sym_utf8(vm, cf->super_name), &e);
		if (c->super != NULL && (c->super->access & ACC_INTERFACE)) e = E_CHANGE;
	}
	if (e == NULL) {
		c->interface_count = cf->interface_count;
		c->interfaces = pool_alloc(&c->pool, (cf->interface_count + 1) * sizeof(Class *));
		if (c->interfaces == NULL) e = E_OOM;
		for (u32 i = 0; e == NULL && i < cf->interface_count; i++) {
			c->interfaces[i] = load(vm, sym_utf8(vm, cf->interfaces[i]), &e);
			if (e == NULL && !(c->interfaces[i]->access & ACC_INTERFACE)) e = E_CHANGE;
		}
	}
	vm->loading_count--;
	if (e == NULL) e = link_class(vm, c);
	if (e == NULL && !register_class(vm, c)) e = E_OOM;
	if (e != NULL) {
		free_class(c);
		*err = e;
		return NULL;
	}
	return c;
}

static Class *load(VM *vm, const char *name, const char **err) {
	Class *c = map_get(vm, name);
	if (c != NULL) return c;
	return name[0] == '[' ? load_array(vm, name, err) : load_instance_class(vm, name, err);
}

JmeStatus class_load(VM *vm, const char *name, Class **out) {
	const char *err = NULL;
	*out = load(vm, sym_cstr(vm, name), &err);
	if (*out != NULL) return JME_OK;
	if (err == E_NOCLASS) return JME_E_NOTFOUND;
	if (err == E_OOM) return JME_E_NOMEM;
	return JME_E_FORMAT;
}

Class *class_load_or_throw(VM *vm, const char *name) {
	const char *err = NULL;
	Class *c = load(vm, sym_cstr(vm, name), &err);
	if (c == NULL) throw_new(vm, err, name);
	return c;
}

Class *array_class_of(VM *vm, Class *component) {
	if (component->array_class != NULL) return component->array_class;
	size_t n = strlen(component->name);
	char *name = malloc(n + 4);
	if (name == NULL) {
		vm->current->exception = vm->oom_error;
		return NULL;
	}
	if (component->kind == CK_ARRAY) {
		snprintf(name, n + 4, "[%s", component->name);
	} else {
		snprintf(name, n + 4, "[L%s;", component->name);
	}
	Class *c = class_load_or_throw(vm, name);
	free(name);
	return c;
}

Class *primitive_array_class(VM *vm, u8 elem_type) {
	static const char *names[] = {"[Z", "[C", "[F", "[D", "[B", "[S", "[I", "[J"};
	if (elem_type < T_BOOLEAN || elem_type > T_LONG) return NULL;
	return class_load_or_throw(vm, names[elem_type - T_BOOLEAN]);
}

/* ---- type checks and lookups ---- */

int class_is_subclass(Class *c, Class *super) {
	for (; c != NULL; c = c->super)
		if (c == super) return 1;
	return 0;
}

int class_is_assignable(Class *from, Class *to) {
	if (from == to) return 1;
	if (to->access & ACC_INTERFACE) {
		for (u32 i = 0; i < from->all_interface_count; i++)
			if (from->all_interfaces[i] == to) return 1;
		return 0;
	}
	if (from->kind == CK_ARRAY) {
		if (to->kind != CK_ARRAY) return to->super == NULL;
		if (from->elem_type != T_OBJECT || to->elem_type != T_OBJECT) return 0;
		return class_is_assignable(from->component, to->component);
	}
	return class_is_subclass(from, to);
}

Field *class_find_field(Class *c, const char *name, const char *desc) {
	for (; c != NULL; c = c->super) {
		for (u32 i = 0; i < c->field_count; i++)
			if (c->fields[i].name == name && c->fields[i].desc == desc) return &c->fields[i];
		for (u32 i = 0; i < c->interface_count; i++) {
			Field *f = class_find_field(c->interfaces[i], name, desc);
			if (f != NULL) return f;
		}
	}
	return NULL;
}

Method *class_find_own_method(Class *c, const char *name, const char *desc) {
	for (u32 i = 0; i < c->method_count; i++)
		if (c->methods[i].name == name && c->methods[i].desc == desc) return &c->methods[i];
	return NULL;
}

Method *class_find_method(Class *c, const char *name, const char *desc) {
	for (Class *k = c; k != NULL; k = k->super) {
		Method *m = class_find_own_method(k, name, desc);
		if (m != NULL) return m;
	}
	for (u32 i = 0; i < c->all_interface_count; i++) {
		Method *m = class_find_own_method(c->all_interfaces[i], name, desc);
		if (m != NULL) return m;
	}
	return NULL;
}

Method *class_find_virtual(Class *c, const char *name, const char *desc) {
	for (u32 i = c->vtable_len; i-- > 0;) {
		Method *m = c->vtable[i];
		if (m->name == name && m->desc == desc) return m;
	}
	return NULL;
}

u32 field_offset(VM *vm, Class *c, const char *name) {
	const char *n = sym_cstr(vm, name);
	for (Class *k = c; k != NULL; k = k->super)
		for (u32 i = 0; i < k->field_count; i++)
			if (k->fields[i].name == n && !(k->fields[i].access & ACC_STATIC)) return k->fields[i].offset;
	fprintf(stderr, "jmevm: class library: %s has no field %s\n", c->name, name);
	abort();
}

void class_java_name(Class *c, char *buf, size_t cap) {
	size_t i = 0;
	for (const char *p = c->name; *p && i + 1 < cap; p++) buf[i++] = *p == '/' ? '.' : *p;
	buf[i] = '\0';
}

Ref class_mirror(VM *vm, Class *c) {
	if (c->mirror == 0) {
		Ref m = new_object(vm, vm->c_class);
		if (m == 0) return 0;
		*(i32 *)FIELD_PTR(vm, m, vm->f_class_id) = (i32)c->id;
		CARD_MARK(vm, m + vm->f_class_id);
		c->mirror = m;
	}
	return c->mirror;
}

Class *mirror_class(VM *vm, Ref mirror) {
	return vm->classes[*(i32 *)FIELD_PTR(vm, mirror, vm->f_class_id)];
}

/* ---- resolution ---- */

static Utf8 cp_utf8(Class *cur, u16 index) {
	return cur->cf->cp[index].u.utf8;
}

Class *resolve_class(VM *vm, Class *cur, u16 index) {
	if (cur->resolved_flag[index]) return cur->resolved[index].cls;
	Utf8 u = cp_utf8(cur, cur->cf->cp[index].u.index);
	const char *err = NULL;
	Class *c = load(vm, sym_utf8(vm, u), &err);
	if (c == NULL) {
		throw_new(vm, err, sym_utf8(vm, u));
		return NULL;
	}
	cur->resolved[index].cls = c;
	cur->resolved_flag[index] = 1;
	return c;
}

static void member_name(VM *vm, Class *cur, u16 index, const char **name, const char **desc) {
	const CpEntry *nat = &cur->cf->cp[cur->cf->cp[index].u.pair.b];
	*name = sym_utf8(vm, cp_utf8(cur, nat->u.pair.a));
	*desc = sym_utf8(vm, cp_utf8(cur, nat->u.pair.b));
}

Field *resolve_field(VM *vm, Class *cur, u16 index, int is_static) {
	if (cur->resolved_flag[index]) return cur->resolved[index].field;
	Class *c = resolve_class(vm, cur, cur->cf->cp[index].u.pair.a);
	if (c == NULL) return NULL;
	const char *name, *desc;
	member_name(vm, cur, index, &name, &desc);
	Field *f = class_find_field(c, name, desc);
	if (f == NULL) {
		throw_new(vm, "java/lang/NoSuchFieldError", name);
		return NULL;
	}
	if (((f->access & ACC_STATIC) != 0) != (is_static != 0)) {
		throw_new(vm, E_CHANGE, name);
		return NULL;
	}
	cur->resolved[index].field = f;
	cur->resolved_flag[index] = 1;
	return f;
}

Method *resolve_method(VM *vm, Class *cur, u16 index, int interface_call) {
	if (cur->resolved_flag[index]) return cur->resolved[index].method;
	Class *c = resolve_class(vm, cur, cur->cf->cp[index].u.pair.a);
	if (c == NULL) return NULL;
	const char *name, *desc;
	member_name(vm, cur, index, &name, &desc);
	Method *m;
	if (interface_call) {
		m = class_find_own_method(c, name, desc);
		for (u32 i = 0; m == NULL && i < c->all_interface_count; i++)
			m = class_find_own_method(c->all_interfaces[i], name, desc);
		if (m == NULL) m = class_find_own_method(vm->c_object, name, desc);
	} else {
		m = class_find_method(c, name, desc);
	}
	if (m == NULL) {
		throw_new(vm, "java/lang/NoSuchMethodError", name);
		return NULL;
	}
	cur->resolved[index].method = m;
	cur->resolved_flag[index] = 1;
	return m;
}

Ref resolve_string(VM *vm, Class *cur, u16 index) {
	if (cur->resolved_flag[index]) return cur->resolved[index].str;
	Utf8 u = cp_utf8(cur, cur->cf->cp[index].u.index);
	Ref s = string_from_utf8(vm, u.bytes, u.len);
	if (s == 0) return 0;
	s = string_intern(vm, s);
	if (s == 0) return 0;
	cur->resolved[index].str = s;
	cur->resolved_flag[index] = 1;
	return s;
}

/* ---- initialization ---- */

/* Static final String fields get their ConstantValue. */
static int set_string_constants(VM *vm, Class *c) {
	for (u32 i = 0; i < c->field_count; i++) {
		Field *f = &c->fields[i];
		if (!(f->access & ACC_STATIC) || f->const_index == 0) continue;
		if (c->cf->cp[f->const_index].tag != CP_STRING) continue;
		Ref s = resolve_string(vm, c, f->const_index);
		if (s == 0) return 0;
		c->statics[f->offset] = s;
	}
	return 1;
}

void class_init_finished(VM *vm, Class *c) {
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *w = vm->threads[i];
		if (w->state == TS_INIT_WAIT && w->init_wait == c) {
			w->init_wait = NULL;
			w->state = TS_RUNNABLE;
		}
	}
}

int class_init(VM *vm, Thread *t, Class *c) {
	for (;;) {
		if (c->state == CS_INITIALIZED) return INIT_DONE;
		if (c->state == CS_INITIALIZING && c->init_thread == t->id) return INIT_DONE;
		if (c->state == CS_ERROR) {
			throw_new(vm, E_NOCLASS, c->name);
			return INIT_THROWN;
		}
		/* The topmost superclass that still needs initializing goes first. */
		Class *target = c;
		for (Class *k = c->super; k != NULL; k = k->super)
			if (k->state != CS_INITIALIZED && !(k->state == CS_INITIALIZING && k->init_thread == t->id))
				target = k;
		if (target->state == CS_ERROR) {
			throw_new(vm, E_NOCLASS, target->name);
			return INIT_THROWN;
		}
		if (target->state == CS_INITIALIZING) {
			/* another thread is running its <clinit> */
			t->state = TS_INIT_WAIT;
			t->init_wait = target;
			return INIT_BLOCKED;
		}
		target->state = CS_INITIALIZING;
		target->init_thread = t->id;
		if (!set_string_constants(vm, target)) {
			target->state = CS_ERROR;
			class_init_finished(vm, target);
			return INIT_THROWN;
		}
		Method *clinit = class_find_own_method(target, vm->sym.clinit, vm->sym.void_desc);
		if (clinit == NULL) {
			target->state = CS_INITIALIZED;
			class_init_finished(vm, target);
			continue;
		}
		if (!frame_push(vm, t, clinit, F_CLINIT)) return INIT_THROWN;
		t->frames[t->frame_count - 1].init_class = target;
		return INIT_PUSHED;
	}
}
