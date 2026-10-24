/*
 * events.c - the input event queue between the platform and the Java event
 * thread (jmevm.midp.Events). Events.next() blocks on a host channel until
 * input arrives; the queue is part of the snapshot.
 */
#include "midp_jme.h"
#include "../pal/pal.h"

#define QUEUE_SIZE 64
#define CHANNEL 1

static PalEvent queue[QUEUE_SIZE];
static int     head, tail;
static bool    wakeup;

static bool queue_put(const PalEvent *ev)
{
    int next = (tail + 1) % QUEUE_SIZE;
    if (next == head)
        return false;
    queue[tail] = *ev;
    tail = next;
    return true;
}

static bool queue_get(PalEvent *ev)
{
    if (head == tail)
        return false;
    *ev = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    return true;
}

bool midp_jme_poll(JmeVM *vm)
{
    PalEvent ev;
    bool    any = false;
    /* after a snapshot load: the loaded screen and sound */
    midp_jme_screen_resync();
    midp_jme_audio_resync(vm);
    while (pal_poll_event(&ev)) {
        if (ev.type == EV_QUIT)
            return false;
        if (queue_put(&ev))
            any = true;
    }
    if (any)
        jme_signal(vm, CHANNEL);
    return true;
}

/* static int next(int[] out, int timeoutMs): the event type with key code
 * (or pointer x, y) in out[0..1], or 0 on timeout or wakeup(). timeoutMs < 0
 * waits forever, 0 polls. */
static int events_next(JmeCall *c)
{
    JmeVM  *vm = jme_call_vm(c);
    JmeRef  out = jme_arg_ref(c, 0);
    int32_t timeout = jme_arg_int(c, 1);
    PalEvent ev;
    if (!out || jme_array_length(vm, out) < 2)
        return midp_npe(c);
    if (queue_get(&ev)) {
        int32_t *o = jme_array_data(vm, out);
        o[0] = ev.a;
        o[1] = ev.b;
        jme_return_int(c, ev.type);
        return JME_NATIVE_RETURN;
    }
    if (jme_call_woken(c) || timeout == 0 || wakeup) {
        wakeup = false;
        jme_return_int(c, 0);
        return JME_NATIVE_RETURN;
    }
    return jme_block(c, CHANNEL, timeout < 0 ? -1 : timeout);
}

/* static void wakeup(): makes a pending or future next() return 0. */
static int events_wakeup(JmeCall *c)
{
    wakeup = true;
    jme_signal(jme_call_vm(c), CHANNEL);
    return JME_NATIVE_RETURN;
}

static const JmeNative natives[] = {
    {"jmevm/midp/Events", "next", NULL, events_next},
    {"jmevm/midp/Events", "wakeup", NULL, events_wakeup},
};

/* State: wakeup, count, then (type, a, b) per queued event. */
static JmeStatus save(void *user, JmeWriteFn write, void *write_user)
{
    (void)user;
    StateOut o = {write, write_user, JME_OK};
    state_put_u32(&o, wakeup);
    state_put_u32(&o, (uint32_t)((tail - head + QUEUE_SIZE) % QUEUE_SIZE));
    for (int i = head; i != tail; i = (i + 1) % QUEUE_SIZE) {
        state_put_u32(&o, (uint32_t)queue[i].type);
        state_put_u32(&o, (uint32_t)queue[i].a);
        state_put_u32(&o, (uint32_t)queue[i].b);
    }
    return o.status;
}

static JmeStatus load(void *user, const void *data, size_t len)
{
    (void)user;
    StateIn  in = {data, (const uint8_t *)data + len, false};
    uint32_t w = state_get_u32(&in), n = state_get_u32(&in);
    if (in.bad || n >= QUEUE_SIZE || (size_t)(in.end - in.p) != (size_t)n * 12)
        return JME_E_FORMAT;
    head = tail = 0;
    wakeup = w != 0;
    for (uint32_t i = 0; i < n; i++) {
        PalEvent ev;
        ev.type = (int)state_get_u32(&in);
        ev.a = (int)state_get_u32(&in);
        ev.b = (int)state_get_u32(&in);
        queue_put(&ev);
    }
    return JME_OK;
}

void midp_jme_events_init(JmeVM *vm)
{
    head = tail = 0;
    wakeup = false;
    jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
    jme_register_state(vm, "events", save, load, NULL);
}
