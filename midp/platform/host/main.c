/*
 * main.c - jmevm-midp, the desktop runner: runs a MIDlet (or a class's
 * main) on jmevm, headless or in a window, with scripted or recorded
 * input, frame logs, screenshots, sound output, save states and rewind.
 *
 * The program runs one display frame (1/60 s of emulated time) at a time.
 * Frame f ends at an exact tick and input enters only between frames, so
 * the same program, start time and input always give the same run.
 */
#include "runner.h"
#include "engine.h"

#include <stdlib.h>
#include <string.h>

#define FPS          60
#define REWIND_EVERY 6    /* frames between rewind history entries */
#define REWIND_TEST  60   /* frames --rewind-at holds rewind for */

typedef struct Options {
    EngineOptions engine;
    EngineApp     app;
    int64_t       run_ms;        /* stop after this much emulated time; -1: never */
    int           runs;          /* start the program this many times */
    bool          realtime;
    int           window;        /* scale; 0: no window */
    const char   *screenshot;
    int64_t       shot_every;    /* also numbered screenshots this often */
    const char   *audio_out;
    const char   *load_state;
    int64_t       save_at;       /* --save-state-at: when, and where to */
    const char   *save_state;
    int64_t       rewind_at;
    bool          dump_threads;
} Options;

static Options opt;

static void usage(void)
{
    fprintf(stderr,
        "usage: jmevm-midp [options] game.jar|game.jad [MIDletClass]\n"
        "       jmevm-midp [options] -cp <dir|jar> -main <Class> [args...]\n"
        "options:\n"
        "  --classlib PATH    class library jar (default: $JMEVM_MIDP_CLASSLIB,\n"
        "                     or classlib.jar next to this program)\n"
        "  --heap MB          Java heap size (default 16)\n"
        "  --screen WxH       screen size (default 240x320)\n"
        "  --data DIR         record store directory (default ./rms)\n"
        "  --window [SCALE]   play in a window SCALE times the screen (default 2):\n"
        "                     arrows, Enter = fire, F1/F2 = soft keys, 0-9, keypad\n"
        "                     * and /, the mouse for touches; F5 saves the state,\n"
        "                     F9 loads it (DATA/quick.jms), hold Backspace to\n"
        "                     rewind, Esc quits. Implies --realtime.\n"
        "  --realtime         run at real speed (default: as fast as possible)\n"
        "  --run-ms N         stop after N emulated milliseconds\n"
        "  --runs N           start the program N times (testing)\n"
        "  --epoch MS         System.currentTimeMillis() at the start (default: now)\n"
        "  --ticks-per-ms N   emulated CPU speed (default 10000)\n"
        "  --keys SCRIPT      scripted input in emulated milliseconds: ms:key[:up]\n"
        "                     (MIDP key codes) and ms:@x:y[:drag|:up] (touches)\n"
        "  --record FILE      save all input, for --replay\n"
        "  --replay FILE      feed input saved by --record\n"
        "  --frame-log FILE   write \"ms hash\" for every new frame\n"
        "  --screenshot FILE  save the last frame as a PPM image at the end\n"
        "  --shot-every MS    also FILE-0.ppm, FILE-1.ppm, ... every MS ms\n"
        "  --audio-out FILE   record the sound as a WAV file\n"
        "  --load-state FILE  start from a saved state\n"
        "  --save-state-at MS:FILE  save the state once MS ms have passed\n"
        "  --rewind-at MS     rewind for 60 frames from MS (testing)\n"
        "  --gc-stress        collect before every allocation (testing)\n"
        "  --dump-threads     print every thread's stack at the end\n"
        "  --trace-audio      log what the sound natives do\n");
    exit(2);
}

