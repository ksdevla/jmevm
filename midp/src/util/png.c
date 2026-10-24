/*
 * png.c - PNG decoding, after the PNG specification (ISO/IEC 15948).
 *
 * The chunks are read once for the header, palette and transparency, and
 * the image data of all IDAT chunks is joined and inflated in one go. Each
 * row is then unfiltered against the one above it and converted to ARGB.
 * Chunk CRCs are not checked: phones did not insist on them, and games
 * that patch palettes in their images do not always update them.
 */
#include "png.h"
#include "inflate.h"

#include <stdlib.h>
#include <string.h>

#define MAX_SIDE 4096

enum { GRAY = 0, RGB = 2, INDEXED = 3, GRAY_ALPHA = 4, RGB_ALPHA = 6 };

typedef struct Image {
    int      width, height;
    int      depth;       /* bits per sample */
    int      type;        /* colour type */
    bool     interlaced;
    int      samples;     /* per pixel */
    uint32_t palette[256];
    bool     transparent; /* tRNS present */
    uint16_t key[3];      /* the transparent gray or RGB value */
    uint8_t *data;        /* joined IDAT contents, when asked for */
    uint32_t data_len;
} Image;

static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

/* Adam7: where each pass starts, and its spacing. */
static const uint8_t pass_x[7] = {0, 4, 0, 2, 0, 1, 0};
static const uint8_t pass_y[7] = {0, 0, 4, 0, 2, 0, 1};
static const uint8_t pass_dx[7] = {8, 8, 4, 4, 2, 2, 1};
static const uint8_t pass_dy[7] = {8, 8, 8, 4, 4, 2, 2};

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool png_is_png(const uint8_t *data, uint32_t len)
{
    return len >= 8 && memcmp(data, signature, 8) == 0;
}

/* Whether the colour type and bit depth go together. */
static bool valid_format(int type, int depth)
{
    switch (type) {
    case GRAY:
        return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
    case INDEXED:
        return depth == 1 || depth == 2 || depth == 4 || depth == 8;
    case RGB:
    case GRAY_ALPHA:
    case RGB_ALPHA:
        return depth == 8 || depth == 16;
    default:
        return false;
    }
}

/* Reads the chunks up to IEND (or the end of the data). With want_data the
 * IDAT contents are joined into img->data. */
