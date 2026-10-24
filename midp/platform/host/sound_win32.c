/*
 * sound_win32.c - the sound device on Windows (waveOut): a ring of
 * buffers, each handed to the device when full. The mix runs ahead of the
 * device by a few buffers, which absorbs the difference of the clocks.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "runner.h"

#define BUFFERS 8
#define FRAMES  1024   /* stereo frames per buffer, 31 ms */

static HWAVEOUT out;
static WAVEHDR  headers[BUFFERS];
static int16_t  buffers[BUFFERS][FRAMES * 2];
static int      current, used;

bool sound_device_open(uint32_t rate)
{
    WAVEFORMATEX fmt = {0};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = rate * 4;
    if (waveOutOpen(&out, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return false;
    for (int i = 0; i < BUFFERS; i++) {
        ZeroMemory(&headers[i], sizeof headers[i]);
        headers[i].lpData = (LPSTR)buffers[i];
        headers[i].dwBufferLength = sizeof buffers[i];
        headers[i].dwFlags = WHDR_DONE;
    }
    current = used = 0;
    return true;
}

void sound_device_write(const int16_t frame[2])
{
    WAVEHDR *h = &headers[current];
    if (used == 0 && !(h->dwFlags & WHDR_DONE))
        return;   /* every buffer is still queued: the mix is too far ahead, drop */
    buffers[current][used * 2] = frame[0];
    buffers[current][used * 2 + 1] = frame[1];
    if (++used < FRAMES)
        return;
    if (h->dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(out, h, sizeof *h);
    h->dwFlags = 0;
    waveOutPrepareHeader(out, h, sizeof *h);
    waveOutWrite(out, h, sizeof *h);
    current = (current + 1) % BUFFERS;
    used = 0;
}

void sound_device_close(void)
{
    waveOutReset(out);
    for (int i = 0; i < BUFFERS; i++)
        if (headers[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(out, &headers[i], sizeof headers[i]);
    waveOutClose(out);
}
