/*
 * inflate.c - DEFLATE decompression, after RFC 1951.
 *
 * Bits are read least significant first into a 32-bit buffer. Huffman codes
 * up to FAST_BITS long are decoded with one table lookup; longer ones by
 * walking the canonical code, length by length. Reading past the end of the
 * input pads with zeros, and consuming any of that padding is an error.
 */
#include "inflate.h"

#include <stdbool.h>
#include <string.h>

#define MAX_BITS  15
#define FAST_BITS 9
#define FAST_SIZE (1 << FAST_BITS)
#define MAX_LIT   288   /* literal/length symbols; 286 and 287 are never used */
#define MAX_DIST  32    /* distance symbols; 30 and 31 are never used */

typedef struct Huffman {
    /* (code length << 9) | symbol, indexed by the next FAST_BITS input bits;
     * 0 when the code is longer than FAST_BITS (or unused). */
    uint16_t fast[FAST_SIZE];
    uint16_t count[MAX_BITS + 1];   /* number of codes of each length */
    uint16_t sorted[MAX_LIT];       /* symbols by code length, then value */
} Huffman;

typedef struct Inflater {
    const uint8_t *in;
    uint32_t       in_len, in_pos;  /* in_pos may run past in_len (padding) */
    uint32_t       bits;
    int            nbits;
    uint8_t       *out;
    uint32_t       out_len, out_pos;
    bool           bad;
} Inflater;

static const uint16_t length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
/* The order in which the code length code's lengths are stored. */
static const uint8_t clen_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* ---- bits ---- */

static void fill(Inflater *s, int n)
{
    while (s->nbits < n) {
        uint32_t b = s->in_pos < s->in_len ? s->in[s->in_pos] : 0;
        s->in_pos++;
        s->bits |= b << s->nbits;
        s->nbits += 8;
    }
}

static void drop(Inflater *s, int n)
{
    s->bits >>= n;
    s->nbits -= n;
    /* Consumed more bits than the input had? */
    if (s->in_pos > s->in_len && (uint64_t)(s->in_pos - s->in_len) * 8 > (uint64_t)s->nbits)
        s->bad = true;
}

static uint32_t take(Inflater *s, int n)
{
    if (n == 0)
        return 0;
    fill(s, n);
    uint32_t v = s->bits & ((1u << n) - 1);
    drop(s, n);
    return v;
}

/* ---- Huffman codes ---- */

static uint32_t reverse(uint32_t code, int len)
{
    uint32_t r = 0;
    for (int i = 0; i < len; i++) {
        r = (r << 1) | (code & 1);
        code >>= 1;
    }
    return r;
}

/* Builds the code for n symbols with the given code lengths (0 = unused).
 * Fails if the lengths ask for more codes than exist. A code with unused
 * entries is allowed; decoding one of them is then an error. */