static bool read_chunks(const uint8_t *data, uint32_t len, Image *img, bool want_data)
{
    memset(img, 0, sizeof *img);
    if (!png_is_png(data, len))
        return false;
    /* Entries the palette leaves out are opaque black. */
    for (int i = 0; i < 256; i++)
        img->palette[i] = 0xFF000000u;
    uint8_t alpha[256];
    int     n_alpha = 0;
    bool    have_header = false;

    for (uint32_t at = 8; len - at >= 12;) {
        uint32_t       size = get32(data + at);
        const uint8_t *type = data + at + 4;
        const uint8_t *body = data + at + 8;
        if (size > len - at - 12)
            return false;
        if (memcmp(type, "IHDR", 4) == 0 && !have_header && size >= 13) {
            uint32_t w = get32(body), h = get32(body + 4);
            img->depth = body[8];
            img->type = body[9];
            img->interlaced = body[12] == 1;
            if (w == 0 || h == 0 || w > MAX_SIDE || h > MAX_SIDE || body[12] > 1 ||
                !valid_format(img->type, img->depth))
                return false;
            img->width = (int)w;
            img->height = (int)h;
            img->samples = img->type == RGB_ALPHA ? 4 : img->type == RGB ? 3
                         : img->type == GRAY_ALPHA ? 2 : 1;
            have_header = true;
        } else if (memcmp(type, "PLTE", 4) == 0) {
            uint32_t n = size / 3 < 256 ? size / 3 : 256;
            for (uint32_t i = 0; i < n; i++)
                img->palette[i] = 0xFF000000u | ((uint32_t)body[i * 3] << 16) |
                                  ((uint32_t)body[i * 3 + 1] << 8) | body[i * 3 + 2];
        } else if (memcmp(type, "tRNS", 4) == 0) {
            img->transparent = true;
            n_alpha = size < 256 ? (int)size : 256;
            memcpy(alpha, body, (size_t)n_alpha);
            for (int i = 0; i < 3 && (uint32_t)i * 2 + 1 < size; i++)
                img->key[i] = (uint16_t)((body[i * 2] << 8) | body[i * 2 + 1]);
        } else if (memcmp(type, "IDAT", 4) == 0 && want_data) {
            uint8_t *joined = realloc(img->data, (size_t)img->data_len + size + 1);
            if (!joined)
                return false;
            img->data = joined;
            memcpy(img->data + img->data_len, body, size);
            img->data_len += size;
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        at += size + 12;
    }
    if (img->type == INDEXED)
        for (int i = 0; i < n_alpha; i++)
            img->palette[i] = (img->palette[i] & 0x00FFFFFFu) | ((uint32_t)alpha[i] << 24);
    return have_header;
}

bool png_info(const uint8_t *data, uint32_t len, PngInfo *info)
{
    Image img;
    if (!read_chunks(data, len, &img, false))
        return false;
    info->width = img.width;
    info->height = img.height;
    info->has_alpha = img.type == GRAY_ALPHA || img.type == RGB_ALPHA || img.transparent;
    return true;
}

/* ---- rows ---- */

static int paeth(int left, int up, int up_left)
{
    int p = left + up - up_left;
    int dl = abs(p - left), du = abs(p - up), dul = abs(p - up_left);
    if (dl <= du && dl <= dul)
        return left;
    return du <= dul ? up : up_left;
}

/* Undoes the row's filter in place. above is the previous row of the same
 * pass after unfiltering, or NULL for the first. bpp is bytes per pixel,
 * at least 1. */
static bool unfilter(int filter, uint8_t *row, const uint8_t *above, size_t n, size_t bpp)
{
    for (size_t i = 0; i < n; i++) {
        int left = i >= bpp ? row[i - bpp] : 0;
        int up = above ? above[i] : 0;
        int up_left = above && i >= bpp ? above[i - bpp] : 0;
        int add;
        switch (filter) {
        case 0: return true;
        case 1: add = left; break;
        case 2: add = up; break;
        case 3: add = (left + up) / 2; break;
        case 4: add = paeth(left, up, up_left); break;
        default: return false;
        }
        row[i] = (uint8_t)(row[i] + add);
    }
    return filter >= 0 && filter <= 4;
}

/* Sample i of the row, at the image's bit depth. */
static uint32_t sample(const uint8_t *row, int i, int depth)
{
    switch (depth) {
    case 8:  return row[i];
    case 16: return ((uint32_t)row[i * 2] << 8) | row[i * 2 + 1];
    default: {
        int per_byte = 8 / depth;
        int shift = (per_byte - 1 - i % per_byte) * depth;
        return (row[i / per_byte] >> shift) & ((1u << depth) - 1);
    }
    }
}

static void to_argb(const Image *img, const uint8_t *row, int n, uint32_t *out)
{
    uint32_t max = (1u << img->depth) - 1;
    for (int x = 0; x < n; x++) {
        if (img->type == INDEXED) {
            out[x] = img->palette[sample(row, x, img->depth) & 0xFF];
            continue;
        }
        int      base = x * img->samples;
        uint32_t v[4];
        for (int s = 0; s < img->samples; s++)
            v[s] = sample(row, base + s, img->depth);
        uint32_t r, g, b, a = 255;
        switch (img->type) {
        case GRAY:
            r = g = b = v[0];
            if (img->transparent && v[0] == img->key[0])
                a = 0;
            break;
        case GRAY_ALPHA:
            r = g = b = v[0];
            a = v[1] * 255 / max;
            break;
        case RGB:
            r = v[0], g = v[1], b = v[2];
            if (img->transparent && r == img->key[0] && g == img->key[1] && b == img->key[2])
                a = 0;
            break;
        default:   /* RGB_ALPHA */
            r = v[0], g = v[1], b = v[2];
            a = v[3] * 255 / max;
            break;
        }
        out[x] = (a << 24) | ((r * 255 / max) << 16) | ((g * 255 / max) << 8) | (b * 255 / max);
    }
}

static int pass_size(int full, int start, int step)
{
    return full > start ? (full - start + step - 1) / step : 0;
}

bool png_decode(const uint8_t *data, uint32_t len, PngRowFn fn, void *ctx)
{
    Image img;
    if (!read_chunks(data, len, &img, true) || !img.data) {
        free(img.data);
        return false;
    }
    int    passes = img.interlaced ? 7 : 1;
    size_t bits = (size_t)img.samples * img.depth;   /* per pixel */
    size_t bpp = (bits + 7) / 8;

    /* Every row of every pass is a filter byte and the packed samples. */
    size_t total = 0;
    for (int p = 0; p < passes; p++) {
        int w = img.interlaced ? pass_size(img.width, pass_x[p], pass_dx[p]) : img.width;
        int h = img.interlaced ? pass_size(img.height, pass_y[p], pass_dy[p]) : img.height;
        if (w > 0 && h > 0)
            total += (size_t)h * (1 + ((size_t)w * bits + 7) / 8);
    }
    uint8_t  *raw = malloc(total + 1);
    uint32_t *argb = malloc(sizeof(uint32_t) * (size_t)img.width);
    bool ok = raw && argb && inflate_zlib(raw, (uint32_t)total, img.data, img.data_len) == (int32_t)total;
    free(img.data);

    uint8_t *row = raw;
    for (int p = 0; ok && p < passes; p++) {
        int x0 = img.interlaced ? pass_x[p] : 0, dx = img.interlaced ? pass_dx[p] : 1;
        int y0 = img.interlaced ? pass_y[p] : 0, dy = img.interlaced ? pass_dy[p] : 1;
        int w = pass_size(img.width, x0, dx), h = pass_size(img.height, y0, dy);
        if (w == 0 || h == 0)
            continue;
        size_t         n = ((size_t)w * bits + 7) / 8;
        const uint8_t *above = NULL;
        for (int y = 0; y < h && ok; y++) {
            ok = unfilter(row[0], row + 1, above, n, bpp);
            if (ok) {
                to_argb(&img, row + 1, w, argb);
                fn(ctx, y0 + y * dy, x0, dx, argb, w);
            }
            above = row + 1;
            row += n + 1;
        }
    }
    free(raw);
    free(argb);
    return ok;
}
