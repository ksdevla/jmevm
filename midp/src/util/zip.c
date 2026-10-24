/*
 * zip.c - reading ZIP archives (APPNOTE.TXT).
 *
 * Only the central directory is trusted for the list of entries and their
 * sizes: J2ME JARs come from many obfuscators and packers, and local headers
 * or data descriptors are often wrong. A damaged central directory ends the
 * list where the damage starts. Names are found through a hash table with
 * open addressing.
 */
#include "zip.h"
#include "inflate.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define SIG_END     0x06054b50u   /* end of central directory */
#define SIG_CENTRAL 0x02014b50u
#define SIG_LOCAL   0x04034b50u
#define END_SIZE    22
#define CENTRAL_SIZE 46
#define LOCAL_SIZE  30
#define MAX_COMMENT 65535

struct Zip {
    const uint8_t *mem;      /* the archive in memory, or NULL */
    FILE          *file;     /* or the open file */
    uint32_t       size;
    ZipEntry      *entries;
    uint32_t       count;
    char          *names;    /* all names, NUL-terminated, one block */
    uint32_t      *slots;    /* entry index + 1, or 0 for an empty slot */
    uint32_t       mask;     /* number of slots - 1 */
};

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t hash_name(const char *s)
{
    uint32_t h = 5381;
    while (*s)
        h = h * 33 + (uint8_t)*s++;
    return h;
}

/* Reads len bytes at off; false if they are not all there. */
static bool read_bytes(Zip *z, uint32_t off, void *dst, uint32_t len)
{
    if (off > z->size || len > z->size - off)
        return false;
    if (z->mem) {
        memcpy(dst, z->mem + off, len);
        return true;
    }
    return fseek(z->file, (long)off, SEEK_SET) == 0 && fread(dst, 1, len, z->file) == len;
}

static void insert(Zip *z, uint32_t index)
{
    const char *name = z->entries[index].name;
    for (uint32_t i = hash_name(name) & z->mask;; i = (i + 1) & z->mask) {
        if (z->slots[i] == 0 || strcmp(z->entries[z->slots[i] - 1].name, name) == 0) {
            z->slots[i] = index + 1;   /* a later entry replaces an earlier one */
            return;
        }
    }
}

static bool read_directory(Zip *z)
{
    /* The end record is in the last 22 bytes plus at most a 64 KB comment;
     * the one nearest the end counts. */
    uint32_t tail = z->size < END_SIZE + MAX_COMMENT ? z->size : END_SIZE + MAX_COMMENT;
    if (tail < END_SIZE)
        return false;
    uint8_t *buf = malloc(tail);
    if (!buf || !read_bytes(z, z->size - tail, buf, tail)) {
        free(buf);
        return false;
    }
    uint32_t at = tail - END_SIZE + 1;
    do {
        at--;
    } while (at > 0 && get32(buf + at) != SIG_END);
    bool     found = get32(buf + at) == SIG_END;
    uint32_t total = get16(buf + at + 10);
    uint32_t dir_size = get32(buf + at + 12);
    uint32_t dir_off = get32(buf + at + 16);
    free(buf);
    if (!found || dir_size > z->size)
        return false;

    uint8_t *dir = malloc(dir_size + 1);
    z->entries = calloc(total + 1, sizeof(ZipEntry));
    z->names = malloc(dir_size + 1);
    uint32_t slots = 16;
    while (slots < total * 2)
        slots <<= 1;
    z->slots = calloc(slots, sizeof(uint32_t));
    z->mask = slots - 1;
    if (!dir || !z->entries || !z->names || !z->slots || !read_bytes(z, dir_off, dir, dir_size)) {
        free(dir);
        return false;
    }

    char    *names = z->names;
    uint32_t p = 0;
    while (z->count < total && p + CENTRAL_SIZE <= dir_size && get32(dir + p) == SIG_CENTRAL) {
        uint32_t name_len = get16(dir + p + 28);
        uint32_t next = p + CENTRAL_SIZE + name_len + get16(dir + p + 30) + get16(dir + p + 32);
        if (p + CENTRAL_SIZE + name_len > dir_size)
            break;
        ZipEntry *e = &z->entries[z->count];
        e->method = get16(dir + p + 10);
        e->crc = get32(dir + p + 16);
        e->csize = get32(dir + p + 20);
        e->usize = get32(dir + p + 24);
        e->local_off = get32(dir + p + 42);
        memcpy(names, dir + p + CENTRAL_SIZE, name_len);
        names[name_len] = '\0';
        e->name = names;
        names += name_len + 1;
        insert(z, z->count++);
        p = next;
    }
    free(dir);
    return true;
}

