/*
 * png.h - decoding PNG images: every colour type and bit depth, palettes
 * and tRNS transparency, Adam7 interlacing.
 */
#ifndef MIDP_PNG_H
#define MIDP_PNG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct PngInfo {
    int  width, height;
    bool has_alpha;    /* an alpha channel, or tRNS transparency */
} PngInfo;

/* Receives n pixels (0xAARRGGBB) of row y, for columns x0, x0 + step, ...
 * Interlaced images arrive in seven passes. */
typedef void (*PngRowFn)(void *ctx, int y, int x0, int step, const uint32_t *argb, int n);

/* Whether data starts with the PNG signature. */
bool png_is_png(const uint8_t *data, uint32_t len);
/* Size and transparency, without decoding. */
bool png_info(const uint8_t *data, uint32_t len, PngInfo *info);
/* Decodes the image row by row into fn. False if it is damaged; rows
 * delivered before the damage stay delivered. */
bool png_decode(const uint8_t *data, uint32_t len, PngRowFn fn, void *ctx);

#endif
