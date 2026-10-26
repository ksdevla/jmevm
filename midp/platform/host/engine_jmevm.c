/*
 * engine_jmevm.c - the runner's engine on jmevm, with the MIDP natives in
 * src/midp_jme. Classes come from the class library jar, then the
 * application (jar or -cp); resources from the application. Save states are
 * jmevm snapshots written to files.
 */
#include "engine.h"
#include "../../src/midp_jme/midp_jme.h"
#include "../../src/pal/pal.h"
#include "../../src/util/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Cached {
    struct Cached *next;
    char          *name;
    uint8_t       *data;   /* NULL: not found (also cached) */
    uint32_t       len;
} Cached;

static JmeVM      *vm;
static EngineOptions options;
static Zip        *system_zip, *app_zip;
static const char *app_dir;
static Cached     *classes, *files;
/* The rewind history, made on the first push within options.rewind_mb. */
static JmeRewind  *ring;
/* The every-frame history, made on the first record within options.history_mb. */
static JmeHistory *history;

static bool ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

static uint8_t *read_file(const char *path, uint32_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (data && fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *len = (uint32_t)size;
    return data;
}

static uint8_t *read_app(const char *name, uint32_t *len)
{
    if (app_zip)
        return zip_read_name(app_zip, name, len);
    if (app_dir) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", app_dir, name);
        return read_file(path, len);
    }
    return NULL;
}

/* Bytes stay valid until the engine stops: the VM points into them, and
 * snapshot loads ask for the same classes again. */
static Cached *lookup(Cached **list, const char *name, bool is_class)
{
    for (Cached *c = *list; c; c = c->next)
        if (strcmp(c->name, name) == 0)
            return c;
    Cached *c = calloc(1, sizeof(Cached));
    if (!c)
        return NULL;
    c->name = malloc(strlen(name) + 1);
    if (!c->name) {
        free(c);
        return NULL;
    }
    strcpy(c->name, name);
    if (is_class) {
        char entry[1024];
        snprintf(entry, sizeof entry, "%s.class", name);
        c->data = system_zip ? zip_read_name(system_zip, entry, &c->len) : NULL;
        if (!c->data)
            c->data = read_app(entry, &c->len);
    } else {
        c->data = read_app(name, &c->len);
    }
    c->next = *list;
    *list = c;
    return c;
}

static JmeStatus class_source(void *user, const char *name, const uint8_t **data, size_t *len)
{
    (void)user;
    Cached *c = lookup(&classes, name, true);
    if (!c)
        return JME_E_NOMEM;
    if (!c->data)
        return JME_E_NOTFOUND;
    *data = c->data;
    *len = c->len;
    return JME_OK;
}

static JmeStatus resource_source(void *user, const char *name, const uint8_t **data, size_t *len)
{
    (void)user;
    Cached *c = lookup(&files, name, false);
    if (!c)
        return JME_E_NOMEM;
    if (!c->data)
        return JME_E_NOTFOUND;
    *data = c->data;
    *len = c->len;
    return JME_OK;
}

static void console(void *user, int stream, const char *data, size_t len)
{
    (void)user;
    (void)stream;
    pal_console_write(data, (int)len);
}

static void free_cache(Cached **list)
{
    while (*list) {
        Cached *next = (*list)->next;
        free((*list)->name);
        free((*list)->data);
        free(*list);
        *list = next;
    }
}

bool engine_start(const EngineOptions *o, const EngineApp *app)
{
    options = *o;
    if (options.ticks_per_ms == 0)
        options.ticks_per_ms = 10000;
    if (options.trace_classes || options.trace_exceptions)
        fprintf(stderr, "jmevm-midp: class and exception tracing are not available with jmevm\n");

    system_zip = zip_open_file(app->classlib);
    if (!system_zip) {
        fprintf(stderr, "jmevm-midp: cannot open %s\n", app->classlib);
        return false;
    }
    const char *src = app->main_class ? app->cp : app->jar;
    if (src && (ends_with(src, ".jar") || ends_with(src, ".zip"))) {
        app_zip = zip_open_file(src);
        if (!app_zip) {
            fprintf(stderr, "jmevm-midp: cannot open %s\n", src);
            return false;
        }
    } else {
        app_dir = src;
    }

    JmeOptions jo;
    jme_options_default(&jo);
    jo.heap_bytes = options.heap_bytes;
    jo.ticks_per_ms = options.ticks_per_ms;
    jo.epoch_ms = options.epoch_ms;
    jo.gc_stress = options.gc_stress;
    JmeStatus st = jme_create(&jo, &vm);
    if (st != JME_OK) {
        fprintf(stderr, "jmevm-midp: cannot create the VM: %s\n", jme_status_name(st));
        return false;
    }
    jme_set_class_source(vm, class_source, NULL);
    jme_set_console(vm, console, NULL);
    MidpSetup setup = {app->main_class ? NULL : app->jad_text, resource_source, NULL, options.trace_audio};
    if (!midp_jme_init(vm, &setup)) {
        fprintf(stderr, "jmevm-midp: cannot set up MIDP\n");
        return false;
    }

    if (app->main_class) {
        st = jme_start(vm, app->main_class, app->argc, (const char *const *)app->argv);
    } else {
        const char *args[2] = {"-midlet", app->midlet ? app->midlet : ""};
        st = jme_start(vm, "jmevm/midp/Boot", 2, args);
    }
    if (st != JME_OK) {
        fprintf(stderr, "jmevm-midp: cannot start: %s\n", jme_status_name(st));
        return false;
    }
    return true;
}

