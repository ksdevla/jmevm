/*
 * Object monitors. An object gets a monitor id the first time it is locked
 * and keeps it until it dies; the collector then returns the id to a free
 * list. Ids are reused in a fixed order, so they stay deterministic.
 *
 * Each monitor has an entry queue (threads blocked in monitorenter, a
 * synchronized call, or getting the monitor back after wait) and a wait set
 * (threads in wait()), both FIFO. Releasing a monitor hands it directly to
 * the first thread in the entry queue: that thread becomes the owner with
 * its saved count and is marked granted, and when it runs its blocked
 * instruction again it finds the monitor already its own. Hand-off in queue
 * order keeps the outcome independent of anything but the program.
 */
#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

static u32 monitor_new(VM *vm) {
	u32 id;
	if (vm->monitor_free != 0) {
		id = vm->monitor_free;
		vm->monitor_free = vm->monitors[id].owner;
	} else {
		if (vm->monitor_count == vm->monitor_cap) {
			u32 cap = vm->monitor_cap ? vm->monitor_cap * 2 : 256;
			Monitor *a = realloc(vm->monitors, cap * sizeof(Monitor));
			if (a == NULL) {
				fputs("jmevm: out of memory for monitors\n", stderr);
				abort();
			}
			vm->monitors = a;
			vm->monitor_cap = cap;
			if (vm->monitor_count == 0) vm->monitor_count = 1; /* id 0 means none */
		}
		id = vm->monitor_count++;
	}
	memset(&vm->monitors[id], 0, sizeof(Monitor));
	return id;
}

static Monitor *monitor_of(VM *vm, Ref obj) {
	if (OBJ(vm, obj)->monitor == 0) {
		u32 id = monitor_new(vm);
		OBJ(vm, obj)->monitor = id;
		CARD_MARK(vm, obj);
	}
	return &vm->monitors[OBJ(vm, obj)->monitor];
}

/* ---- queues ---- */

static void queue_push(VM *vm, u32 *head, u32 *tail, Thread *t) {
	t->queue_next = 0;
	if (*tail != 0) thread_by_id(vm, *tail)->queue_next = t->id;
	else *head = t->id;
	*tail = t->id;
}

static Thread *queue_pop(VM *vm, u32 *head, u32 *tail) {
	if (*head == 0) return NULL;
	Thread *t = thread_by_id(vm, *head);
	*head = t->queue_next;
	if (*head == 0) *tail = 0;
	t->queue_next = 0;
	return t;
}

static int queue_remove(VM *vm, u32 *head, u32 *tail, Thread *t) {
	u32 prev = 0;
	for (u32 id = *head; id != 0; id = thread_by_id(vm, id)->queue_next) {
		if (id == t->id) {
			if (prev == 0) *head = t->queue_next;
			else thread_by_id(vm, prev)->queue_next = t->queue_next;
			if (*tail == t->id) *tail = prev;
			t->queue_next = 0;
			return 1;
		}
		prev = id;
	}
	return 0;
}

/* ---- ownership ---- */

/* The monitor is free: the first thread in the entry queue gets it. */
static void hand_over(VM *vm, Monitor *m) {
	Thread *w = queue_pop(vm, &m->entry_head, &m->entry_tail);
	if (w == NULL) {
		m->owner = 0;
		m->count = 0;
		return;
	}
	m->owner = w->id;
	m->count = w->enter_count;
	w->granted = 1;
	w->state = TS_RUNNABLE;
}

int monitor_enter(VM *vm, Thread *t, Ref obj) {
	Monitor *m = monitor_of(vm, obj);
	if (m->owner == t->id && t->granted) {
		/* handed over while t was blocked */
		t->granted = 0;
		t->blocked_on = 0;
		return 1;
	}
	if (m->owner == 0) {
		m->owner = t->id;
		m->count = 1;
		return 1;
	}
	if (m->owner == t->id) {
		m->count++;
		return 1;
	}
	t->enter_count = 1;
	t->blocked_on = obj;
	t->state = TS_BLOCKED;
	queue_push(vm, &m->entry_head, &m->entry_tail, t);
	return 0;
}

int monitor_owned(VM *vm, Thread *t, Ref obj) {
	u32 id = OBJ(vm, obj)->monitor;
	return id != 0 && vm->monitors[id].owner == t->id && !t->granted;
}

int monitor_exit(VM *vm, Thread *t, Ref obj) {
	if (!monitor_owned(vm, t, obj)) return 0;
	Monitor *m = &vm->monitors[OBJ(vm, obj)->monitor];
	if (--m->count == 0) hand_over(vm, m);
	return 1;
}

/* ---- wait and notify ---- */

void monitor_wait(VM *vm, Thread *t, Ref obj, i64 ms) {
	Monitor *m = &vm->monitors[OBJ(vm, obj)->monitor];
	t->enter_count = m->count;
	hand_over(vm, m);
	queue_push(vm, &m->wait_head, &m->wait_tail, t);
	t->blocked_on = obj;
	t->state = TS_WAITING;
	t->timed = ms > 0;
	if (ms > 0) t->wake_tick = vm->ticks + (u64)ms * vm->options.ticks_per_ms;
	t->block_phase = 1;
}

int monitor_reacquired(VM *vm, Thread *t) {
	(void)vm;
	if (!t->granted) return 0;
	t->granted = 0;
	t->blocked_on = 0;
	return 1;
}

/* w left the wait set: it needs the monitor back before wait() returns. */
static void reacquire(VM *vm, Monitor *m, Thread *w) {
	w->block_phase = 2;
	w->timed = 0;
	if (m->owner == 0) {
		m->owner = w->id;
		m->count = w->enter_count;
		w->granted = 1;
		w->state = TS_RUNNABLE;
	} else {
		w->state = TS_BLOCKED;
		queue_push(vm, &m->entry_head, &m->entry_tail, w);
	}
}

void monitor_notify(VM *vm, Ref obj, int all) {
	Monitor *m = &vm->monitors[OBJ(vm, obj)->monitor];
	do {
		Thread *w = queue_pop(vm, &m->wait_head, &m->wait_tail);
		if (w == NULL) return;
		reacquire(vm, m, w);
	} while (all);
}

void monitor_wake_waiter(VM *vm, Thread *w) {
	Monitor *m = &vm->monitors[OBJ(vm, w->blocked_on)->monitor];
	if (queue_remove(vm, &m->wait_head, &m->wait_tail, w)) reacquire(vm, m, w);
}

void monitor_release_dead(VM *vm, u32 id) {
	vm->monitors[id].owner = vm->monitor_free;
	vm->monitors[id].count = 0;
	vm->monitor_free = id;
}

void monitors_free(VM *vm) {
	free(vm->monitors);
	vm->monitors = NULL;
	vm->monitor_count = vm->monitor_cap = vm->monitor_free = 0;
}
