/*
 * The host extension API (see jmevm.h): host natives, object access for
 * them, blocking on host channels, resources, properties and host state in
 * snapshots.
 */
#include <stdlib.h>

#include "vm.h"

struct JmeCall {
	VM *vm;
	Thread *t;
	Method *m;
	Slot *args;
	Value *ret;
};

static char *dup_str(const char *s) {
	size_t n = strlen(s) + 1;
	char *d = malloc(n);
	if (d != NULL) memcpy(d, s, n);
	return d;
}

/* ---- natives ---- */

JmeStatus jme_register_natives(JmeVM *vm, const JmeNative *natives, size_t count) {
	HostSetup *h = &vm->host;
	HostNatives *a = realloc(h->natives, (h->native_count + 1) * sizeof(HostNatives));
	if (a == NULL) return JME_E_NOMEM;
	h->natives = a;
	h->natives[h->native_count].table = natives;
	h->natives[h->native_count].count = count;
	h->native_count++;
	return JME_OK;
}

JmeNativeFn host_native_find(VM *vm, Method *m) {
	for (u32 i = vm->host.native_count; i-- > 0;) {
		const HostNatives *t = &vm->host.natives[i];
		for (size_t k = 0; k < t->count; k++) {
			const JmeNative *n = &t->table[k];
			if (strcmp(n->cls, m->owner->name) == 0 && strcmp(n->name, m->name) == 0
					&& (n->desc == NULL || strcmp(n->desc, m->desc) == 0))
				return n->fn;
		}
	}
	return NULL;
}

NativeResult host_native_call(VM *vm, Thread *t, Method *m, Slot *args, Value *ret) {
	JmeCall call = {vm, t, m, args, ret};
	int r = m->host_native(&call);
	if (r == JME_NATIVE_BLOCK) return N_REEXECUTE;
	t->block_phase = 0;
	if (r == JME_NATIVE_THROW) {
		if (t->exception == 0) throw_new(vm, "java/lang/InternalError", "a host native threw nothing");
		return N_THROW;
	}
	return vm->finished ? N_YIELD : N_OK;
}

JmeVM *jme_call_vm(JmeCall *call) {
	return call->vm;
}

int32_t jme_arg_int(JmeCall *call, unsigned slot) {
	return (i32)call->args[slot];
}

int64_t jme_arg_long(JmeCall *call, unsigned slot) {
	return slot_get_long(&call->args[slot]);
}

float jme_arg_float(JmeCall *call, unsigned slot) {
	return slot_get_float(&call->args[slot]);
}

double jme_arg_double(JmeCall *call, unsigned slot) {
	return slot_get_double(&call->args[slot]);
}

JmeRef jme_arg_ref(JmeCall *call, unsigned slot) {
	return call->args[slot];
}

void jme_return_int(JmeCall *call, int32_t v) {
	call->ret->i = v;
}

void jme_return_long(JmeCall *call, int64_t v) {
	call->ret->j = v;
}

void jme_return_float(JmeCall *call, float v) {
	call->ret->f = v;
}

void jme_return_double(JmeCall *call, double v) {
	call->ret->d = v;
}

void jme_return_ref(JmeCall *call, JmeRef v) {
	call->ret->r = v;
}

int jme_throw(JmeCall *call, const char *cls, const char *msg) {
	Class *c;
	if (class_load(call->vm, cls, &c) == JME_OK) {
		throw_new(call->vm, cls, msg);
	} else {
		throw_new(call->vm, "java/lang/InternalError", cls);
	}
	return JME_NATIVE_THROW;
}

int jme_block(JmeCall *call, uint32_t channel, int64_t timeout_ms) {
	Thread *t = call->t;
	t->state = TS_HOST_WAIT;
	t->wait_channel = channel;
	t->timed = timeout_ms >= 0;
	if (timeout_ms >= 0) t->wake_tick = call->vm->ticks + (u64)timeout_ms * call->vm->options.ticks_per_ms;
	t->block_phase = 1;
	return JME_NATIVE_BLOCK;
}

int jme_call_woken(JmeCall *call) {
	return call->t->block_phase == 1;
}

void jme_signal(JmeVM *vm, uint32_t channel) {
	for (u32 i = 0; i < vm->thread_count; i++) {
		Thread *t = vm->threads[i];
		if (t->state == TS_HOST_WAIT && t->wait_channel == channel) {
			t->state = TS_RUNNABLE;
			t->timed = 0;
		}
	}
}

int jme_exit(JmeCall *call, int code) {
	VM *vm = call->vm;
	vm->exit_code = code;
	vm->finished = 1;
	for (u32 i = 0; i < vm->thread_count; i++) vm->threads[i]->state = TS_DONE;
	return JME_NATIVE_RETURN;
}

int64_t jme_time_ms(const JmeVM *vm) {
	return vm->options.epoch_ms + (i64)(vm->ticks / vm->options.ticks_per_ms);
}

/* ---- objects ---- */

JmeField jme_field(JmeVM *vm, const char *cls, const char *name, const char *desc) {
	Class *c;
	if (class_load(vm, cls, &c) != JME_OK) return 0;
	Field *f = class_find_field(c, sym_cstr(vm, name), sym_cstr(vm, desc));
	if (f == NULL || (f->access & ACC_STATIC)) return 0;
	return f->offset; /* never 0: fields start after the header */
}

int32_t jme_get_int(JmeVM *vm, JmeRef obj, JmeField f) {
	return *(i32 *)FIELD_PTR(vm, obj, f);
}

void jme_set_int(JmeVM *vm, JmeRef obj, JmeField f, int32_t v) {
	*(i32 *)FIELD_PTR(vm, obj, f) = v;
	CARD_MARK(vm, obj + f);
}

