/*
 * sound.c - the mixer's output, made as the emulated clock advances: to a
 * WAV file (--audio-out) and, with a window, to the sound device.
 */
#include "runner.h"
#include "engine.h"
#include "../../src/audio/mixer.h"

#include <string.h>

static FILE    *wav;
static uint32_t wav_frames;
static bool     device;
static bool     started;

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* A 16-bit stereo PCM header for the frames written so far. */
static void wav_header(void)
{
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                     16, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0,
                     'd', 'a', 't', 'a', 0, 0, 0, 0};
    put32(h + 4, 36 + wav_frames * 4);
    put32(h + 24, MIXER_RATE);
    put32(h + 28, MIXER_RATE * 4);
    put32(h + 40, wav_frames * 4);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, wav);
    fseek(wav, 0, SEEK_END);
}

static void out(const int16_t frame[2], void *user)
{
    (void)user;
    if (wav) {
        uint8_t b[4] = {(uint8_t)frame[0], (uint8_t)(frame[0] >> 8), (uint8_t)frame[1], (uint8_t)(frame[1] >> 8)};
        fwrite(b, 1, 4, wav);
        wav_frames++;
    }
    if (device)
        sound_device_write(frame);
}

bool sound_record(const char *path)
{
    wav = fopen(path, "wb");
    if (wav)
        wav_header();
    return wav != NULL;
}

bool sound_play(void)
{
    device = sound_device_open(MIXER_RATE);
    return device;
}

void sound_update(void)
{
    if (!started) {
        mixer_reset(engine_time_ms());
        started = true;
    }
    mixer_advance(engine_time_ms(), out, NULL);
}

void sound_close(void)
{
    if (device)
        sound_device_close();
    device = false;
    if (wav) {
        wav_header();
        fclose(wav);
        wav = NULL;
    }
}
