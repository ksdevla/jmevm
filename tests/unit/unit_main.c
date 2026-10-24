/*
 * Unit test runner. Runs every test, or only those whose name contains the
 * first argument. Exits 1 if any check failed.
 */
#include <stdio.h>
#include <string.h>

#include "unit.h"

#define TESTS(X) \
	X(api_create_destroy) \
	X(api_rejects_bad_options) \
	X(classfile_minimal) \
	X(classfile_truncated) \
	X(classfile_trailing_bytes) \
	X(classfile_versions) \
	X(classfile_bad_references) \
	X(classfile_method_bodies) \
	X(classfile_mutations) \
	X(fpconv_double_to_string) \
	X(fpconv_float_to_string) \
	X(fpconv_parse) \
	X(fdlibm_matches_strictmath) \
	X(lz_round_trips) \
	X(lz_rejects_malformed)

#define DECLARE(name) void test_##name(void);
TESTS(DECLARE)

static const struct {
	const char *name;
	void (*fn)(void);
} tests[] = {
#define ENTRY(name) {#name, test_##name},
	TESTS(ENTRY)
};

static int failures;

void unit_fail(const char *file, int line, const char *what) {
	failures++;
	printf("  %s:%d: CHECK(%s) failed\n", file, line, what);
}

void unit_fail_eq(const char *file, int line, const char *what, int64_t got, int64_t want) {
	failures++;
	printf("  %s:%d: %s is %lld, expected %lld\n", file, line, what, (long long)got, (long long)want);
}

int main(int argc, char **argv) {
	const char *filter = argc > 1 ? argv[1] : "";
	int run = 0, failed = 0;
	for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
		if (strstr(tests[i].name, filter) == NULL) continue;
		int before = failures;
		tests[i].fn();
		run++;
		if (failures != before) failed++;
		printf("%s %s\n", failures == before ? "ok  " : "FAIL", tests[i].name);
	}
	printf("%d tests, %d failed\n", run, failed);
	return failed != 0 || run == 0;
}