int64_t jme_get_long(JmeVM *vm, JmeRef obj, JmeField f) {
	return *(i64 *)FIELD_PTR(vm, obj, f);
}

void jme_set_long(JmeVM *vm, JmeRef obj, JmeField f, int64_t v) {
	*(i64 *)FIELD_PTR(vm, obj, f) = v;
	CARD_MARK(vm, obj + f);
}

JmeRef jme_get_ref(JmeVM *vm, JmeRef obj, JmeField f) {
	return *(Ref *)FIELD_PTR(vm, obj, f);
}

void jme_set_ref(JmeVM *vm, JmeRef obj, JmeField f, JmeRef v) {
	*(Ref *)FIELD_PTR(vm, obj, f) = v;
	CARD_MARK(vm, obj + f);
}

uint32_t jme_array_length(JmeVM *vm, JmeRef array) {
	return ARRAY_LEN(vm, array);
}

void *jme_array_data(JmeVM *vm, JmeRef array) {
	/* The caller may write anywhere in it */
	heap_mark(vm, array, array + object_size(vm, array));
	return ARRAY_PTR(vm, array);
}

const uint16_t *jme_string_chars(JmeVM *vm, JmeRef s, uint32_t *length) {
	return string_chars(vm, s, length);
}

char *jme_string_utf8(JmeVM *vm, JmeRef s) {
	return string_to_utf8(vm, s);
}

JmeRef jme_new_array(JmeCall *call, const char *array_class, int32_t length) {
	VM *vm = call->vm;
	Class *c;
	if (array_class[0] != '[' || class_load(vm, array_class, &c) != JME_OK) {
		jme_throw(call, "java/lang/NoClassDefFoundError", array_class);
		return 0;
	}
	return new_array(vm, c, length);
}

JmeRef jme_new_string(JmeCall *call, const char *utf8) {
	return string_from_cstr(call->vm, utf8);
}

JmeRef jme_new_string_chars(JmeCall *call, const uint16_t *chars, uint32_t length) {
	return string_from_chars(call->vm, chars, length);
}

void jme_root(JmeVM *vm, JmeRef *ref) {
	vm_root(vm, ref);
}

void jme_unroot(JmeVM *vm, unsigned n) {
	vm_unroot(vm, n);
}

/* ---- resources and properties ---- */

void jme_set_resource_source(JmeVM *vm, JmeResourceSource source, void *user) {
	vm->host.resources = source;
	vm->host.resources_user = user;
}

JmeStatus jme_set_property(JmeVM *vm, const char *key, const char *value) {
	HostSetup *h = &vm->host;
	char *v = dup_str(value);
	if (v == NULL) return JME_E_NOMEM;
	for (u32 i = 0; i < h->prop_count; i++) {
		if (strcmp(h->props[i].key, key) == 0) {
			free(h->props[i].value);
			h->props[i].value = v;
			return JME_OK;
		}
	}
	HostProperty *a = realloc(h->props, (h->prop_count + 1) * sizeof(HostProperty));
	char *k = dup_str(key);
	if (a != NULL) h->props = a;
	if (a == NULL || k == NULL) {
		free(v);
		free(k);
		return JME_E_NOMEM;
	}
	h->props[h->prop_count].key = k;
	h->props[h->prop_count].value = v;
	h->prop_count++;
	return JME_OK;
}

const char *host_property(VM *vm, const char *key) {
	for (u32 i = 0; i < vm->host.prop_count; i++)
		if (strcmp(vm->host.props[i].key, key) == 0) return vm->host.props[i].value;
	return NULL;
}

/* ---- state ---- */

JmeStatus jme_register_state(JmeVM *vm, const char *name, JmeStateSave save, JmeStateLoad load, void *user) {
	HostSetup *h = &vm->host;
	for (u32 i = 0; i < h->state_count; i++)
		if (strcmp(h->states[i].name, name) == 0) return JME_E_ARGUMENT;
	HostState *a = realloc(h->states, (h->state_count + 1) * sizeof(HostState));
	char *n = dup_str(name);
	if (a != NULL) h->states = a;
	if (a == NULL || n == NULL) {
		free(n);
		return JME_E_NOMEM;
	}
	HostState *s = &h->states[h->state_count++];
	s->name = n;
	s->save = save;
	s->load = load;
	s->user = user;
	return JME_OK;
}

/* ---- copying and freeing the setup ---- */

JmeStatus host_setup_copy(VM *to, const VM *from) {
	const HostSetup *h = &from->host;
	for (u32 i = 0; i < h->native_count; i++)
		if (jme_register_natives(to, h->natives[i].table, h->natives[i].count) != JME_OK) return JME_E_NOMEM;
	for (u32 i = 0; i < h->state_count; i++) {
		const HostState *s = &h->states[i];
		if (jme_register_state(to, s->name, s->save, s->load, s->user) != JME_OK) return JME_E_NOMEM;
	}
	for (u32 i = 0; i < h->prop_count; i++)
		if (jme_set_property(to, h->props[i].key, h->props[i].value) != JME_OK) return JME_E_NOMEM;
	to->host.resources = h->resources;
	to->host.resources_user = h->resources_user;
	return JME_OK;
}

void host_setup_free(VM *vm) {
	HostSetup *h = &vm->host;
	for (u32 i = 0; i < h->state_count; i++) free(h->states[i].name);
	for (u32 i = 0; i < h->prop_count; i++) {
		free(h->props[i].key);
		free(h->props[i].value);
	}
	free(h->natives);
	free(h->states);
	free(h->props);
	memset(h, 0, sizeof *h);
}
