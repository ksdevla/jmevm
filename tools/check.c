#include "check.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POINTS 48
#define EXIT_CHECK 4

typedef struct Capture {
	char *buf;
	size_t len, cap;
	int echo;
} Capture;

typedef struct Bytes {
	unsigned char *data;
	size_t len;
} Bytes;

typedef struct Point {
	Bytes snap;
	size_t out_pos;   /* reference output printed before the snapshot */
} Point;

static void capture(void *user, int stream, const char *data, size_t len) {
	Capture *c = user;
	if (c->echo) {
		FILE *f = stream == 2 ? stderr : stdout;
		fwrite(data, 1, len, f);
		fflush(f);
	}
	if (c->len + len > c->cap) {
		size_t cap = c->cap ? c->cap * 2 : 4096;
		while (cap < c->len + len) cap *= 2;
		char *b = realloc(c->buf, cap);
		if (b == NULL) return;
		c->buf = b;
		c->cap = cap;
	}
	memcpy(c->buf + c->len, data, len);
	c->len += len;
}

static JmeStatus collect(void *user, const void *data, size_t len) {
	Bytes *b = user;
	b->data = malloc(len);
	if (b->data == NULL) return JME_E_NOMEM;
	memcpy(b->data, data, len);
	b->len = len;
	return JME_OK;
}

