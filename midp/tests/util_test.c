/*
 * util_test.c - tests of the DEFLATE, ZIP and PNG decoders (src/util)
 * against the files in tests/util (see make_fixtures.py), real JARs, and
 * damaged copies of all of them.
 *
 *   util_test inflate DIR
 *   util_test png DIR
 *   util_test zip DIR JAR...
 *
 * Damaged input must never crash or read out of bounds (run under
 * AddressSanitizer for that); it may decode to anything or fail.
 */
#include "util/inflate.h"
#include "util/png.h"
#include "util/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                         \
    do {                                         \
        if (!(cond)) {                           \
            failures++;                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                 \
            printf("\n");                        \
        }                                        \
    } while (0)

static uint32_t rng_state = 0x1951u;

static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint8_t *load(const char *dir, const char *name, uint32_t *len)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("FAIL: cannot open %s\n", path);
        failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size + 1);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(data);
        return NULL;
    }
    fclose(f);
    *len = (uint32_t)size;
    return data;
}

/* Copies of data with a few random bytes changed. */
static uint8_t *mutate(const uint8_t *data, uint32_t len)
{
    uint8_t *copy = malloc(len ? len : 1);
    memcpy(copy, data, len);
    int changes = 1 + (int)(rng() % 4);
    for (int i = 0; i < changes && len; i++) {
        uint32_t at = rng() % len;
        copy[at] = (rng() & 1) ? (uint8_t)rng() : (uint8_t)(copy[at] ^ (1u << (rng() % 8)));
    }
    return copy;
}

/* ---- inflate ---- */

static const char *variants[] = {
    "stored", "fast", "default", "best", "fixed", "huffman", "rle", "smallwindow", NULL};

static void test_inflate(const char *dir)
{
    uint32_t input_len;
    uint8_t *input = load(dir, "input.bin", &input_len);
    if (!input)
        return;
    uint8_t *out = malloc(input_len + 64);

    for (int v = 0; variants[v]; v++) {
        char     name[64];
        uint32_t len;
        snprintf(name, sizeof name, "%s.deflate", variants[v]);
        uint8_t *raw = load(dir, name, &len);
        snprintf(name, sizeof name, "%s.zlib", variants[v]);
        uint32_t zlen;
        uint8_t *z = load(dir, name, &zlen);
        if (!raw || !z)
            continue;

        int32_t n = inflate_raw(out, input_len + 64, raw, len);
        CHECK(n == (int32_t)input_len && memcmp(out, input, input_len) == 0,
              "%s.deflate: %d bytes", variants[v], n);
        n = inflate_zlib(out, input_len, z, zlen);
        CHECK(n == (int32_t)input_len && memcmp(out, input, input_len) == 0,
              "%s.zlib: %d bytes", variants[v], n);
        CHECK(inflate_raw(out, input_len - 1, raw, len) == -1, "%s: output space too small",
              variants[v]);
        CHECK(inflate_zlib(out, input_len, raw, len) == -1, "%s: raw data as zlib", variants[v]);

        /* Every truncation fails (the last byte always holds part of the
         * end-of-block code or stored data). */
        for (uint32_t cut = 0; cut < len; cut++) {
            int32_t r = inflate_raw(out, input_len + 64, raw, cut);
            CHECK(r == -1, "%s cut to %u bytes: %d", variants[v], cut, r);
            if (r != -1)
                break;
        }
        for (int i = 0; i < 3000; i++) {
            uint8_t *bad = mutate(raw, len);
            int32_t  r = inflate_raw(out, input_len + 64, bad, len);
            CHECK(r <= (int32_t)input_len + 64, "%s: mutation gave %d", variants[v], r);
            free(bad);
        }
        free(raw);
        free(z);
    }

    uint32_t len;
    uint8_t *blocks = load(dir, "blocks.deflate", &len);
    if (blocks) {
        int32_t n = inflate_raw(out, input_len, blocks, len);
        CHECK(n == (int32_t)input_len && memcmp(out, input, input_len) == 0,
              "blocks.deflate: %d bytes", n);
        free(blocks);
    }
    uint8_t *empty = load(dir, "empty.deflate", &len);
    if (empty) {
        CHECK(inflate_raw(out, 0, empty, len) == 0, "empty.deflate");
        free(empty);
    }
    /* Invalid block type 3; a stored block whose length check fails. */
    static const uint8_t type3[] = {0x07};
    static const uint8_t bad_stored[] = {0x01, 0x05, 0x00, 0xFA, 0xFE, 'h', 'e', 'l', 'l', 'o'};
    static const uint8_t good_stored[] = {0x01, 0x05, 0x00, 0xFA, 0xFF, 'h', 'e', 'l', 'l', 'o'};
    CHECK(inflate_raw(out, 16, type3, sizeof type3) == -1, "block type 3");
    CHECK(inflate_raw(out, 16, bad_stored, sizeof bad_stored) == -1, "stored LEN/NLEN mismatch");
    CHECK(inflate_raw(out, 16, good_stored, sizeof good_stored) == 5 &&
              memcmp(out, "hello", 5) == 0, "stored block");
    free(out);
    free(input);
}

