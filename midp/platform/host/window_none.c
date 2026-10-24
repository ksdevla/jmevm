/*
 * window_none.c - no window on this platform: the runner works headless.
 */
#include "runner.h"

bool window_open(int w, int h, int scale)
{
    (void)w;
    (void)h;
    (void)scale;
    return false;
}

bool window_pump(void)
{
    return false;
}

void window_show(void)
{
}