static void fail(const char *what, const char *path)
{
    fprintf(stderr, "jmevm-midp: %s %s\n", what, path);
    exit(2);
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* Reads a .jad; returns the jar it names in MIDlet-Jar-URL (looked for next
 * to the .jad), or else the .jad's own name with .jar. */
static const char *open_jad(const char *path)
{
    static char text[65536], jar[1024];
    FILE       *f = fopen(path, "rb");
    if (!f)
        fail("cannot open", path);
    text[fread(text, 1, sizeof text - 1, f)] = 0;
    fclose(f);
    opt.app.jad_text = text;

    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    if (back > slash)
        slash = back;
    int         dir = slash ? (int)(slash - path + 1) : 0;
    const char *url = strstr(text, "MIDlet-Jar-URL:");
    if (url) {
        url += strlen("MIDlet-Jar-URL:");
        url += strspn(url, " \t");
        int         len = (int)strcspn(url, "\r\n");
        const char *name = url;
        for (int i = 0; i < len; i++)
            if (url[i] == '/')
                name = url + i + 1;
        snprintf(jar, sizeof jar, "%.*s%.*s", dir, path, (int)(url + len - name), name);
        FILE *j = fopen(jar, "rb");
        if (j) {
            fclose(j);
            return jar;
        }
    }
    snprintf(jar, sizeof jar, "%.*s.jar", (int)strlen(path) - 4, path);
    return jar;
}

static void parse(int argc, char **argv)
{
    opt.engine.heap_bytes = 16u << 20;
    opt.engine.epoch_ms = epoch_ms();
    opt.run_ms = -1;
    opt.runs = 1;
    opt.shot_every = -1;
    opt.save_at = -1;
    opt.rewind_at = -1;
    runner.screen_w = 240;
    runner.screen_h = 320;
    runner.data_dir = "rms";

    static char beside[1024];   /* classlib.jar next to this program */
    const char *self = argv[0], *sep = NULL;
    for (const char *p = self; *p; p++)
        if (*p == '/' || *p == '\\')
            sep = p + 1;
    snprintf(beside, sizeof beside, "%.*sclasslib.jar", sep ? (int)(sep - self) : 0, self);
    opt.app.classlib = getenv("JMEVM_MIDP_CLASSLIB") ? getenv("JMEVM_MIDP_CLASSLIB") : beside;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(a, "-main") == 0 && v) {
            opt.app.main_class = v;
            opt.app.argc = argc - i - 2;
            opt.app.argv = argv + i + 2;
            break;
        }
        if (a[0] != '-') {
            if (!opt.app.jar)
                opt.app.jar = a;
            else if (!opt.app.midlet)
                opt.app.midlet = a;
            else
                usage();
            continue;
        }
        /* Options without a value. */
        if (strcmp(a, "--realtime") == 0) {
            opt.realtime = true;
        } else if (strcmp(a, "--window") == 0) {
            opt.window = 2;
            opt.realtime = true;
            if (v && v[0] >= '1' && v[0] <= '9')
                opt.window = atoi(argv[++i]);
        } else if (strcmp(a, "--gc-stress") == 0) {
            opt.engine.gc_stress = true;
        } else if (strcmp(a, "--dump-threads") == 0) {
            opt.dump_threads = true;
        } else if (strcmp(a, "--trace-audio") == 0) {
            opt.engine.trace_audio = true;
        } else if (strcmp(a, "--trace-classes") == 0) {
            opt.engine.trace_classes = true;
        } else if (strcmp(a, "--trace-exceptions") == 0) {
            opt.engine.trace_exceptions = true;
        } else if (!v) {
            usage();
        } else {
            /* Options with a value. */
            i++;
            if (strcmp(a, "--classlib") == 0) {
                opt.app.classlib = v;
            } else if (strcmp(a, "-cp") == 0) {
                opt.app.cp = v;
            } else if (strcmp(a, "--heap") == 0) {
                opt.engine.heap_bytes = (uint32_t)atoi(v) << 20;
            } else if (strcmp(a, "--screen") == 0) {
                if (sscanf(v, "%dx%d", &runner.screen_w, &runner.screen_h) != 2)
                    usage();
            } else if (strcmp(a, "--data") == 0) {
                runner.data_dir = v;
            } else if (strcmp(a, "--run-ms") == 0) {
                opt.run_ms = strtoll(v, NULL, 10);
            } else if (strcmp(a, "--runs") == 0) {
                opt.runs = atoi(v);
            } else if (strcmp(a, "--epoch") == 0) {
                opt.engine.epoch_ms = strtoll(v, NULL, 10);
            } else if (strcmp(a, "--ticks-per-ms") == 0) {
                opt.engine.ticks_per_ms = (uint32_t)strtoul(v, NULL, 10);
            } else if (strcmp(a, "--keys") == 0) {
                if (!input_script(v))
                    usage();
            } else if (strcmp(a, "--record") == 0) {
                if (!input_record(v))
                    fail("cannot write", v);
            } else if (strcmp(a, "--replay") == 0) {
                if (!input_replay(v))
                    fail("cannot read", v);
            } else if (strcmp(a, "--frame-log") == 0) {
                if (!(runner.frame_log = fopen(v, "w")))
                    fail("cannot write", v);
            } else if (strcmp(a, "--screenshot") == 0) {
                opt.screenshot = v;
            } else if (strcmp(a, "--shot-every") == 0) {
                opt.shot_every = strtoll(v, NULL, 10);
            } else if (strcmp(a, "--audio-out") == 0) {
                opt.audio_out = v;
            } else if (strcmp(a, "--load-state") == 0) {
                opt.load_state = v;
            } else if (strcmp(a, "--save-state-at") == 0) {
                char *end;
                opt.save_at = strtoll(v, &end, 10);
                if (*end != ':' || !end[1])
                    usage();
                opt.save_state = end + 1;
            } else if (strcmp(a, "--rewind-at") == 0) {
                opt.rewind_at = strtoll(v, NULL, 10);
            } else {
                usage();
            }
        }
    }
    if (!opt.app.main_class && !opt.app.jar)
        usage();
    if (opt.app.jar && ends_with(opt.app.jar, ".jad"))
        opt.app.jar = open_jad(opt.app.jar);
}

/* ---- the run ---- */

static int64_t next_shot;
static int     shot_number, rewind_frames;

