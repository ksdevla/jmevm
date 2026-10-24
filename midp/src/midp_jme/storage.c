/*
 * storage.c - jmevm.midp.Storage on jmevm: the key/value store behind
 * javax.microedition.rms.
 *
 * The stores are kept in memory, so they are part of every snapshot: loading
 * a save state also brings back the game's own saves as they were. Every
 * change is written through to the platform data directory as well, one
 * file per key (hex-encoded name + ".rms"), and loading a snapshot
 * rewrites the directory to match.
 *
 * The stores in memory are what the game sees. Writing the files can fail
 * (a full disk, a path too long for the host), and that must not change
 * what the game does, or runs would depend on where the data directory
 * is: a failed write is reported on the console and the game goes on.
 */
#include "midp_jme.h"
#include "../pal/pal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXT ".rms"

typedef struct Store {
    char    *key;    /* UTF-8 */
    uint8_t *data;
    uint32_t len;
} Store;

static Store   *stores;  /* sorted by key, so list() is deterministic */
static uint32_t count, cap;

/* ---- files ---- */

/* The file of a store, with room for a ".tmp" suffix, in a malloc'd
 * buffer; NULL without a data directory or memory. */
static char *key_path(const char *key)
{
    const char *dir = pal_data_dir();
    if (!dir)
        return NULL;
    size_t n = strlen(key);
    char  *path = malloc(strlen(dir) + 1 + 2 * n + sizeof EXT + 4);
    if (!path)
        return NULL;
    char *p = path + sprintf(path, "%s/", dir);
    for (size_t i = 0; i < n; i++)
        p += sprintf(p, "%02x", (unsigned char)key[i]);
    strcpy(p, EXT);
    return path;
}

static void file_write(const char *key, const uint8_t *data, uint32_t len)
{
    char *path = key_path(key);
    char *tmp = path ? malloc(strlen(path) + 5) : NULL;
    bool  ok = false;
    if (tmp) {
        /* A temporary file first, so a crash never leaves half a store. */
        sprintf(tmp, "%s.tmp", path);
        FILE *f = fopen(tmp, "wb");
        if (f) {
            ok = fwrite(data, 1, len, f) == len;
            ok = fclose(f) == 0 && ok;
            if (ok) {
                remove(path);
                ok = rename(tmp, path) == 0;
            }
            if (!ok)
                remove(tmp);
        }
    }
    if (!ok && pal_data_dir()) {
        char msg[300];
        int  n = snprintf(msg, sizeof msg, "jmevm-midp: cannot write record store \"%.200s\"\n", key);
        pal_console_write(msg, n < (int)sizeof msg ? n : (int)sizeof msg - 1);
    }
    free(tmp);
    free(path);
}

