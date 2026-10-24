/* A small host for testing the host API (jmevm --test-host). */
#ifndef JMEVM_TESTHOST_H
#define JMEVM_TESTHOST_H

#include "jmevm/jmevm.h"

/* Registers the test natives, state, resource and property on vm; user is
 * unused. Each VM gets its own host state. */
void testhost_prepare(JmeVM *vm, void *user);

/* Called before each step of the run: posts an input event every 7 emulated
 * milliseconds (part of the host state, so snapshots keep it). */
void testhost_step(JmeVM *vm, void *user);

/* Frees the host states made by testhost_prepare. */
void testhost_free_all(void);

#endif