/* ---- PNG ---- */

typedef struct Canvas {
    int       w, h;
    uint32_t *px;
    int       rows;
} Canvas;

static void put_row(void *ctx, int y, int x0, int step, const uint32_t *argb, int n)
{
    Canvas *c = ctx;
    c->rows++;
    if (y < 0 || y >= c->h)
        return;
    for (int i = 0; i < n; i++) {
        int x = x0 + i * step;
        if (x >= 0 && x < c->w)
            c->px[y * c->w + x] = argb[i];
    }
}

static void test_png(const char *dir)
{
    uint32_t list_len;
    char    *list = (char *)load(dir, "png_list.txt", &list_len);
    if (!list)
        return;
    list[list_len] = 0;
    int   images = 0;
    char *line = strtok(list, "\n");
    for (; line; line = strtok(NULL, "\n")) {
        char name[64];
        int  w, h, alpha;
        if (sscanf(line, "%63s %d %d %d", name, &w, &h, &alpha) != 4)
            continue;
        char     file[80];
        uint32_t len, exp_len;
        snprintf(file, sizeof file, "%s.png", name);
        uint8_t *png = load(dir, file, &len);
        snprintf(file, sizeof file, "%s.argb", name);
        uint8_t *exp = load(dir, file, &exp_len);
        if (!png || !exp)
            continue;
        images++;

        PngInfo info;
        CHECK(png_is_png(png, len) && png_info(png, len, &info), "%s: info", name);
        CHECK(info.width == w && info.height == h && info.has_alpha == (alpha != 0),
              "%s: %dx%d alpha %d", name, info.width, info.height, info.has_alpha);

        Canvas c = {w, h, calloc((size_t)w * h, sizeof(uint32_t)), 0};
        CHECK(png_decode(png, len, put_row, &c), "%s: decode", name);
        CHECK(exp_len == (uint32_t)(w * h * 4), "%s: expected size", name);
        for (int i = 0; i < w * h && exp_len == (uint32_t)(w * h * 4); i++) {
            uint32_t want = (uint32_t)exp[i * 4] | ((uint32_t)exp[i * 4 + 1] << 8) |
                            ((uint32_t)exp[i * 4 + 2] << 16) | ((uint32_t)exp[i * 4 + 3] << 24);
            if (c.px[i] != want) {
                CHECK(0, "%s: pixel (%d,%d) is %08x, want %08x", name, i % w, i / w, c.px[i], want);
                break;
            }
        }

        for (uint32_t cut = 0; cut < len; cut += 7) {
            c.rows = 0;
            png_decode(png, cut, put_row, &c);
        }
        for (int i = 0; i < 300; i++) {
            uint8_t *bad = mutate(png, len);
            if (png_info(bad, len, &info))
                png_decode(bad, len, put_row, &c);
            free(bad);
        }
        free(c.px);
        free(png);
        free(exp);
    }
    CHECK(images == 48, "%d images", images);
    static const uint8_t not_png[] = "GIF89a....";
    PngInfo info;
    CHECK(!png_is_png(not_png, sizeof not_png) && !png_info(not_png, sizeof not_png, &info),
          "not a PNG");
    free(list);
}

