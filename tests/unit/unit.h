/* Minimal unit test helpers: a failed CHECK records the failure and continues. */
#ifndef JMEVM_UNIT_H
#define JMEVM_UNIT_H

#include <stdint.h>

void unit_fail(const char *file, int line, const char *what);
void unit_fail_eq(const char *file, int line, const char *what, int64_t got, int64_t want);

#define CHECK(cond) do { if (!(cond)) unit_fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_EQ(got, want) do { \
		int64_t g_ = (int64_t)(got), w_ = (int64_t)(want); \
		if (g_ != w_) unit_fail_eq(__FILE__, __LINE__, #got, g_, w_); \
	} while (0)

#endif
