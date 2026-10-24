/*
 * zip.h - reading ZIP archives (JARs), from memory or from a file.
 */
#ifndef MIDP_ZIP_H
#define MIDP_ZIP_H

#include <stdint.h>
#include <stdio.h>

typedef struct ZipEntry {
    const char *name;
    uint16_t    method;      /* 0 stored, 8 deflated */
    uint32_t    crc;         /* CRC-32 of the contents, as the archive says */
    uint32_t    csize, usize;
    uint32_t    local_off;   /* of the entry's local header */
} ZipEntry;

typedef struct Zip Zip;

/* Opens an archive in memory; data is not copied and must outlive the Zip. */
Zip *zip_open_mem(const uint8_t *data, uint32_t size);
/* Opens an archive file; entries are read when asked for. */
Zip *zip_open_file(const char *path);
void zip_close(Zip *z);

/* The entry with this name (a leading '/' is ignored), or NULL. When an
 * archive has several entries of the same name, the last one counts. */
ZipEntry *zip_find(Zip *z, const char *name);
/* Entries in the order of the central directory. */
uint32_t  zip_count(Zip *z);
ZipEntry *zip_entry(Zip *z, uint32_t index);

/* The contents of an entry in a malloc'd buffer with a NUL byte after them
 * (not counted in *len), or NULL if they cannot be read. The CRC is not
 * checked: phones did not insist on it. */
uint8_t *zip_read(Zip *z, ZipEntry *e, uint32_t *len);
uint8_t *zip_read_name(Zip *z, const char *name, uint32_t *len);

/* CRC-32 (ISO 3309, as in ZIP and PNG) of data, continuing from crc
 * (0 to start). */
uint32_t zip_crc32(uint32_t crc, const uint8_t *data, uint32_t len);

#endif
