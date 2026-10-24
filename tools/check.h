/* Snapshot and rewind self-checks of the command-line runner. */
#ifndef JMEVM_CHECK_H
#define JMEVM_CHECK_H

#include <stdint.h>

#include "jmevm/jmevm.h"

typedef struct CheckSetup {
	const JmeOptions *options;
	JmeClassSource source;
	void *source_user;
	const char *main_class;
	int argc;
	const char *const *argv;
	uint64_t step;     /* ticks between snapshots */
	int rewind;        /* 1: check the rewind ring instead */
	int stats;         /* print sizes on stderr */
	/* Optional host: prepare registers natives and state on every new VM;
	 * step runs before every step of every run (reference and restored),
	 * like a front end feeding input between frames. */
	void (*prepare)(JmeVM *vm, void *user);
	void (*on_step)(JmeVM *vm, void *user);
	void *host_user;
} CheckSetup;

/*
 * Runs the program, printing its output, and saves a snapshot every `step`
 * ticks. Snapshot mode: every snapshot must load into a fresh VM, save back
 * to the same bytes, and run to the end with the same remaining output, exit
 * code and final tick; damaged snapshots must be refused. Rewind mode: every
 * entry of a rewind ring (and of a small one that drops old entries) must
 * load back to the exact snapshot. Returns the program's exit code, or 4
 * after printing what failed.
 */
int check_snapshots(const CheckSetup *setup);

#endif
