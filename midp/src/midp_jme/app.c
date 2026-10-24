/*
 * app.c - the application glue on jmevm: jmevm.midp.AppProperties (the .jad),
 * jmevm.midp.Resources (files of the jar) and jmevm.midp.Boot.exit.
 *
 * On jmevm the runner starts the program itself (main class or
 * jmevm.midp.Boot -midlet), so Boot.invokeMain is not needed here.
 */
#include "midp_jme.h"

#include <stdlib.h>
#include <string.h>

static char             *jad;
static JmeResourceSource resources;
static void             *resources_user;

static JmeRef new_bytes(JmeCall *c, const void *data, size_t len)
{
    if (len > INT32_MAX) {
        midp_throw(c, "java/lang/OutOfMemoryError", NULL);
        return 0;
    }
    JmeRef a = jme_new_array(c, "[B", (int32_t)len);
    if (a)
        memcpy(jme_array_data(jme_call_vm(c), a), data, len);
    return a;
}

/* static byte[] jad(): the .jad contents, or null */
static int app_jad(JmeCall *c)
{
    if (!jad)
        return JME_NATIVE_RETURN;
    JmeRef a = new_bytes(c, jad, strlen(jad));
    if (!a)
        return JME_NATIVE_THROW;
    jme_return_ref(c, a);
    return JME_NATIVE_RETURN;
}

/* static byte[] read(String name): a jar entry (no leading slash), or null */
static int resources_read(JmeCall *c)
{
    if (!jme_arg_ref(c, 0))
        return midp_npe(c);
    if (!resources)
        return JME_NATIVE_RETURN;
    char *name = jme_string_utf8(jme_call_vm(c), jme_arg_ref(c, 0));
    if (!name)
        return midp_throw(c, "java/lang/OutOfMemoryError", NULL);
    const uint8_t *data;
    size_t         len;
    JmeStatus      st = resources(resources_user, name, &data, &len);
    free(name);
    if (st != JME_OK)
        return JME_NATIVE_RETURN;
    JmeRef a = new_bytes(c, data, len);
    if (!a)
        return JME_NATIVE_THROW;
    jme_return_ref(c, a);
    return JME_NATIVE_RETURN;
}

/* static void exit(int code) */
static int boot_exit(JmeCall *c)
{
    return jme_exit(c, jme_arg_int(c, 0));
}

static const JmeNative natives[] = {
    {"jmevm/midp/AppProperties", "jad", NULL, app_jad},
    {"jmevm/midp/Resources", "read", NULL, resources_read},
    {"jmevm/midp/Boot", "exit", NULL, boot_exit},
};

void midp_jme_app_init(JmeVM *vm, const MidpSetup *setup)
{
    free(jad);
    jad = NULL;
    if (setup->jad_text) {
        size_t n = strlen(setup->jad_text) + 1;
        jad = malloc(n);
        if (jad)
            memcpy(jad, setup->jad_text, n);
    }
    resources = setup->resources;
    resources_user = setup->resources_user;
    jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
}