bool engine_run_until(uint64_t ticks)
{
    if (!midp_jme_poll(vm))
        return false;
    jme_run_until(vm, ticks);
    return !jme_finished(vm, NULL);
}

int engine_exit_code(void)
{
    int code = 0;
    jme_finished(vm, &code);
    return code;
}

uint32_t engine_ticks_per_ms(void)
{
    return options.ticks_per_ms;
}

uint64_t engine_ticks(void)
{
    return vm ? jme_ticks(vm) : 0;
}

int64_t engine_time_ms(void)
{
    return vm ? (int64_t)(jme_ticks(vm) / options.ticks_per_ms) : 0;
}

double engine_busy_percent(void)
{
    return 0.0; /* jmevm does not count idle time separately */
}

void engine_stop(void)
{
    midp_jme_shutdown();
    jme_rewind_destroy(ring);
    ring = NULL;
    jme_history_destroy(history);
    history = NULL;
    jme_destroy(vm);
    vm = NULL;
    free_cache(&classes);
    free_cache(&files);
    if (system_zip)
        zip_close(system_zip);
    if (app_zip)
        zip_close(app_zip);
    system_zip = app_zip = NULL;
    app_dir = NULL;
}

void engine_dump_threads(void)
{
    fprintf(stderr, "jmevm-midp: --dump-threads is not available with jmevm\n");
}

static JmeStatus write_file(void *user, const void *data, size_t len)
{
    return fwrite(data, 1, len, (FILE *)user) == len ? JME_OK : JME_E_STATE;
}

bool engine_save_state(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = jme_snapshot_save(vm, write_file, f) == JME_OK;
    ok = fclose(f) == 0 && ok;
    if (!ok)
        remove(path);
    return ok;
}

bool engine_rewind_push(void)
{
    size_t budget = (size_t)(options.rewind_mb ? options.rewind_mb : 256) << 20;
    if (!ring && jme_rewind_create(budget, 30, &ring) != JME_OK)
        return false;
    return jme_rewind_push(ring, vm) == JME_OK;
}

bool engine_rewind_back(void)
{
    return engine_rewind_to(1);
}

uint32_t engine_rewind_count(void)
{
    return ring ? jme_rewind_count(ring) : 0;
}

bool engine_rewind_to(uint32_t back)
{
    uint32_t n = engine_rewind_count();
    if (back >= n || jme_rewind_load(ring, vm, back) != JME_OK)
        return false;
    jme_rewind_truncate(ring, n - back);
    if (history)
        jme_history_reset(history);
    return true;
}

bool engine_history_record(void)
{
    if (!history && (options.history_mb == 0 ||
                     jme_history_create(vm, (size_t)options.history_mb << 20, &history) != JME_OK))
        return false;
    return jme_history_record(history) == JME_OK;
}

bool engine_history_back(void)
{
    if (!history || jme_history_back(history) != JME_OK)
        return false;
    midp_jme_screen_resync();   /* show the frame of that moment now */
    return true;
}

uint32_t engine_history_count(void)
{
    return history ? jme_history_count(history) : 0;
}

bool engine_load_state(const char *path)
{
    uint32_t len;
    uint8_t *data = read_file(path, &len);
    if (!data)
        return false;
    JmeStatus st = jme_snapshot_load(vm, data, len);
    free(data);
    if (st != JME_OK)
        fprintf(stderr, "jmevm-midp: cannot load %s: %s\n", path, jme_status_name(st));
    else {
        if (ring)
            jme_rewind_truncate(ring, 0);   /* that history belongs to another timeline */
        if (history)
            jme_history_reset(history);
    }
    return st == JME_OK;
}