static bool build(Huffman *h, const uint8_t *lengths, int n)
{
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++)
        h->count[lengths[i]]++;
    h->count[0] = 0;

    int left = 1;
    for (int len = 1; len <= MAX_BITS; len++) {
        left = (left << 1) - h->count[len];
        if (left < 0)
            return false;
    }

    uint16_t next[MAX_BITS + 2];
    next[1] = 0;
    for (int len = 1; len <= MAX_BITS; len++)
        next[len + 1] = (uint16_t)(next[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (lengths[i])
            h->sorted[next[lengths[i]]++] = (uint16_t)i;

    /* Canonical codes: consecutive within a length, in symbol order, and
     * doubled when moving on to the next length. In the stream they arrive
     * most significant bit first, hence the reversal for the table. */
    memset(h->fast, 0, sizeof h->fast);
    uint32_t code = 0;
    int      k = 0;
    for (int len = 1; len <= FAST_BITS; len++) {
        for (int j = 0; j < h->count[len]; j++, k++, code++) {
            uint16_t entry = (uint16_t)((len << 9) | h->sorted[k]);
            for (uint32_t i = reverse(code, len); i < FAST_SIZE; i += 1u << len)
                h->fast[i] = entry;
        }
        code <<= 1;
    }
    return true;
}

/* The next symbol, or -1. */
static int decode(Inflater *s, const Huffman *h)
{
    fill(s, MAX_BITS);
    uint16_t e = h->fast[s->bits & (FAST_SIZE - 1)];
    if (e) {
        drop(s, e >> 9);
        return e & 511;
    }
    /* Longer than FAST_BITS: walk the code one bit at a time. code is the
     * value read so far, first the first code of this length, and index
     * where the symbols of this length start in sorted. */
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAX_BITS; len++) {
        code |= (int)((s->bits >> (len - 1)) & 1);
        int count = h->count[len];
        if (code - first < count) {
            drop(s, len);
            return h->sorted[index + code - first];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    s->bad = true;
    return -1;
}

/* ---- blocks ---- */

static bool stored(Inflater *s)
{
    drop(s, s->nbits & 7);   /* to the next byte boundary */
    uint32_t len = take(s, 16);
    uint32_t nlen = take(s, 16);
    if (s->bad || (len ^ 0xFFFF) != nlen)
        return false;
    /* Give back the whole bytes still in the buffer, then copy directly. */
    s->in_pos -= (uint32_t)(s->nbits / 8);
    s->bits = 0;
    s->nbits = 0;
    if (s->in_pos > s->in_len || len > s->in_len - s->in_pos || len > s->out_len - s->out_pos)
        return false;
    memcpy(s->out + s->out_pos, s->in + s->in_pos, len);
    s->in_pos += len;
    s->out_pos += len;
    return true;
}

static bool codes(Inflater *s, const Huffman *lit, const Huffman *dist)
{
    for (;;) {
        int sym = decode(s, lit);
        if (sym < 0 || s->bad)
            return false;
        if (sym < 256) {
            if (s->out_pos == s->out_len)
                return false;
            s->out[s->out_pos++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256)
            return true;
        sym -= 257;
        if (sym >= 29)
            return false;
        uint32_t len = length_base[sym] + take(s, length_extra[sym]);
        int      d = decode(s, dist);
        if (d < 0 || d >= 30)
            return false;
        uint32_t back = dist_base[d] + take(s, dist_extra[d]);
        if (s->bad || back > s->out_pos || len > s->out_len - s->out_pos)
            return false;
        /* Byte by byte: the source may overlap what is being written. */
        uint8_t       *to = s->out + s->out_pos;
        const uint8_t *from = to - back;
        for (uint32_t i = 0; i < len; i++)
            to[i] = from[i];
        s->out_pos += len;
    }
}

static bool fixed(Inflater *s)
{
    uint8_t lengths[MAX_LIT + 30];
    int     i = 0;
    for (; i < 144; i++) lengths[i] = 8;
    for (; i < 256; i++) lengths[i] = 9;
    for (; i < 280; i++) lengths[i] = 7;
    for (; i < 288; i++) lengths[i] = 8;
    for (; i < 288 + 30; i++) lengths[i] = 5;
    Huffman lit, dist;
    build(&lit, lengths, 288);
    build(&dist, lengths + 288, 30);
    return codes(s, &lit, &dist);
}

static bool dynamic(Inflater *s)
{
    uint32_t nlit = take(s, 5) + 257, ndist = take(s, 5) + 1, nclen = take(s, 4) + 4;
    if (s->bad || nlit > 286 || ndist > 30)
        return false;

    uint8_t clens[19] = {0};
    for (uint32_t i = 0; i < nclen; i++)
        clens[clen_order[i]] = (uint8_t)take(s, 3);
    Huffman h;
    if (s->bad || !build(&h, clens, 19))
        return false;

    /* The literal/length and distance code lengths, as one sequence. */
    uint8_t  lengths[MAX_LIT + MAX_DIST];
    uint32_t n = 0;
    while (n < nlit + ndist) {
        int sym = decode(s, &h);
        if (sym < 0 || s->bad)
            return false;
        if (sym < 16) {
            lengths[n++] = (uint8_t)sym;
            continue;
        }
        uint8_t  value = 0;
        uint32_t repeat;
        if (sym == 16) {   /* the previous length again */
            if (n == 0)
                return false;
            value = lengths[n - 1];
            repeat = 3 + take(s, 2);
        } else if (sym == 17) {
            repeat = 3 + take(s, 3);
        } else {
            repeat = 11 + take(s, 7);
        }
        if (s->bad || repeat > nlit + ndist - n)
            return false;
        memset(lengths + n, value, repeat);
        n += repeat;
    }
    if (lengths[256] == 0)   /* no end-of-block code */
        return false;

    Huffman lit, dist;
    if (!build(&lit, lengths, (int)nlit) || !build(&dist, lengths + nlit, (int)ndist))
        return false;
    return codes(s, &lit, &dist);
}

int32_t inflate_raw(uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len)
{
    if (dst_len > INT32_MAX)
        return -1;
    Inflater s;
    memset(&s, 0, sizeof s);
    s.in = src;
    s.in_len = src_len;
    s.out = dst;
    s.out_len = dst_len;
    bool last;
    do {
        last = take(&s, 1);
        uint32_t type = take(&s, 2);
        if (s.bad)
            return -1;
        bool ok;
        switch (type) {
        case 0:  ok = stored(&s); break;
        case 1:  ok = fixed(&s); break;
        case 2:  ok = dynamic(&s); break;
        default: ok = false; break;
        }
        if (!ok || s.bad)
            return -1;
    } while (!last);
    return (int32_t)s.out_pos;
}

int32_t inflate_zlib(uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len)
{
    /* CMF: method 8 (deflate). FLG: no preset dictionary. Together a
     * multiple of 31. */
    if (src_len < 2 || (src[0] & 0x0F) != 8 || (src[1] & 0x20) != 0 ||
        ((src[0] << 8) | src[1]) % 31 != 0)
        return -1;
    return inflate_raw(dst, dst_len, src + 2, src_len - 2);
}