/* The tick at which frame f ends. */
static uint64_t frame_end(uint64_t f)
{
    return f * engine_ticks_per_ms() * 1000 / FPS;
}

/* After a state was loaded: how many frames it had done (states are saved
 * between frames), with the input moved to match. */
static uint64_t frames_after_load(void)
{
    uint64_t per_second = (uint64_t)engine_ticks_per_ms() * 1000;
    uint64_t done = (engine_ticks() * FPS + per_second - 1) / per_second;
    /* frame f gets the scripted input due by the end of frame f - 1 */
    input_seek(done, done > 0 ? (int64_t)(frame_end(done - 1) / engine_ticks_per_ms()) : -1);
    return done;
}

static const char *quick_state(void)
{
    static char path[1100];
    snprintf(path, sizeof path, "%s/quick.jms", runner.data_dir);
    return path;
}

/* Between frames: sound, screenshots, and whether to go on. */
static bool between_frames(void)
{
    sound_update();
    if (opt.shot_every > 0 && opt.screenshot && engine_time_ms() >= next_shot) {
        char path[1100];
        snprintf(path, sizeof path, "%.*s-%d.ppm", (int)strcspn(opt.screenshot, "."), opt.screenshot,
                 shot_number++);
        write_screenshot(path);
        next_shot += opt.shot_every;
    }
    return opt.run_ms < 0 || engine_time_ms() < opt.run_ms;
}

/* Runs the program once: 1 if it ended by itself, 0 if stopped (--run-ms,
 * window closed), -1 if it could not start. */
static int run_once(void)
{
    if (!engine_start(&opt.engine, &opt.app))
        return -1;
    input_reset();
    next_shot = opt.shot_every;
    shot_number = 0;
    uint64_t frame = 0;   /* frames done */
    if (opt.load_state) {
        if (!engine_load_state(opt.load_state))
            return -1;
        frame = frames_after_load();
    }
    bool    history = opt.window || opt.rewind_at >= 0;
    int64_t start = wall_ms();
    for (uint64_t shown = 1;; shown++) {
        if (opt.window && !window_pump())
            return 0;
        /* --rewind-at holds rewind once, for REWIND_TEST frames. Time goes
         * back meanwhile, so only the start looks at the clock. */
        if (opt.rewind_at >= 0 && rewind_frames < REWIND_TEST
            && (rewind_frames > 0 || engine_time_ms() >= opt.rewind_at))
            runner.rewinding = ++rewind_frames < REWIND_TEST;
        if (runner.load_request) {
            runner.load_request = false;
            if (engine_load_state(quick_state()))
                frame = frames_after_load();
        }
        if (runner.rewinding && history) {
            /* One history entry back per frame shown, then one frame without
             * input, so the screen shows that moment. */
            if (engine_rewind_back()) {
                frame = frames_after_load() + 1;
                if (!engine_run_until(frame_end(frame)))
                    return 1;
            }
        } else {
            frame++;
            input_begin_frame(frame);
            if (!engine_run_until(frame_end(frame)))
                return 1;
            if (opt.save_at >= 0 && engine_time_ms() >= opt.save_at) {
                opt.save_at = -1;
                if (!engine_save_state(opt.save_state))
                    fprintf(stderr, "jmevm-midp: cannot save the state to %s\n", opt.save_state);
            }
            if (runner.save_request) {
                runner.save_request = false;
                if (!engine_save_state(quick_state()))
                    fprintf(stderr, "jmevm-midp: cannot save the state to %s\n", quick_state());
            }
            if (history && frame % REWIND_EVERY == 0)
                engine_rewind_push();
        }
        if (opt.window)
            window_show();
        if (!between_frames())
            return 0;
        if (opt.realtime)
            sleep_until(start + (int64_t)(shown * 1000 / FPS));
    }
}

int main(int argc, char **argv)
{
    parse(argc, argv);
    if (opt.audio_out && !sound_record(opt.audio_out))
        fail("cannot write", opt.audio_out);
    if (opt.window && !window_open(runner.screen_w, runner.screen_h, opt.window)) {
        fprintf(stderr, "jmevm-midp: cannot open a window on this platform\n");
        return 2;
    }
    if (opt.window && !sound_play())
        fprintf(stderr, "jmevm-midp: no sound device, playing silently\n");

    int code = 0;
    for (int run = 0; run < opt.runs; run++) {
        if (run > 0)
            engine_stop();
        int result = run_once();
        code = result < 0 ? 1 : engine_exit_code();
    }
    if (opt.dump_threads)
        engine_dump_threads();
    if (getenv("JMEVM_MIDP_STATS"))
        fprintf(stderr, "jmevm-midp: %d frames, %lld emulated ms in %lld real ms, %lld ms waiting\n",
                runner.frames_presented, (long long)engine_time_ms(), (long long)wall_ms(),
                (long long)runner.idle_ms);
    if (runner.frame_log)
        fclose(runner.frame_log);
    input_close();
    sound_close();
    if (opt.screenshot && !write_screenshot(opt.screenshot))
        fprintf(stderr, "jmevm-midp: no frame to save\n");
    return code;
}
