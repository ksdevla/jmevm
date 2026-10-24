/*
 * window_win32.c - a Win32 window that shows the screen, scaled, and turns
 * keys and the mouse into MIDP input (through the input queue, so --record
 * captures it):
 *
 *   arrows        up, down, left, right
 *   Enter, Space  fire
 *   F1, F2        left and right soft keys
 *   0-9           number keys (top row or keypad); keypad * and / are * and #
 *   mouse         touches
 *   F5, F9        save and load the state (DATA/quick.jms)
 *   Backspace     rewind while held
 *   Esc           quit
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "runner.h"

static HWND window;
static int  width, height, scale;
static bool closed, dragging;

static int midp_key(WPARAM vk)
{
    if (vk >= '0' && vk <= '9')
        return (int)vk;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return '0' + (int)(vk - VK_NUMPAD0);
    switch (vk) {
    case VK_UP:       return -1;
    case VK_DOWN:     return -2;
    case VK_LEFT:     return -3;
    case VK_RIGHT:    return -4;
    case VK_RETURN:
    case VK_SPACE:    return -5;
    case VK_F1:       return -6;
    case VK_F2:       return -7;
    case VK_MULTIPLY: return '*';
    case VK_DIVIDE:   return '#';
    default:          return 0;
    }
}

static void paint(HDC dc)
{
    int             w, h;
    const uint32_t *px = last_frame(&w, &h);
    if (!px)
        return;
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   /* top-down rows */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, w * scale, h * scale, 0, 0, w, h, px, &bi, DIB_RGB_COLORS, SRCCOPY);
}

static void touch(int type, LPARAM lp)
{
    PalEvent ev = {type, (short)LOWORD(lp) / scale, (short)HIWORD(lp) / scale};
    if (ev.a < 0) ev.a = 0;
    if (ev.b < 0) ev.b = 0;
    if (ev.a >= width) ev.a = width - 1;
    if (ev.b >= height) ev.b = height - 1;
    input_push(&ev);
}

static LRESULT CALLBACK handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        closed = true;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC         dc = BeginPaint(hwnd, &ps);
        paint(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_KEYUP: {
        bool down = msg == WM_KEYDOWN;
        if (down && (lp & (1 << 30)))
            return 0;   /* auto-repeat */
        if (wp == VK_ESCAPE) {
            closed = closed || down;
            return 0;
        }
        if (wp == VK_F5 || wp == VK_F9) {
            if (down)
                *(wp == VK_F5 ? &runner.save_request : &runner.load_request) = true;
            return 0;
        }
        if (wp == VK_BACK) {
            runner.rewinding = down;
            return 0;
        }
        int key = midp_key(wp);
        if (key) {
            PalEvent ev = {down ? EV_KEY_DOWN : EV_KEY_UP, key, 0};
            input_push(&ev);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        dragging = true;
        touch(EV_POINTER_DOWN, lp);
        return 0;
    case WM_MOUSEMOVE:
        if (dragging)
            touch(EV_POINTER_DRAG, lp);
        return 0;
    case WM_LBUTTONUP:
        if (dragging) {
            dragging = false;
            ReleaseCapture();
            touch(EV_POINTER_UP, lp);
        }
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

bool window_open(int w, int h, int s)
{
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = handle;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "jmevm-midp";
    RegisterClassA(&wc);
    width = w;
    height = h;
    scale = s < 1 ? 1 : s;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT  r = {0, 0, w * scale, h * scale};
    AdjustWindowRect(&r, style, FALSE);
    window = CreateWindowA("jmevm-midp", "jmevm", style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (!window)
        return false;
    ShowWindow(window, SW_SHOW);
    return true;
}

bool window_pump(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return !closed;
}

void window_show(void)
{
    HDC dc = GetDC(window);
    paint(dc);
    ReleaseDC(window, dc);
}