static void file_delete(const char *key)
{
    char *path = key_path(key);
    if (path)
        remove(path);
    free(path);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* ---- the table ---- */

static uint32_t find(const char *key, bool *found)
{
    uint32_t lo = 0, hi = count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        int      d = strcmp(stores[mid].key, key);
        if (d == 0) {
            *found = true;
            return mid;
        }
        if (d < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *found = false;
    return lo;
}

/* Takes ownership of key and data. */
static bool put(char *key, uint8_t *data, uint32_t len)
{
    bool     found;
    uint32_t i = find(key, &found);
    if (found) {
        free(stores[i].data);
        free(key);
        stores[i].data = data;
        stores[i].len = len;
        return true;
    }
    if (count == cap) {
        uint32_t n = cap ? cap * 2 : 16;
        Store   *a = realloc(stores, n * sizeof(Store));
        if (!a)
            return false;
        stores = a;
        cap = n;
    }
    memmove(&stores[i + 1], &stores[i], (count - i) * sizeof(Store));
    stores[i] = (Store){key, data, len};
    count++;
    return true;
}

static void clear_all(void)
{
    for (uint32_t i = 0; i < count; i++) {
        free(stores[i].key);
        free(stores[i].data);
    }
    count = 0;
}

static char *dup_bytes(const void *p, size_t n, size_t extra)
{
    char *d = malloc(n + extra);
    if (d && n)
        memcpy(d, p, n);
    return d;
}

static void load_file(const char *file, void *ctx)
{
    (void)ctx;
    size_t len = strlen(file), ext = strlen(EXT);
    if (len <= ext || (len - ext) % 2 || strcmp(file + len - ext, EXT) != 0)
        return;
    char *key = malloc((len - ext) / 2 + 1);
    if (!key)
        return;
    for (size_t i = 0; i < len - ext; i += 2) {
        int hi = hexval(file[i]), lo = hexval(file[i + 1]);
        if (hi < 0 || lo < 0) {
            free(key);
            return;
        }
        key[i / 2] = (char)(hi * 16 + lo);
    }
    key[(len - ext) / 2] = 0;
    char *path = key_path(key);
    FILE *f = path ? fopen(path, "rb") : NULL;
    free(path);
    if (!f) {
        free(key);
        return;
    }
    fseek(f, 0, SEEK_END);
    long     size = ftell(f);
    uint8_t *data = size >= 0 ? malloc((size_t)size + 1) : NULL;
    fseek(f, 0, SEEK_SET);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size || !put(key, data, (uint32_t)size)) {
        free(key);
        free(data);
    }
    fclose(f);
}

/* ---- natives ---- */

static char *key_arg(JmeCall *c, unsigned slot)
{
    return jme_string_utf8(jme_call_vm(c), jme_arg_ref(c, slot));
}

/* static byte[] read(String key): null if there is no such store */
static int storage_read(JmeCall *c)
{
    if (!jme_arg_ref(c, 0))
        return midp_npe(c);
    char *key = key_arg(c, 0);
    if (!key)
        return midp_throw(c, "java/lang/OutOfMemoryError", NULL);
    bool     found;
    uint32_t i = find(key, &found);
    free(key);
    if (!found)
        return JME_NATIVE_RETURN;
    JmeRef a = jme_new_array(c, "[B", (int32_t)stores[i].len);
    if (!a)
        return JME_NATIVE_THROW;
    memcpy(jme_array_data(jme_call_vm(c), a), stores[i].data, stores[i].len);
    jme_return_ref(c, a);
    return JME_NATIVE_RETURN;
}

/* static boolean write(String key, byte[] data) */
static int storage_write(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    JmeRef data = jme_arg_ref(c, 1);
    if (!jme_arg_ref(c, 0) || !data)
        return midp_npe(c);
    char    *key = key_arg(c, 0);
    uint32_t len = jme_array_length(vm, data);
    uint8_t *copy = (uint8_t *)dup_bytes(jme_array_data(vm, data), len, 1);
    bool     ok = key && copy;
    if (ok) {
        file_write(key, copy, len);
        char *owned = (char *)dup_bytes(key, strlen(key) + 1, 0);
        ok = owned && put(owned, copy, len);
        if (!ok)
            free(owned);
    }
    if (!ok)
        free(copy);
    free(key);
    jme_return_int(c, ok);
    return JME_NATIVE_RETURN;
}

/* static boolean delete(String key) */
static int storage_delete(JmeCall *c)
{
    if (!jme_arg_ref(c, 0))
        return midp_npe(c);
    char *key = key_arg(c, 0);
    if (!key)
        return midp_throw(c, "java/lang/OutOfMemoryError", NULL);
    bool     found;
    uint32_t i = find(key, &found);
    if (found) {
        file_delete(key);
        free(stores[i].key);
        free(stores[i].data);
        memmove(&stores[i], &stores[i + 1], (count - i - 1) * sizeof(Store));
        count--;
    }
    free(key);
    jme_return_int(c, found);
    return JME_NATIVE_RETURN;
}

/* static String[] list(): every key, sorted */
static int storage_list(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    JmeRef a = jme_new_array(c, "[Ljava/lang/String;", (int32_t)count);
    if (!a)
        return JME_NATIVE_THROW;
    jme_root(vm, &a);
    for (uint32_t i = 0; i < count; i++) {
        JmeRef s = jme_new_string(c, stores[i].key);
        if (!s) {
            jme_unroot(vm, 1);
            return JME_NATIVE_THROW;
        }
        ((JmeRef *)jme_array_data(vm, a))[i] = s;
    }
    jme_unroot(vm, 1);
    jme_return_ref(c, a);
    return JME_NATIVE_RETURN;
}

static const JmeNative natives[] = {
    {"jmevm/midp/Storage", "read", NULL, storage_read},
    {"jmevm/midp/Storage", "write", NULL, storage_write},
    {"jmevm/midp/Storage", "delete", NULL, storage_delete},
    {"jmevm/midp/Storage", "list", NULL, storage_list},
};

/* ---- state: count, then key length, key, data length, data ---- */

static JmeStatus save(void *user, JmeWriteFn write, void *write_user)
{
    (void)user;
    StateOut o = {write, write_user, JME_OK};
    state_put_u32(&o, count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t n = (uint32_t)strlen(stores[i].key);
        state_put_u32(&o, n);
        state_put(&o, stores[i].key, n);
        state_put_u32(&o, stores[i].len);
        state_put(&o, stores[i].data, stores[i].len);
    }
    return o.status;
}

static JmeStatus load(void *user, const void *data, size_t len)
{
    (void)user;
    /* Check everything first: on an error nothing may change. */
    StateIn  in = {data, (const uint8_t *)data + len, false};
    uint32_t n = state_get_u32(&in);
    for (uint32_t i = 0; i < n && !in.bad; i++) {
        uint32_t kn = state_get_u32(&in);
        const uint8_t *k = state_get(&in, kn);
        if (k && memchr(k, 0, kn))
            in.bad = true;
        state_get(&in, state_get_u32(&in));
    }
    if (in.bad || in.p != in.end)
        return JME_E_FORMAT;

    for (uint32_t i = 0; i < count; i++)
        file_delete(stores[i].key);
    clear_all();
    in = (StateIn){data, (const uint8_t *)data + len, false};
    n = state_get_u32(&in);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t       kn = state_get_u32(&in);
        char          *key = dup_bytes(state_get(&in, kn), kn, 1);
        uint32_t       dn = state_get_u32(&in);
        const uint8_t *d = state_get(&in, dn);
        uint8_t       *copy = (uint8_t *)dup_bytes(d, dn, 1);
        if (!key || !copy) {
            free(key);
            free(copy);
            return JME_E_NOMEM;
        }
        key[kn] = 0;
        file_write(key, copy, dn);
        if (!put(key, copy, dn)) {
            free(key);
            free(copy);
            return JME_E_NOMEM;
        }
    }
    return JME_OK;
}

bool midp_jme_storage_init(JmeVM *vm)
{
    clear_all();
    if (pal_data_dir())
        pal_data_list(load_file, NULL);
    jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
    return jme_register_state(vm, "rms", save, load, NULL) == JME_OK;
}

void midp_jme_storage_shutdown(void)
{
    clear_all();
    free(stores);
    stores = NULL;
    cap = 0;
}
