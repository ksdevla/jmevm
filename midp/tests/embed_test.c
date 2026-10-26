/*
 * embed_test.c - checks the library interface (platform/embed) the way an
 * app uses it: the game runs in 16 ms steps with keys between them, frames
 * and sound come out, and a state saved part-way and loaded again gives
 * the same frames as the run that went on.
 *
 * embed_test CLASSLIB JAR DATA_DIR STATE_FILE
 */
#include "../platform/embed/jmevm_midp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEP_MS 16

static void console(void *user, const char *text, int len)
{
    (void)user;
    fwrite(text, 1, (size_t)len, stdout);
}

static uint32_t hash_frame(void)
{
    int      w, h;
    uint32_t serial;
    const uint32_t *px = j2me_frame(&w, &h, &serial);
    uint32_t hash = 2166136261u;
    for (int i = 0; px && i < w * h; i++)
        hash = (hash ^ px[i]) * 16777619u;
    return hash;
}

/* Runs steps steps, pressing a key now and then; returns the hash of all
 * frames shown, and counts them and the sound frames. */
static uint32_t run(int from, int steps, int *frames, long *sound)
{
    static const int keys[] = {-1, -2, -5, 53};
    uint32_t all = 0, last_serial = 0;
    int16_t  buf[1024 * 2];
    for (int i = from; i < from + steps; i++) {
        if (i % 40 == 10)
            j2me_key(true, keys[i / 40 % 4]);
        if (i % 40 == 14)
            j2me_key(false, keys[i / 40 % 4]);
        if (!j2me_run(STEP_MS))
            break;
        int      w, h;
        uint32_t serial;
        if (j2me_frame(&w, &h, &serial) && serial != last_serial) {
            last_serial = serial;
            all = all * 31 + hash_frame();
            (*frames)++;
        }
        int n;
        while ((n = j2me_audio_read(buf, 1024)) > 0)
            *sound += n;
    }
    return all;
}

static bool start(const char **argv)
{
    J2meConfig c = {0};
    c.classlib = argv[1];
    c.jar = argv[2];
    c.data_dir = argv[3];
    c.screen_w = 240;
    c.screen_h = 320;
    c.history_mb = 64;
    c.console = console;
    return j2me_start(&c);
}

int main(int argc, const char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: embed_test CLASSLIB JAR DATA_DIR STATE_FILE\n");
        return 2;
    }
    if (!start(argv)) {
        fprintf(stderr, "embed_test: cannot start\n");
        return 1;
    }
    int  frames = 0;
    long sound = 0;
    run(0, 80, &frames, &sound);
    if (!j2me_save_state(argv[4])) {
        fprintf(stderr, "embed_test: cannot save\n");
        return 1;
    }
    int64_t saved_at = j2me_time_ms();
    int     after = 0;
    uint32_t a = run(80, 200, &after, &sound);
    int64_t end_a = j2me_time_ms();

    /* A new game, then the state: everything after it must repeat. */
    j2me_stop();
    if (!start(argv) || !j2me_load_state(argv[4])) {
        fprintf(stderr, "embed_test: cannot load\n");
        return 1;
    }
    if (j2me_time_ms() != saved_at) {
        fprintf(stderr, "embed_test: loaded at %lld ms, saved at %lld\n",
                (long long)j2me_time_ms(), (long long)saved_at);
        return 1;
    }
    int      after_b = 0;
    uint32_t b = run(80, 200, &after_b, &sound);
    if (a != b || after != after_b || j2me_time_ms() != end_a) {
        fprintf(stderr, "embed_test: the loaded state ran differently (%d/%d frames)\n", after,
                after_b);
        return 1;
    }

    /* Rewind goes back to an earlier time. */
    int64_t pushed_at[5];
    for (int i = 0; i < 5; i++) {
        pushed_at[i] = j2me_time_ms();
        j2me_rewind_push();
        run(280 + i * 10, 10, &after_b, &sound);
    }
    int64_t before = j2me_time_ms();
    if (j2me_rewind_count() != 5 || !j2me_rewind_back() || j2me_time_ms() != pushed_at[3] ||
        j2me_time_ms() >= before) {
        fprintf(stderr, "embed_test: rewind did not go back\n");
        return 1;
    }
    /* Several entries at once; the newer ones are dropped. */
    if (j2me_rewind_to(4) || !j2me_rewind_to(2) || j2me_time_ms() != pushed_at[1] ||
        j2me_rewind_count() != 2) {
        fprintf(stderr, "embed_test: rewind_to went wrong\n");
        return 1;
    }
    /* The every-frame history goes back one recorded frame at a time. */
    int64_t recorded_at[6];
    for (int i = 0; i < 6; i++) {
        run(300 + i * 17, 17, &after_b, &sound);
        recorded_at[i] = j2me_time_ms();
        if (!j2me_history_record()) {
            fprintf(stderr, "embed_test: history recording failed\n");
            return 1;
        }
    }
    if (j2me_history_count() != 6) {
        fprintf(stderr, "embed_test: the history holds %u frames, not 6\n", j2me_history_count());
        return 1;
    }
    for (int i = 4; i >= 0; i--) {
        if (!j2me_history_back() || j2me_time_ms() != recorded_at[i]) {
            fprintf(stderr, "embed_test: history step back to frame %d went wrong\n", i);
            return 1;
        }
    }
    /* After a load both histories are gone: they belonged to another timeline. */
    if (!j2me_load_state(argv[4]) || j2me_rewind_back() || j2me_history_back()) {
        fprintf(stderr, "embed_test: rewind history survived a load\n");
        return 1;
    }
    j2me_stop();
    printf("embed_test: %d frames before the save, %d after, %ld sound frames\n", frames, after,
           sound);
    if (frames + after == 0 || sound == 0) {
        fprintf(stderr, "embed_test: no frames or no sound\n");
        return 1;
    }
    return 0;
}