/* ---- ZIP ---- */

/* Reads every entry and checks its size and CRC. */
static void check_archive(const char *what, Zip *z)
{
    CHECK(z != NULL, "%s: open", what);
    if (!z)
        return;
    for (uint32_t i = 0; i < zip_count(z); i++) {
        ZipEntry *e = zip_entry(z, i);
        uint32_t  len = 0;
        uint8_t  *data = zip_read(z, e, &len);
        CHECK(data && len == e->usize && data[len] == 0, "%s: %s: read", what, e->name);
        if (data)
            CHECK(zip_crc32(0, data, len) == e->crc, "%s: %s: CRC", what, e->name);
        free(data);
    }
}

static void test_zip(const char *dir, int njars, char **jars)
{
    uint32_t len;
    uint8_t *dup = load(dir, "dup.zip", &len);
    if (dup) {
        Zip *z = zip_open_mem(dup, len);
        check_archive("dup.zip", z);
        if (z) {
            CHECK(zip_count(z) == 4, "dup.zip: %u entries", zip_count(z));
            uint32_t n;
            uint8_t *a = zip_read_name(z, "a.txt", &n);
            CHECK(a && n == 6 && memcmp(a, "second", 6) == 0, "dup.zip: the last a.txt counts");
            free(a);
            a = zip_read_name(z, "/dir/deflated.txt", &n);
            CHECK(a && n == 4500 && zip_entry(z, 2)->method == 8, "dup.zip: leading slash, deflated");
            free(a);
            CHECK(zip_find(z, "missing") == NULL && zip_entry(z, 4) == NULL, "dup.zip: missing");
            zip_close(z);
        }
        for (int i = 0; i < 3000; i++) {
            uint8_t *bad = mutate(dup, len);
            Zip     *z2 = zip_open_mem(bad, len);
            for (uint32_t k = 0; z2 && k < zip_count(z2); k++)
                free(zip_read(z2, zip_entry(z2, k), NULL));
            zip_close(z2);
            free(bad);
        }
        for (uint32_t cut = 0; cut < len; cut++)
            zip_close(zip_open_mem(dup, cut));
        free(dup);
    }

    uint32_t crc = zip_crc32(0, (const uint8_t *)"123456789", 9);
    CHECK(crc == 0xCBF43926u, "CRC-32 check value: %08x", crc);

    for (int j = 0; j < njars; j++) {
        Zip *z = zip_open_file(jars[j]);
        check_archive(jars[j], z);
        uint32_t entries = z ? zip_count(z) : 0;
        CHECK(entries > 0, "%s: no entries", jars[j]);
        zip_close(z);
        /* The same from memory, and damaged copies. */
        FILE *f = fopen(jars[j], "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *mem = malloc((size_t)size);
        if (mem && fread(mem, 1, (size_t)size, f) == (size_t)size) {
            z = zip_open_mem(mem, (uint32_t)size);
            check_archive(jars[j], z);
            zip_close(z);
            for (int i = 0; i < 200; i++) {
                uint8_t *bad = mutate(mem, (uint32_t)size);
                /* Mostly the directory at the end, where damage matters. */
                for (int k = 0; k < 4; k++)
                    bad[size - 1 - (long)(rng() % (uint32_t)(size < 4096 ? size : 4096))] = (uint8_t)rng();
                Zip *z2 = zip_open_mem(bad, (uint32_t)size);
                for (uint32_t k = 0; z2 && k < zip_count(z2) && k < 64; k++)
                    free(zip_read(z2, zip_entry(z2, k), NULL));
                zip_close(z2);
                free(bad);
            }
        }
        free(mem);
        fclose(f);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: util_test inflate|png|zip DIR [JAR...]\n");
        return 2;
    }
    if (strcmp(argv[1], "inflate") == 0)
        test_inflate(argv[2]);
    else if (strcmp(argv[1], "png") == 0)
        test_png(argv[2]);
    else if (strcmp(argv[1], "zip") == 0)
        test_zip(argv[2], argc - 3, argv + 3);
    else
        return 2;
    printf("%s: %s\n", argv[1], failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