static Zip *open_zip(Zip *z)
{
    if (!read_directory(z)) {
        zip_close(z);
        return NULL;
    }
    return z;
}

Zip *zip_open_mem(const uint8_t *data, uint32_t size)
{
    Zip *z = calloc(1, sizeof(Zip));
    if (!z)
        return NULL;
    z->mem = data;
    z->size = size;
    return open_zip(z);
}

Zip *zip_open_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    Zip *z = calloc(1, sizeof(Zip));
    long size = -1;
    if (z && fseek(f, 0, SEEK_END) == 0)
        size = ftell(f);
    if (!z || size < 0 || (unsigned long)size > UINT32_MAX) {
        free(z);
        fclose(f);
        return NULL;
    }
    z->file = f;
    z->size = (uint32_t)size;
    return open_zip(z);
}

void zip_close(Zip *z)
{
    if (!z)
        return;
    if (z->file)
        fclose(z->file);
    free(z->entries);
    free(z->names);
    free(z->slots);
    free(z);
}

ZipEntry *zip_find(Zip *z, const char *name)
{
    if (name[0] == '/')
        name++;
    for (uint32_t i = hash_name(name) & z->mask; z->slots[i]; i = (i + 1) & z->mask) {
        ZipEntry *e = &z->entries[z->slots[i] - 1];
        if (strcmp(e->name, name) == 0)
            return e;
    }
    return NULL;
}

uint32_t zip_count(Zip *z)
{
    return z->count;
}

ZipEntry *zip_entry(Zip *z, uint32_t index)
{
    return index < z->count ? &z->entries[index] : NULL;
}

uint8_t *zip_read(Zip *z, ZipEntry *e, uint32_t *len)
{
    uint8_t local[LOCAL_SIZE];
    if (!read_bytes(z, e->local_off, local, LOCAL_SIZE) || get32(local) != SIG_LOCAL)
        return NULL;
    /* The local header's own name and extra field lengths place the data. */
    uint64_t data_off = (uint64_t)e->local_off + LOCAL_SIZE + get16(local + 26) + get16(local + 28);
    if (data_off > z->size)
        return NULL;
    /* Sizes a damaged archive cannot back up are refused before allocating:
     * stored data is in the file, and DEFLATE expands at most 1032 times. */
    if (e->method == 0 ? e->usize > z->size - data_off
                       : (uint64_t)e->usize > (uint64_t)e->csize * 1032 + 64)
        return NULL;

    uint8_t *out = malloc((size_t)e->usize + 1);
    if (!out)
        return NULL;
    bool ok = false;
    if (e->method == 0) {
        ok = read_bytes(z, (uint32_t)data_off, out, e->usize);
    } else if (e->method == 8) {
        uint8_t *in = malloc((size_t)e->csize + 1);
        ok = in && read_bytes(z, (uint32_t)data_off, in, e->csize) &&
             inflate_raw(out, e->usize, in, e->csize) == (int32_t)e->usize;
        free(in);
    }
    if (!ok) {
        free(out);
        return NULL;
    }
    out[e->usize] = 0;
    if (len)
        *len = e->usize;
    return out;
}

uint8_t *zip_read_name(Zip *z, const char *name, uint32_t *len)
{
    ZipEntry *e = zip_find(z, name);
    return e ? zip_read(z, e, len) : NULL;
}

uint32_t zip_crc32(uint32_t crc, const uint8_t *data, uint32_t len)
{
    static uint32_t table[256];
    static bool     ready;
    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
