/*
 * midp_jme.c - setup of the MIDP natives on jmevm, and helpers.
 */
#include "midp_jme.h"
#include "../pal/pal.h"

#include <string.h>

static const char *const properties[][2] = {
    {"microedition.platform", "jmevm"},
    {"microedition.configuration", "CLDC-1.1"},
    {"microedition.profiles", "MIDP-2.0"},
    {"microedition.encoding", "ISO-8859-1"},
    {"microedition.locale", "en-US"},
    {"microedition.commports", ""},
    {"microedition.hostname", "localhost"},
    {"microedition.media.version", "1.1"},
    {"supports.mixing", "false"},
    {"supports.audio.capture", "false"},
    {"supports.video.capture", "false"},
    {"supports.recording", "false"},
};

bool midp_jme_init(JmeVM *vm, const MidpSetup *setup)
{
    for (size_t i = 0; i < sizeof properties / sizeof properties[0]; i++)
        if (jme_set_property(vm, properties[i][0], properties[i][1]) != JME_OK)
            return false;
    jme_set_resource_source(vm, setup->resources, setup->resources_user);
    midp_jme_events_init(vm);
    midp_jme_lcdui_init(vm);
    if (!midp_jme_storage_init(vm))
        return false;
    midp_jme_audio_init(vm, setup->trace_audio);
    midp_jme_app_init(vm, setup);
    return true;
}

void midp_jme_shutdown(void)
{
    midp_jme_audio_shutdown();
    midp_jme_storage_shutdown();
}

int midp_npe(JmeCall *c)
{
    return jme_throw(c, "java/lang/NullPointerException", NULL);
}

int midp_throw(JmeCall *c, const char *cls, const char *msg)
{
    return jme_throw(c, cls, msg);
}

void state_put(StateOut *o, const void *data, size_t n)
{
    if (o->status == JME_OK && n > 0)
        o->status = o->write(o->user, data, n);
}

void state_put_u32(StateOut *o, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    state_put(o, b, 4);
}

const uint8_t *state_get(StateIn *in, size_t n)
{
    if (in->bad || (size_t)(in->end - in->p) < n) {
        in->bad = true;
        return NULL;
    }
    const uint8_t *p = in->p;
    in->p += n;
    return p;
}

uint32_t state_get_u32(StateIn *in)
{
    const uint8_t *b = state_get(in, 4);
    return b ? (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24 : 0;
}
