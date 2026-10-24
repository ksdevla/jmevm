/*
 * A small byte-oriented LZ77 compressor for snapshots and rewind deltas,
 * written for jmevm. The stream is a series of sequences:
 *
 *   token          high 4 bits: literal count, low 4 bits: match length - 4
 *                  (15 in either means more length bytes follow: each adds
 *                  its value, and a byte below 255 ends them)
 *   literal bytes
 *   offset         u16, little-endian, 1..65535 back from the output end
 *   match length bytes (if the low nibble was 15)
 *
 * The last sequence has literals only and ends the stream. Matches may
 * overlap their own output, so a run of zeros (most of a rewind delta)
 * becomes a single sequence with offset 1.
 *
 * Compression is deterministic. The decompressor checks every length and
 * offset against its input and output, so malformed data fails cleanly.
 */
#include <stdlib.h>

#include "vm.h"

#define MIN_MATCH 4
#define MAX_OFFSET 65535
#define HASH_BITS 14

size_t lz_bound(size_t n) {
	return n + n / 255 + 16;
}

static u32 hash4(const u8 *p) {
	u32 v = (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
	return (v * 2654435761u) >> (32 - HASH_BITS);
}

static size_t put_length(u8 *dst, size_t op, size_t v) {
	while (v >= 255) {
		dst[op++] = 255;
		v -= 255;
	}
	dst[op++] = (u8)v;
	return op;
}

static size_t put_sequence(u8 *dst, size_t op, const u8 *lit, size_t nlit, size_t offset, size_t match) {
	size_t m = match ? match - MIN_MATCH : 0;
	dst[op++] = (u8)((nlit < 15 ? nlit : 15) << 4 | (m < 15 ? m : 15));
	if (nlit >= 15) op = put_length(dst, op, nlit - 15);
	memcpy(dst + op, lit, nlit);
	op += nlit;
	if (match == 0) return op;
	dst[op++] = (u8)offset;
	dst[op++] = (u8)(offset >> 8);
	if (m >= 15) op = put_length(dst, op, m - 15);
	return op;
}

size_t lz_compress(const u8 *src, size_t n, u8 *dst) {
	u32 *table = calloc((size_t)1 << HASH_BITS, sizeof(u32));
	size_t ip = 0, anchor = 0, op = 0;
	if (table != NULL) {
		u32 misses = 0;
		while (ip + MIN_MATCH <= n) {
			u32 h = hash4(src + ip);
			size_t cand = table[h]; /* position + 1, 0 = none */
			table[h] = (u32)(ip + 1);
			if (cand != 0 && ip - (cand - 1) <= MAX_OFFSET && memcmp(src + cand - 1, src + ip, MIN_MATCH) == 0) {
				size_t from = cand - 1, len = MIN_MATCH;
				while (ip + len < n && src[from + len] == src[ip + len]) len++;
				op = put_sequence(dst, op, src + anchor, ip - anchor, ip - from, len);
				ip += len;
				anchor = ip;
				misses = 0;
			} else {
				/* skip faster through data that does not compress */
				ip += 1 + (misses++ >> 6);
			}
		}
		free(table);
	}
	return put_sequence(dst, op, src + anchor, n - anchor, 0, 0);
}

/* Reads a length continuation into *v; 0 if the input ends or it exceeds max. */
static int get_length(const u8 *src, size_t n, size_t *ip, size_t *v, size_t max) {
	for (;;) {
		if (*ip >= n) return 0;
		u8 b = src[(*ip)++];
		*v += b;
		if (*v > max) return 0;
		if (b != 255) return 1;
	}
}

int lz_decompress(const u8 *src, size_t n, u8 *dst, size_t out_len) {
	size_t ip = 0, op = 0;
	for (;;) {
		/* the stream must end with a literals-only sequence */
		if (ip >= n) return 0;
		u8 token = src[ip++];
		size_t lit = token >> 4;
		if (lit == 15 && !get_length(src, n, &ip, &lit, out_len)) return 0;
		if (lit > n - ip || lit > out_len - op) return 0;
		memcpy(dst + op, src + ip, lit);
		ip += lit;
		op += lit;
		if (ip == n) return op == out_len && (token & 15) == 0;
		if (n - ip < 2) return 0;
		size_t offset = (size_t)src[ip] | (size_t)src[ip + 1] << 8;
		ip += 2;
		if (offset == 0 || offset > op) return 0;
		size_t match = token & 15;
		if (match == 15 && !get_length(src, n, &ip, &match, out_len)) return 0;
		match += MIN_MATCH;
		if (match > out_len - op) return 0;
		for (size_t i = 0; i < match; i++, op++) dst[op] = dst[op - offset];
	}
}
