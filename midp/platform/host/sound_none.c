/*
 * sound_none.c - no sound device on this platform.
 */
#include "runner.h"

bool sound_device_open(uint32_t rate)
{
    (void)rate;
    return false;
}

void sound_device_write(const int16_t frame[2])
{
    (void)frame;
}

void sound_device_close(void)
{
}
