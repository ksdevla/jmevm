/*
 * input.c - all input goes through one queue: keys from the window, the
 * --keys script and a --replay file. The queue is filled before each frame
 * and emptied by the MIDP layer's polls during it. --record writes each
 * event with its frame, so --replay delivers it to the same frame again.
 */
#include "runner.h"
#include "engine.h"

#include <stdlib.h>
#include <string.h>

#define QUEUE 256

typedef struct Timed {
    int64_t  at;   /* emulated ms (script) or frame (replay) */
    PalEvent ev;
} Timed;

typedef struct List {
    Timed *items;
    int    n, cap, next;
} List;

static PalEvent queue[QUEUE];
static int      head, tail;
static uint64_t frame_now;
static List     script, replay;
static FILE    *record;

static bool append(List *l, int64_t at, int type, int a, int b)
{
    if (l->n == l->cap) {
        int    cap = l->cap ? l->cap * 2 : 64;
        Timed *grown = realloc(l->items, sizeof(Timed) * (size_t)cap);
        if (!grown)
            return false;
        l->items = grown;
        l->cap = cap;
    }
    Timed *t = &l->items[l->n++];
    t->at = at;
    t->ev.type = type;
    t->ev.a = a;
    t->ev.b = b;
    return true;
}

void input_push(const PalEvent *ev)
{
    int next = (tail + 1) % QUEUE;
    if (next == head)
        return;   /* full: dropped, as a phone's key buffer would */
    queue[tail] = *ev;
    tail = next;
    if (record)
        fprintf(record, "%llu %d %d %d\n", (unsigned long long)frame_now, ev->type, ev->a, ev->b);
}

bool pal_poll_event(PalEvent *ev)
{
    if (head == tail)
        return false;
    *ev = queue[head];
    head = (head + 1) % QUEUE;
    return true;
}

static bool expect(const char **s, const char *word)
{
    size_t n = strlen(word);
    if (strncmp(*s, word, n) != 0)
        return false;
    *s += n;
    return true;
}

bool input_script(const char *s)
{
    while (*s) {
        char   *end;
        int64_t at = strtoll(s, &end, 10);
        if (end == s || *end != ':')
            return false;
        s = end + 1;
        int  type, a, b = 0;
        if (*s == '@') {   /* a touch */
            a = (int)strtol(s + 1, &end, 10);
            if (*end != ':')
                return false;
            b = (int)strtol(end + 1, &end, 10);
            s = end;
            type = expect(&s, ":drag") ? EV_POINTER_DRAG : expect(&s, ":up") ? EV_POINTER_UP : EV_POINTER_DOWN;
        } else {
            a = (int)strtol(s, &end, 10);
            if (end == s)
                return false;
            s = end;
            type = expect(&s, ":up") ? EV_KEY_UP : EV_KEY_DOWN;
        }
        if (!append(&script, at, type, a, b))
            return false;
        if (*s == ',')
            s++;
        else if (*s)
            return false;
    }
    return true;
}

void input_begin_frame(uint64_t frame)
{
    frame_now = frame;
    int64_t now = engine_time_ms();
    while (script.next < script.n && script.items[script.next].at <= now)
        input_push(&script.items[script.next++].ev);
    while (replay.next < replay.n && replay.items[replay.next].at <= (int64_t)frame)
        input_push(&replay.items[replay.next++].ev);
}

void input_reset(void)
{
    head = tail = 0;
    frame_now = 0;
    script.next = replay.next = 0;
}

void input_seek(uint64_t frame, int64_t until_ms)
{
    head = tail = 0;
    frame_now = frame;
    for (script.next = 0; script.next < script.n && script.items[script.next].at <= until_ms; script.next++)
        ;
    for (replay.next = 0; replay.next < replay.n && replay.items[replay.next].at <= (int64_t)frame; replay.next++)
        ;
}

bool input_record(const char *path)
{
    record = fopen(path, "w");
    return record != NULL;
}

/* A --record file: one "frame type a b" line per event. */
bool input_replay(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    unsigned long long frame;
    int                type, a, b;
    bool               ok = true;
    while (ok && fscanf(f, "%llu %d %d %d", &frame, &type, &a, &b) == 4)
        ok = append(&replay, (int64_t)frame, type, a, b);
    fclose(f);
    return ok;
}

void input_close(void)
{
    if (record)
        fclose(record);
    record = NULL;
}
