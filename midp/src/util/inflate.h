/*
 * inflate.h - DEFLATE decompression (RFC 1951), for JAR entries, and the
 * zlib wrapper around it (RFC 1950), for PNG image data.
 */
#ifndef MIDP_INFLATE_H
#define MIDP_INFLATE_H

#include <stdint.h>

/* Decompresses the raw DEFLATE stream src into dst. Returns the number of
 * bytes written, or -1 if the stream is damaged or does not fit in dst.
 * Bytes after the last block are ignored. */
int32_t inflate_raw(uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len);

/* The same for a zlib stream: a two-byte header (deflate, no preset
 * dictionary), then DEFLATE data. The Adler-32 trailer is not checked:
 * phones did not insist on it, and games' images are not always right. */
int32_t inflate_zlib(uint8_t *dst, uint32_t dst_len, const uint8_t *src, uint32_t src_len);

#endif
