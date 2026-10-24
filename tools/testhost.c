/*
 * A test host: natives for tests/java/TestHost.java that use every part of
 * the host API, host state saved in snapshots, an input queue fed by the
 * step hook and read by a blocking native.
 */
#include "testhost.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVENTS 1        /* the channel nextEvent() blocks on */
#define QUEUE 16
#define EVERY_MS 7

typedef struct State {
	int32_t counter;
	int32_t queue[QUEUE];
	int32_t head, count;
	int32_t posted;     /* events posted so far */
	int64_t next_ms;    /* when the next one is due */
} State;

typedef struct Host {
	struct Host *next;
	JmeVM *vm;
	State s;
} Host;

static Host *hosts;

static Host *host_of(JmeVM *vm) {
	for (Host *h = hosts; h != NULL; h = h->next)
		if (h->vm == vm) return h;
	return NULL;
}

/* A loaded snapshot keeps the registrations, so the same Host serves the VM
 * the state was loaded into. */
static Host *host_for(JmeCall *call) {
	JmeVM *vm = jme_call_vm(call);
	Host *h = host_of(vm);
	return h;
}

/* ---- natives of TestHost ---- */

static int counter(JmeCall *c) {
	jme_return_int(c, ++host_for(c)->s.counter);
	return JME_NATIVE_RETURN;
}

/* int nextEvent(int timeoutMs): the next event, -1 on timeout. */
static int next_event(JmeCall *c) {
	State *s = &host_for(c)->s;
	if (s->count > 0) {
		jme_return_int(c, s->queue[s->head]);
		s->head = (s->head + 1) % QUEUE;
		s->count--;
		return JME_NATIVE_RETURN;
	}
	int32_t timeout = jme_arg_int(c, 0);
	if (jme_call_woken(c) || timeout == 0) {
		jme_return_int(c, -1);
		return JME_NATIVE_RETURN;
	}
	return jme_block(c, EVENTS, timeout);
}

static JmeField field(JmeVM *vm, const char *name, const char *desc) {
	return jme_field(vm, "TestHost$Point", name, desc);
}

/* int sum(Point p): x + y + (int) big */
static int sum(JmeCall *c) {
	JmeVM *vm = jme_call_vm(c);
	JmeRef p = jme_arg_ref(c, 0);
	if (p == 0) return jme_throw(c, "java/lang/NullPointerException", NULL);
	int64_t big = jme_get_long(vm, p, field(vm, "big", "J"));
	jme_return_int(c, jme_get_int(vm, p, field(vm, "x", "I")) + jme_get_int(vm, p, field(vm, "y", "I")) + (int32_t)big);
	return JME_NATIVE_RETURN;
}

/* void move(Point p, int dx, int dy): also sets p.tag to "moved" */
static int move(JmeCall *c) {
	JmeVM *vm = jme_call_vm(c);
	JmeRef tag = jme_new_string(c, "moved");
	if (tag == 0) return JME_NATIVE_THROW;
	JmeRef p = jme_arg_ref(c, 0); /* read after allocating */
	JmeField x = field(vm, "x", "I"), y = field(vm, "y", "I");
	jme_set_int(vm, p, x, jme_get_int(vm, p, x) + jme_arg_int(c, 1));
	jme_set_int(vm, p, y, jme_get_int(vm, p, y) + jme_arg_int(c, 2));
	jme_set_ref(vm, p, field(vm, "tag", "Ljava/lang/Object;"), tag);
	return JME_NATIVE_RETURN;
}

/* int[] squares(int n) */
static int squares(JmeCall *c) {
	int32_t n = jme_arg_int(c, 0);
	JmeRef a = jme_new_array(c, "[I", n);
	if (a == 0) return JME_NATIVE_THROW;
	int32_t *e = jme_array_data(jme_call_vm(c), a);
	for (int32_t i = 0; i < n; i++) e[i] = i * i;
	jme_return_ref(c, a);
	return JME_NATIVE_RETURN;
}

/* String[] words(int n): "w0", "w1", ... (one allocation per element) */
static int words(JmeCall *c) {
	JmeVM *vm = jme_call_vm(c);
	int32_t n = jme_arg_int(c, 0);
	JmeRef a = jme_new_array(c, "[Ljava/lang/String;", n);
	if (a == 0) return JME_NATIVE_THROW;
	jme_root(vm, &a);
	for (int32_t i = 0; i < n; i++) {
		char text[16];
		snprintf(text, sizeof text, "w%d", (int)i);
		JmeRef s = jme_new_string(c, text);
		if (s == 0) {
			jme_unroot(vm, 1);
			return JME_NATIVE_THROW;
		}
		((JmeRef *)jme_array_data(vm, a))[i] = s;
	}
	jme_unroot(vm, 1);
	jme_return_ref(c, a);
	return JME_NATIVE_RETURN;
}