static int fail(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fputs("jmevm: check failed: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	return 0;
}

static JmeVM *new_vm(const CheckSetup *s, Capture *out) {
	JmeVM *vm;
	if (jme_create(s->options, &vm) != JME_OK) return NULL;
	jme_set_class_source(vm, s->source, s->source_user);
	jme_set_console(vm, capture, out);
	if (s->prepare != NULL) s->prepare(vm, s->host_user);
	return vm;
}

/* One step of a run: the host's hook, then `step` more ticks. */
static void step_once(const CheckSetup *s, JmeVM *vm) {
	if (s->on_step != NULL) s->on_step(vm, s->host_user);
	jme_run_until(vm, jme_ticks(vm) + s->step);
}

/* Runs to the end in steps; 0 if it does not finish in time. */
static int run_to_end(const CheckSetup *s, JmeVM *vm) {
	for (int steps = 0; !jme_finished(vm, NULL); steps++) {
		if (steps > 1000000) return 0;
		step_once(s, vm);
	}
	return 1;
}

static int same_bytes(const Bytes *a, const Bytes *b) {
	return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}

/* Loads p into a fresh VM and runs it to the end like the reference run. */
static int verify_point(const CheckSetup *s, int i, const Point *p, const Capture *ref, int ref_exit, uint64_t ref_ticks) {
	Capture out = {0};
	JmeVM *vm = new_vm(s, &out);
	if (vm == NULL) return fail("no memory");
	int ok = 1;
	JmeStatus st = jme_snapshot_load(vm, p->snap.data, p->snap.len);
	if (st != JME_OK) ok = fail("snapshot %d did not load: %s", i, jme_status_name(st));
	Bytes again = {0};
	if (ok && jme_snapshot_save(vm, collect, &again) != JME_OK) ok = fail("snapshot %d: saving again failed", i);
	if (ok && !same_bytes(&again, &p->snap)) ok = fail("snapshot %d: saving it again gave other bytes", i);
	free(again.data);
	int exit_code = -1;
	if (ok && (!run_to_end(s, vm) || !jme_finished(vm, &exit_code)))
		ok = fail("snapshot %d: the program did not finish", i);
	if (ok && exit_code != ref_exit) ok = fail("snapshot %d: exit code %d, the full run gave %d", i, exit_code, ref_exit);
	if (ok && jme_ticks(vm) != ref_ticks)
		ok = fail("snapshot %d: ended at tick %llu, the full run at %llu", i, (unsigned long long)jme_ticks(vm),
				(unsigned long long)ref_ticks);
	size_t rest = ref->len - p->out_pos;
	if (ok && (out.len != rest || memcmp(out.buf, ref->buf + p->out_pos, rest) != 0))
		ok = fail("snapshot %d: the output after it differs from the full run", i);
	jme_destroy(vm);
	free(out.buf);
	return ok;
}

static uint64_t fnv(const unsigned char *p, size_t n) {
	uint64_t h = 14695981039346656037ull;
	for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
	return h;
}

/* Damaged snapshots must be refused, or at least loaded without harm. */
static int check_damage(const CheckSetup *s, const Bytes *snap) {
	Capture out = {0};
	JmeVM *vm = new_vm(s, &out);
	if (vm == NULL) return fail("no memory");
	int ok = 1;
	size_t n = snap->len;
	size_t cuts[] = {0, 8, 40, n / 4, n / 2, n - 9, n - 1};
	for (size_t i = 0; i < sizeof cuts / sizeof cuts[0] && ok; i++)
		if (jme_snapshot_load(vm, snap->data, cuts[i]) == JME_OK) ok = fail("a snapshot cut to %zu bytes loaded", cuts[i]);
	unsigned char *copy = malloc(n);
	for (int i = 0; i < 32 && ok && copy != NULL; i++) {
		size_t at = (n - 1) * (size_t)i / 31;
		memcpy(copy, snap->data, n);
		copy[at] ^= 0x10;
		if (jme_snapshot_load(vm, copy, n) == JME_OK) ok = fail("a snapshot with byte %zu changed loaded", at);
	}
	/* With a correct checksum, the structural checks are what stands
	 * between damaged data and the interpreter: loading must not crash.
	 * JMEVM_FUZZ=<count> tries that many random changes instead. */
	const char *fuzz = getenv("JMEVM_FUZZ");
	long tries = fuzz != NULL ? strtol(fuzz, NULL, 10) : 256;
	uint32_t seed = 0x9E3779B9u;
	for (long i = 0; i < tries && ok && copy != NULL; i++) {
		seed = seed * 1664525u + 1013904223u;
		size_t at = fuzz != NULL ? 32 + seed % (n - 40) : 32 + (n - 41) * (size_t)i / 255;
		memcpy(copy, snap->data, n);
		copy[at] ^= (unsigned char)(fuzz != NULL ? 1 + (seed >> 24) % 255 : 0x41 + i);
		if (fuzz != NULL && (seed & 3) == 0) copy[32 + (seed >> 8) % (n - 40)] ^= 0x80;
		if (fuzz != NULL && getenv("JMEVM_FUZZ_LOG") != NULL)
			fprintf(stderr, "fuzz %ld: byte %zu of %zu%s\n", i, at, n, (seed & 3) == 0 ? " and another" : "");
		uint64_t h = fnv(copy, n - 8);
		for (int b = 0; b < 8; b++) copy[n - 8 + b] = (unsigned char)(h >> (8 * b));
		(void)jme_snapshot_load(vm, copy, n);
	}
	free(copy);
	jme_destroy(vm);
	free(out.buf);
	return ok;
}

/* Loading into a running VM replaces its state completely. */
static int check_reload(const CheckSetup *s, const Bytes *first, const Bytes *last) {
	Capture out = {0};
	JmeVM *vm = new_vm(s, &out);
	if (vm == NULL) return fail("no memory");
	int ok = jme_snapshot_load(vm, last->data, last->len) == JME_OK;
	if (ok) step_once(s, vm);
	ok = ok && jme_snapshot_load(vm, first->data, first->len) == JME_OK;
	Bytes again = {0};
	if (!ok) fail("loading into a running VM failed");
	else if (jme_snapshot_save(vm, collect, &again) != JME_OK || !same_bytes(&again, first))
		ok = fail("loading into a running VM did not give back the snapshot");
	free(again.data);
	jme_destroy(vm);
	free(out.buf);
	return ok;
}

/* Every entry of rw (the newest `count` points) loads back exactly. */
static int check_ring(const CheckSetup *s, JmeRewind *rw, const Point *points, int npoints, const char *name) {
	Capture out = {0};
	JmeVM *vm = new_vm(s, &out);
	if (vm == NULL) return fail("no memory");
	int ok = 1;
	uint32_t count = jme_rewind_count(rw);
	if (count == 0 || count > (uint32_t)npoints) ok = fail("%s ring holds %u entries for %d snapshots", name, count, npoints);
	for (uint32_t back = 0; back < count && ok; back++) {
		const Bytes *want = &points[npoints - 1 - (int)back].snap;
		Bytes got = {0};
		JmeStatus st = jme_rewind_load(rw, vm, back);
		if (st != JME_OK) ok = fail("%s ring entry %u did not load: %s", name, back, jme_status_name(st));
		else if (jme_snapshot_save(vm, collect, &got) != JME_OK || !same_bytes(&got, want))
			ok = fail("%s ring entry %u is not the snapshot that was pushed", name, back);
		free(got.data);
	}
	jme_destroy(vm);
	free(out.buf);
	return ok;
}

/*
 * Runs the program again in a new VM with a history of `budget` bytes,
 * recording at every snapshot point and once at the end; then steps back.
 * Each step must give exactly the snapshot of its point (the oldest step:
 * the state the history started from). With resume, the program continues
 * from the middle point to its end and must finish like the full run, and
 * the steps back go on from there. *bytes gets the bytes the history held.
 */
static int check_history(const CheckSetup *s, const Point *points, int npoints, size_t budget, int resume,
		const Capture *ref, int ref_exit, uint64_t ref_ticks, const char *name, size_t *bytes) {
	Capture out = {0};
	JmeVM *vm = new_vm(s, &out);
	if (vm == NULL) return fail("no memory");
	JmeHistory *h = NULL;
	Bytes base = {0};
	int ok = jme_start(vm, s->main_class, s->argc, s->argv) == JME_OK;
	if (!ok) fail("the program did not start");
	if (ok && jme_snapshot_save(vm, collect, &base) != JME_OK) ok = fail("saving the first state failed");
	if (ok && jme_history_create(vm, budget, &h) != JME_OK) ok = fail("%s history: no history", name);
	int k = 0;
	for (int steps = 0; ok && !jme_finished(vm, NULL); steps++) {
		if (steps > 1000000) {
			ok = fail("the program did not finish");
			break;
		}
		step_once(s, vm);
		if (jme_finished(vm, NULL) || k == npoints) continue;
		if (jme_history_record(h) != JME_OK) ok = fail("%s history: recording failed", name);
		k++;
	}
	if (ok && jme_history_record(h) != JME_OK) ok = fail("%s history: recording the end failed", name);
	if (ok) *bytes = jme_history_size(h);
	uint32_t count = ok ? jme_history_count(h) : 0;
	/* The full history holds every record; the small one drops the oldest */
	if (ok && (resume ? count != (uint32_t)npoints + 1 : count == 0 || count > (uint32_t)npoints))
		ok = fail("%s history holds %u records for %d snapshots", name, count, npoints);

	/* Step i lands on point npoints - 1 - i; the step after the last point
	 * lands on the base. */
	for (uint32_t i = 0; i < count && ok; i++) {
		JmeStatus st = jme_history_back(h);
		if (st != JME_OK) {
			ok = fail("%s history: step back %u failed: %s", name, i, jme_status_name(st));
			break;
		}
		int at = npoints - 1 - (int)i;
		const Bytes *want = at >= 0 ? &points[at].snap : &base;
		Bytes got = {0};
		if (jme_snapshot_save(vm, collect, &got) != JME_OK || !same_bytes(&got, want))
			ok = fail("%s history: step back %u is not snapshot %d", name, i, at);
		free(got.data);
		if (ok && resume && at == npoints / 2) {
			size_t from = out.len;
			int exit_code = -1;
			if (!run_to_end(s, vm) || !jme_finished(vm, &exit_code))
				ok = fail("%s history: the program did not finish from snapshot %d", name, at);
			size_t rest = ref->len - points[at].out_pos;
			if (ok && (exit_code != ref_exit || jme_ticks(vm) != ref_ticks || out.len - from != rest
					|| memcmp(out.buf + from, ref->buf + points[at].out_pos, rest) != 0))
				ok = fail("%s history: the program went on from snapshot %d differently", name, at);
		}
	}
	jme_history_destroy(h);
	jme_destroy(vm);
	free(base.data);
	free(out.buf);
	return ok;
}

int check_snapshots(const CheckSetup *s) {
	Capture ref = {0};
	ref.echo = 1;
	Point points[MAX_POINTS];
	int npoints = 0, ok = 1;
	JmeRewind *rw = NULL, *small = NULL;
	size_t raw_total = 0;

	JmeVM *vm = new_vm(s, &ref);
	if (vm == NULL) return fail("no memory"), EXIT_CHECK;
	if (jme_start(vm, s->main_class, s->argc, s->argv) != JME_OK) {
		jme_destroy(vm);
		return fail("the program did not start"), EXIT_CHECK;
	}
	if (s->rewind == 1) jme_rewind_create((size_t)1 << 30, 8, &rw);
	for (int steps = 0; !jme_finished(vm, NULL); steps++) {
		if (steps > 1000000) {
			ok = fail("the program did not finish");
			break;
		}
		step_once(s, vm);
		if (jme_finished(vm, NULL) || npoints == MAX_POINTS) continue;
		Point *p = &points[npoints];
		memset(p, 0, sizeof *p);
		if (jme_snapshot_save(vm, collect, &p->snap) != JME_OK) {
			ok = fail("saving a snapshot failed");
			break;
		}
		p->out_pos = ref.len;
		raw_total += p->snap.len;
		npoints++;
		if (rw != NULL) {
			if (jme_rewind_push(rw, vm) != JME_OK) ok = fail("pushing to the rewind ring failed");
			/* The small ring holds about two compressed keyframes: it must
			 * drop old entries and still give back the ones it keeps. */
			if (small == NULL) jme_rewind_create(2 * jme_rewind_size(rw) + 1, 4, &small);
			if (small == NULL || jme_rewind_push(small, vm) != JME_OK) ok = fail("pushing to the small ring failed");
		}
	}
	int ref_exit = 0;
	jme_finished(vm, &ref_exit);
	uint64_t ref_ticks = jme_ticks(vm);
	jme_destroy(vm);
	if (ok && npoints == 0) ok = fail("the program ended before the first snapshot (step %llu)", (unsigned long long)s->step);

	if (ok && !s->rewind) {
		for (int i = 0; i < npoints && ok; i++) ok = verify_point(s, i, &points[i], &ref, ref_exit, ref_ticks);
		if (ok) ok = check_damage(s, &points[0].snap);
		if (ok) ok = check_reload(s, &points[0].snap, &points[npoints - 1].snap);
	}
	if (ok && s->rewind == 2) {
		size_t full = 0, small_bytes = 0;
		ok = check_history(s, points, npoints, (size_t)1 << 30, 1, &ref, ref_exit, ref_ticks, "full", &full);
		/* One byte less must drop the oldest records and keep the newest */
		if (ok) ok = check_history(s, points, npoints, full - 1, 0, &ref, ref_exit, ref_ticks, "small", &small_bytes);
		if (ok && s->stats)
			fprintf(stderr, "history: %d snapshots, %zu bytes raw, %zu bytes of records (%.1f%%)\n", npoints,
					raw_total, full, 100.0 * (double)full / (double)raw_total);
	}
	if (ok && s->rewind == 1) {
		ok = check_ring(s, rw, points, npoints, "full");
		if (ok && npoints >= 4 && jme_rewind_count(small) >= (uint32_t)npoints)
			ok = fail("the small ring never dropped an entry");
		if (ok) ok = check_ring(s, small, points, npoints, "small");
		if (ok && s->stats)
			fprintf(stderr, "rewind: %d snapshots, %zu bytes raw, %zu bytes in the ring (%.1f%%)\n", npoints,
					raw_total, jme_rewind_size(rw), 100.0 * (double)jme_rewind_size(rw) / (double)raw_total);
	}
	if (ok && s->stats && !s->rewind)
		fprintf(stderr, "snapshots: %d checked, %zu bytes on average\n", npoints, raw_total / (size_t)npoints);

	jme_rewind_destroy(rw);
	jme_rewind_destroy(small);
	for (int i = 0; i < npoints; i++) free(points[i].snap.data);
	free(ref.buf);
	return ok ? ref_exit : EXIT_CHECK;
}
