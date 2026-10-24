#include <stdlib.h>
#include <string.h>

#include "unit.h"
#include "vm.h"

static int round_trip(const u8 *src, size_t n, size_t *compressed) {
	u8 *c = malloc(lz_bound(n));
	u8 *d = malloc(n + 1);
	size_t cn = lz_compress(src, n, c);
	int ok = cn <= lz_bound(n) && lz_decompress(c, cn, d, n) && (n == 0 || memcmp(src, d, n) == 0);
	if (compressed != NULL) *compressed = cn;
	free(c);
	free(d);
	return ok;
}

void test_lz_round_trips(void) {
	enum { N = 300000 };
	u8 *buf = malloc(N);
	size_t cn;

	CHECK(round_trip(buf, 0, NULL));
	memset(buf, 0, N);
	CHECK(round_trip(buf, N, &cn));
	CHECK(cn < N / 100); /* zeros: almost nothing */
	u32 x = 12345;
	for (int i = 0; i < N; i++) {
		x = x * 1103515245u + 12345u;
		buf[i] = (u8)(x >> 24);
	}
	CHECK(round_trip(buf, N, &cn)); /* random: must not grow beyond the bound */
	for (int i = 0; i < N; i++) buf[i] = (u8)("abcabcabd"[i % 9] + (i / 5000));
	CHECK(round_trip(buf, N, &cn));
	CHECK(cn < N / 10);
	for (size_t n = 0; n < 40; n++) CHECK(round_trip(buf, n, NULL)); /* tiny inputs */
	/* a mostly-zero delta with a few changes, like a rewind delta */
	memset(buf, 0, N);
	for (int i = 0; i < N; i += 997) buf[i] = (u8)i;
	CHECK(round_trip(buf, N, &cn));
	CHECK(cn < N / 50);
	free(buf);
}

void test_lz_rejects_malformed(void) {
	enum { N = 5000 };
	u8 src[N], out[N];
	for (int i = 0; i < N; i++) src[i] = (u8)(i % 7 == 0 ? i : 0);
	u8 *c = malloc(lz_bound(N));
	size_t cn = lz_compress(src, N, c);
	/* truncated streams and wrong output sizes fail */
	for (size_t n = 0; n < cn; n++) CHECK(!lz_decompress(c, n, out, N));
	CHECK(!lz_decompress(c, cn, out, N - 1));
	/* changed bytes either fail or decode to N bytes, never overrun */
	for (size_t i = 0; i < cn; i++) {
		u8 old = c[i];
		c[i] = (u8)(old ^ 0x5A);
		(void)lz_decompress(c, cn, out, N);
		c[i] = 0xFF;
		(void)lz_decompress(c, cn, out, N);
		c[i] = old;
	}
	/* an offset reaching before the start of the output */
	const u8 bad[] = {0x10, 'a', 0x02, 0x00};
	CHECK(!lz_decompress(bad, sizeof bad, out, 5));
	free(c);
}