/* String shout(String s): upper case, ASCII only */
static int shout(JmeCall *c) {
	JmeVM *vm = jme_call_vm(c);
	uint32_t n;
	const uint16_t *chars = jme_string_chars(vm, jme_arg_ref(c, 0), &n);
	uint16_t *up = malloc((n ? n : 1) * sizeof(uint16_t));
	if (up == NULL) return jme_throw(c, "java/lang/OutOfMemoryError", NULL);
	for (uint32_t i = 0; i < n; i++) up[i] = chars[i] >= 'a' && chars[i] <= 'z' ? (uint16_t)(chars[i] - 32) : chars[i];
	JmeRef s = jme_new_string_chars(c, up, n);
	free(up);
	if (s == 0) return JME_NATIVE_THROW;
	jme_return_ref(c, s);
	return JME_NATIVE_RETURN;
}

/* void fail(String msg): throws IllegalStateException(msg) */
static int fail(JmeCall *c) {
	char *msg = jme_string_utf8(jme_call_vm(c), jme_arg_ref(c, 0));
	jme_throw(c, "java/lang/IllegalStateException", msg);
	free(msg);
	return JME_NATIVE_THROW;
}

static int twice(JmeCall *c) {
	jme_return_long(c, jme_arg_long(c, 0) * 2);
	return JME_NATIVE_RETURN;
}

static int half(JmeCall *c) {
	jme_return_double(c, jme_arg_double(c, 0) / 2);
	return JME_NATIVE_RETURN;
}

static int quit(JmeCall *c) {
	return jme_exit(c, jme_arg_int(c, 0));
}

static const JmeNative natives[] = {
	{"TestHost", "counter", "()I", counter},
	{"TestHost", "nextEvent", NULL, next_event},
	{"TestHost", "sum", NULL, sum},
	{"TestHost", "move", NULL, move},
	{"TestHost", "squares", NULL, squares},
	{"TestHost", "words", NULL, words},
	{"TestHost", "shout", NULL, shout},
	{"TestHost", "fail", NULL, fail},
	{"TestHost", "twice", NULL, twice},
	{"TestHost", "half", NULL, half},
	{"TestHost", "quit", NULL, quit},
};

/* ---- state ---- */

static JmeStatus save(void *user, JmeWriteFn write, void *write_user) {
	Host *h = user;
	return write(write_user, &h->s, sizeof h->s);
}

static JmeStatus load(void *user, const void *data, size_t len) {
	State s;
	if (len != sizeof s) return JME_E_FORMAT;
	memcpy(&s, data, len);
	if (s.head < 0 || s.head >= QUEUE || s.count < 0 || s.count > QUEUE) return JME_E_FORMAT;
	((Host *)user)->s = s;
	return JME_OK;
}

static const char greeting[] = "Hello from the host";

static JmeStatus resource(void *user, const char *name, const uint8_t **data, size_t *len) {
	(void)user;
	if (strcmp(name, "greeting.txt") != 0) return JME_E_NOTFOUND;
	*data = (const uint8_t *)greeting;
	*len = sizeof greeting - 1;
	return JME_OK;
}

void testhost_prepare(JmeVM *vm, void *user) {
	(void)user;
	Host *h = calloc(1, sizeof(Host));
	if (h == NULL) return;
	h->vm = vm;
	h->s.next_ms = EVERY_MS;
	h->next = hosts;
	hosts = h;
	jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
	jme_register_state(vm, "testhost", save, load, h);
	jme_set_resource_source(vm, resource, NULL);
	jme_set_property(vm, "test.host", "yes");
}

void testhost_step(JmeVM *vm, void *user) {
	(void)user;
	Host *h = host_of(vm);
	if (h == NULL) return;
	State *s = &h->s;
	int64_t now = jme_time_ms(vm);
	int posted = 0;
	while (now >= s->next_ms && s->count < QUEUE) {
		s->queue[(s->head + s->count) % QUEUE] = 100 + s->posted++;
		s->count++;
		s->next_ms += EVERY_MS;
		posted = 1;
	}
	if (posted) jme_signal(vm, EVENTS);
}

void testhost_free_all(void) {
	while (hosts != NULL) {
		Host *next = hosts->next;
		free(hosts);
		hosts = next;
	}
}
