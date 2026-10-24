/*
 * lcdui.c - natives of javax.microedition.lcdui Graphics, Image and Font,
 * and jmevm.midp.Screen, on jmevm.
 *
 * Image is { int width, height; int[] pixels (ARGB); boolean alpha; ... }
 * and Graphics keeps its state in plain fields (see the Java sources). The
 * drawing itself is src/midp/gfx.c and font.c. Pixel arrays live in the
 * Java heap, so snapshots contain them.
 */
#include "midp_jme.h"
#include "../midp/gfx.h"
#include "../pal/pal.h"
#include "../util/png.h"

#include <stdlib.h>
#include <string.h>

#define G "javax/microedition/lcdui/Graphics"
#define IMG "javax/microedition/lcdui/Image"
#define FONT "javax/microedition/lcdui/Font"

typedef struct {
    JmeField img_width, img_height, img_pixels, img_alpha;
    JmeField g_target, g_tx, g_ty, g_cx1, g_cy1, g_cx2, g_cy2, g_pixel, g_stroke;
    JmeField g_font, f_size, f_style;
} Fields;

static Fields F;
static bool   fields_ready;

static JmeField field(JmeVM *vm, const char *cls, const char *name, const char *desc)
{
    JmeField f = jme_field(vm, cls, name, desc);
    if (!f)
        pal_fatal("lcdui field missing in the class library");
    return f;
}

/* Field offsets depend only on the class library, so one lookup serves
 * every VM (and every snapshot) of the run. */
static void init_fields(JmeVM *vm)
{
    if (fields_ready)
        return;
    F.img_width = field(vm, IMG, "width", "I");
    F.img_height = field(vm, IMG, "height", "I");
    F.img_pixels = field(vm, IMG, "pixels", "[I");
    F.img_alpha = field(vm, IMG, "alpha", "Z");
    F.g_target = field(vm, G, "target", "Ljavax/microedition/lcdui/Image;");
    F.g_tx = field(vm, G, "tx", "I");
    F.g_ty = field(vm, G, "ty", "I");
    F.g_cx1 = field(vm, G, "cx1", "I");
    F.g_cy1 = field(vm, G, "cy1", "I");
    F.g_cx2 = field(vm, G, "cx2", "I");
    F.g_cy2 = field(vm, G, "cy2", "I");
    F.g_pixel = field(vm, G, "pixel", "I");
    F.g_stroke = field(vm, G, "stroke", "I");
    F.g_font = field(vm, G, "font", "Ljavax/microedition/lcdui/Font;");
    F.f_size = field(vm, FONT, "sizeIndex", "I");
    F.f_style = field(vm, FONT, "style", "I");
    fields_ready = true;
}

/* The pixels of an Image; valid until the next allocation. */
static bool image_surface(JmeVM *vm, JmeRef img, Surface *s)
{
    JmeRef px = img ? jme_get_ref(vm, img, F.img_pixels) : 0;
    if (!px)
        return false;
    s->px = jme_array_data(vm, px);
    s->alpha = jme_get_int(vm, img, F.img_alpha) != 0;
    s->w = jme_get_int(vm, img, F.img_width);
    s->h = jme_get_int(vm, img, F.img_height);
    return true;
}

/* The drawing context of a Graphics object. */
typedef struct {
    Surface  s;
    Clip     c;
    int      tx, ty;
    uint32_t color;
    bool     dotted;
} Ctx;

/* 1: draw, 0: nothing to draw (empty clip), -1: exception thrown */
static int ctx_of(JmeCall *call, Ctx *x)
{
    JmeVM *vm = jme_call_vm(call);
    init_fields(vm);
    JmeRef g = jme_arg_ref(call, 0);
    if (!image_surface(vm, jme_get_ref(vm, g, F.g_target), &x->s)) {
        midp_npe(call);
        return -1;
    }
    x->c.x1 = jme_get_int(vm, g, F.g_cx1);
    x->c.y1 = jme_get_int(vm, g, F.g_cy1);
    x->c.x2 = jme_get_int(vm, g, F.g_cx2);
    x->c.y2 = jme_get_int(vm, g, F.g_cy2);
    x->tx = jme_get_int(vm, g, F.g_tx);
    x->ty = jme_get_int(vm, g, F.g_ty);
    x->color = (uint32_t)jme_get_int(vm, g, F.g_pixel);
    x->dotted = jme_get_int(vm, g, F.g_stroke) != 0;
    return x->c.x1 < x->c.x2 && x->c.y1 < x->c.y2;
}

#define G_BEGIN() \
    Ctx x; \
    int ok_ = ctx_of(c, &x); \
    if (ok_ <= 0) \
        return ok_ < 0 ? JME_NATIVE_THROW : JME_NATIVE_RETURN
#define A(n) jme_arg_int(c, (n))

/* ---- Graphics ---- */

static int g_fill_rect(JmeCall *c)
{
    G_BEGIN();
    gfx_fill_rect(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), x.color);
    return JME_NATIVE_RETURN;
}

static int g_draw_rect(JmeCall *c)
{
    G_BEGIN();
    gfx_rect(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), x.color, x.dotted);
    return JME_NATIVE_RETURN;
}

static int g_draw_line(JmeCall *c)
{
    G_BEGIN();
    gfx_line(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3) + x.tx, A(4) + x.ty, x.color, x.dotted);
    return JME_NATIVE_RETURN;
}

static int g_draw_round_rect(JmeCall *c)
{
    G_BEGIN();
    gfx_round_rect(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), A(5), A(6), x.color, false);
    return JME_NATIVE_RETURN;
}

static int g_fill_round_rect(JmeCall *c)
{
    G_BEGIN();
    gfx_round_rect(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), A(5), A(6), x.color, true);
    return JME_NATIVE_RETURN;
}

static int g_draw_arc(JmeCall *c)
{
    G_BEGIN();
    gfx_arc(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), A(5), A(6), x.color, false);
    return JME_NATIVE_RETURN;
}

static int g_fill_arc(JmeCall *c)
{
    G_BEGIN();
    gfx_arc(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3), A(4), A(5), A(6), x.color, true);
    return JME_NATIVE_RETURN;
}

static int g_fill_triangle(JmeCall *c)
{
    G_BEGIN();
    gfx_fill_triangle(&x.s, &x.c, A(1) + x.tx, A(2) + x.ty, A(3) + x.tx, A(4) + x.ty, A(5) + x.tx,
                      A(6) + x.ty, x.color);
    return JME_NATIVE_RETURN;
}

static void font_of(JmeVM *vm, JmeRef g, int *size, int *style)
{
    JmeRef f = jme_get_ref(vm, g, F.g_font);
    *size = f ? jme_get_int(vm, f, F.f_size) : 1;
    *style = f ? jme_get_int(vm, f, F.f_style) : 0;
}

/* drawString0(String s, int off, int len, int x, int y): top-left */
static int g_draw_string0(JmeCall *c)
{
    G_BEGIN();
    JmeVM   *vm = jme_call_vm(c);
    JmeRef   s = jme_arg_ref(c, 1);
    uint32_t n;
    if (!s)
        return midp_npe(c);
    const uint16_t *chars = jme_string_chars(vm, s, &n);
    int             off = A(2), len = A(3), size, style;
    if (off < 0 || len < 0 || (uint32_t)off + (uint32_t)len > n)
        return midp_throw(c, "java/lang/StringIndexOutOfBoundsException", NULL);
    font_of(vm, jme_arg_ref(c, 0), &size, &style);
    font_draw(&x.s, &x.c, size, style, chars + off, len, A(4) + x.tx, A(5) + x.ty, x.color);
    return JME_NATIVE_RETURN;
}

/* drawChars0(char[] c, int off, int len, int x, int y): top-left */
static int g_draw_chars0(JmeCall *c)
{
    G_BEGIN();
    JmeVM *vm = jme_call_vm(c);
    JmeRef a = jme_arg_ref(c, 1);
    if (!a)
        return midp_npe(c);
    int off = A(2), len = A(3), size, style;
    if (off < 0 || len < 0 || (uint32_t)off + (uint32_t)len > jme_array_length(vm, a))
        return midp_throw(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    font_of(vm, jme_arg_ref(c, 0), &size, &style);
    font_draw(&x.s, &x.c, size, style, (const uint16_t *)jme_array_data(vm, a) + off, len, A(4) + x.tx,
              A(5) + x.ty, x.color);
    return JME_NATIVE_RETURN;
}

/* drawRegion0(Image src, int sx, int sy, int w, int h, int transform,
 *             int x, int y): destination top-left */
static int g_draw_region0(JmeCall *c)
{
    G_BEGIN();
    Surface src;
    if (!image_surface(jme_call_vm(c), jme_arg_ref(c, 1), &src))
        return midp_npe(c);
    int sx = A(2), sy = A(3), w = A(4), h = A(5);
    if (sx < 0 || sy < 0 || w < 0 || h < 0 || sx + w > src.w || sy + h > src.h)
        return midp_throw(c, "java/lang/IllegalArgumentException", "region exceeds image bounds");
    gfx_blit(&x.s, &x.c, &src, sx, sy, w, h, A(6), A(7) + x.tx, A(8) + x.ty, true);
    return JME_NATIVE_RETURN;
}

/* copyArea0(int sx, int sy, int w, int h, int dx, int dy) */
static int g_copy_area0(JmeCall *c)
{
    G_BEGIN();
    int sx = A(1) + x.tx, sy = A(2) + x.ty, w = A(3), h = A(4);
    if (sx < 0 || sy < 0 || w < 0 || h < 0 || sx + w > x.s.w || sy + h > x.s.h)
        return midp_throw(c, "java/lang/IllegalArgumentException", "copyArea out of bounds");
    gfx_blit(&x.s, &x.c, &x.s, sx, sy, w, h, TRANS_NONE, A(5) + x.tx, A(6) + x.ty, false);
    return JME_NATIVE_RETURN;
}

/* drawRGB0(int[] rgb, int offset, int scan, int x, int y, int w, int h,
 *          boolean alpha) */
static int g_draw_rgb0(JmeCall *c)
{
    G_BEGIN();
    JmeVM *vm = jme_call_vm(c);
    JmeRef rgb = jme_arg_ref(c, 1);
    int    off = A(2), scan = A(3), w = A(6), h = A(7);
    if (!rgb)
        return midp_npe(c);
    if (w <= 0 || h <= 0)
        return JME_NATIVE_RETURN;
    int64_t first = (int64_t)off + (scan < 0 ? (int64_t)(h - 1) * scan : 0);
    int64_t last = (int64_t)off + (scan < 0 ? 0 : (int64_t)(h - 1) * scan) + w - 1;
    if (first < 0 || last >= jme_array_length(vm, rgb))
        return midp_throw(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    gfx_draw_rgb(&x.s, &x.c, jme_array_data(vm, rgb), off, scan, A(4) + x.tx, A(5) + x.ty, w, h, A(8) != 0);
    return JME_NATIVE_RETURN;
}

/* ---- Image ---- */

/* Gives the Image in argument slot `slot` a w x h pixel array. Allocates:
 * read every other reference again afterwards. */
static bool alloc_pixels(JmeCall *c, unsigned slot, int w, int h, bool alpha)
{
    JmeVM *vm = jme_call_vm(c);
    init_fields(vm);
    if (w < 0 || h < 0 || (int64_t)w * h > INT32_MAX) {
        midp_throw(c, "java/lang/IllegalArgumentException", "image size");
        return false;
    }
    JmeRef px = jme_new_array(c, "[I", w * h);
    if (!px)
        return false;
    JmeRef img = jme_arg_ref(c, slot);
    jme_set_ref(vm, img, F.img_pixels, px);
    jme_set_int(vm, img, F.img_width, w);
    jme_set_int(vm, img, F.img_height, h);
    jme_set_int(vm, img, F.img_alpha, alpha);
    return true;
}

typedef struct {
    uint32_t *px;
    int       w;
    bool      translucent;
} DecodeCtx;

static void png_row(void *vctx, int y, int x0, int step, const uint32_t *argb, int n)
{
    DecodeCtx *d = vctx;
    for (int i = 0; i < n; i++) {
        uint32_t p = argb[i];
        d->px[y * d->w + x0 + i * step] = p;
        if ((p >> 24) != 255)
            d->translucent = true;
    }
}

/* static boolean decode(Image img, byte[] data, int off, int len) */
static int image_decode(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    JmeRef data = jme_arg_ref(c, 1);
    int    off = A(2), len = A(3);
    if (!data)
        return midp_npe(c);
    if (off < 0 || len < 0 || (uint32_t)off > jme_array_length(vm, data) - (uint32_t)len
        || (uint32_t)len > jme_array_length(vm, data))
        return midp_throw(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    PngInfo info;
    if (!png_info((const uint8_t *)jme_array_data(vm, data) + off, (uint32_t)len, &info))
        return JME_NATIVE_RETURN;
    if (!alloc_pixels(c, 0, info.width, info.height, false))
        return JME_NATIVE_THROW;
    /* The allocation may have moved both arrays: fetch them again. */
    JmeRef    img = jme_arg_ref(c, 0);
    DecodeCtx d;
    d.px = jme_array_data(vm, jme_get_ref(vm, img, F.img_pixels));
    d.w = info.width;
    d.translucent = false;
    const uint8_t *bytes = (const uint8_t *)jme_array_data(vm, jme_arg_ref(c, 1)) + off;
    if (!png_decode(bytes, (uint32_t)len, png_row, &d))
        return JME_NATIVE_RETURN;
    jme_set_int(vm, img, F.img_alpha, d.translucent);
    jme_return_int(c, 1);
    return JME_NATIVE_RETURN;
}

/* static void initRGB(Image img, int[] rgb, int w, int h, boolean alpha) */
static int image_init_rgb(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    int    w = A(2), h = A(3);
    bool   alpha = A(4) != 0;
    if (!jme_arg_ref(c, 1))
        return midp_npe(c);
    if (w < 0 || h < 0 || (int64_t)w * h > jme_array_length(vm, jme_arg_ref(c, 1)))
        return midp_throw(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    if (!alloc_pixels(c, 0, w, h, false))
        return JME_NATIVE_THROW;
    uint32_t       *px = jme_array_data(vm, jme_get_ref(vm, jme_arg_ref(c, 0), F.img_pixels));
    const uint32_t *src = jme_array_data(vm, jme_arg_ref(c, 1));
    bool            translucent = false;
    for (int i = 0; i < w * h; i++) {
        uint32_t p = alpha ? src[i] : src[i] | 0xFF000000u;
        px[i] = p;
        if ((p >> 24) != 255)
            translucent = true;
    }
    jme_set_int(vm, jme_arg_ref(c, 0), F.img_alpha, translucent);
    return JME_NATIVE_RETURN;
}

/* static void initBlank(Image img, int w, int h): white, opaque */
static int image_init_blank(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    int    w = A(1), h = A(2);
    if (!alloc_pixels(c, 0, w, h, false))
        return JME_NATIVE_THROW;
    uint32_t *px = jme_array_data(vm, jme_get_ref(vm, jme_arg_ref(c, 0), F.img_pixels));
    for (int i = 0; i < w * h; i++)
        px[i] = 0xFFFFFFFFu;
    return JME_NATIVE_RETURN;
}

/* static void initRegion(Image dst, Image src, int x, int y, int w, int h,
 *                        int transform) */
static int image_init_region(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    init_fields(vm);
    int x = A(2), y = A(3), w = A(4), h = A(5), tr = A(6);
    if (!jme_arg_ref(c, 1))
        return midp_npe(c);
    bool has_alpha = jme_get_int(vm, jme_arg_ref(c, 1), F.img_alpha) != 0;
    int  dw = tr >= 4 ? h : w, dh = tr >= 4 ? w : h;
    if (!alloc_pixels(c, 0, dw, dh, has_alpha))
        return JME_NATIVE_THROW;
    Surface src, out;
    if (!image_surface(vm, jme_arg_ref(c, 1), &src) || !image_surface(vm, jme_arg_ref(c, 0), &out))
        return midp_npe(c);
    if (x < 0 || y < 0 || w < 0 || h < 0 || x + w > src.w || y + h > src.h)
        return midp_throw(c, "java/lang/IllegalArgumentException", "region exceeds image bounds");
    Clip all = {0, 0, dw, dh};
    gfx_blit(&out, &all, &src, x, y, w, h, tr, 0, 0, false); /* alpha included */
    return JME_NATIVE_RETURN;
}

/* ---- Font: static natives taking (size, style, ...) ---- */

static int font_char_width0(JmeCall *c)
{
    jme_return_int(c, font_char_width(A(0), A(1), (uint16_t)A(2)));
    return JME_NATIVE_RETURN;
}

static int font_string_width0(JmeCall *c)
{
    JmeVM   *vm = jme_call_vm(c);
    JmeRef   s = jme_arg_ref(c, 2);
    uint32_t n;
    if (!s)
        return midp_npe(c);
    const uint16_t *chars = jme_string_chars(vm, s, &n);
    int             off = A(3), len = A(4);
    if (off < 0 || len < 0 || (uint32_t)off + (uint32_t)len > n)
        return midp_throw(c, "java/lang/StringIndexOutOfBoundsException", NULL);
    jme_return_int(c, font_chars_width(A(0), A(1), chars + off, len));
    return JME_NATIVE_RETURN;
}

static int font_chars_width0(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    JmeRef a = jme_arg_ref(c, 2);
    if (!a)
        return midp_npe(c);
    int off = A(3), len = A(4);
    if (off < 0 || len < 0 || (uint32_t)off + (uint32_t)len > jme_array_length(vm, a))
        return midp_throw(c, "java/lang/ArrayIndexOutOfBoundsException", NULL);
    jme_return_int(c, font_chars_width(A(0), A(1), (const uint16_t *)jme_array_data(vm, a) + off, len));
    return JME_NATIVE_RETURN;
}

static int font_height0(JmeCall *c)
{
    jme_return_int(c, font_height(A(0)));
    return JME_NATIVE_RETURN;
}

static int font_baseline0(JmeCall *c)
{
    jme_return_int(c, font_baseline(A(0)));
    return JME_NATIVE_RETURN;
}

/* ---- jmevm.midp.Screen ---- */

static int screen_width(JmeCall *c)
{
    int w, h;
    pal_screen_size(&w, &h);
    jme_return_int(c, w);
    return JME_NATIVE_RETURN;
}

static int screen_height(JmeCall *c)
{
    int w, h;
    pal_screen_size(&w, &h);
    jme_return_int(c, h);
    return JME_NATIVE_RETURN;
}

/* The last frame presented. It is shown by the host, not kept by the game,
 * so snapshots carry a copy and a loaded state shows it again. */
static uint32_t *shown;
static int       shown_w, shown_h;
static bool      reshow;

static bool keep_shown(const uint32_t *px, int w, int h)
{
    if ((int64_t)w * h != (int64_t)shown_w * shown_h || !shown) {
        uint32_t *p = realloc(shown, (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1) * 4);
        if (!p)
            return false;
        shown = p;
    }
    memcpy(shown, px, (size_t)w * (size_t)h * 4);
    shown_w = w;
    shown_h = h;
    return true;
}

/* static void present(Image img) */
static int screen_present(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    init_fields(vm);
    Surface s;
    if (!image_surface(vm, jme_arg_ref(c, 0), &s))
        return midp_npe(c);
    keep_shown(s.px, s.w, s.h);
    pal_present(s.px, s.w, s.h);
    return JME_NATIVE_RETURN;
}

static JmeStatus screen_save(void *user, JmeWriteFn write, void *write_user)
{
    (void)user;
    StateOut o = {write, write_user, JME_OK};
    state_put_u32(&o, (uint32_t)shown_w);
    state_put_u32(&o, (uint32_t)shown_h);
    if (shown)
        state_put(&o, shown, (size_t)shown_w * (size_t)shown_h * 4);
    return o.status;
}

static JmeStatus screen_load(void *user, const void *data, size_t len)
{
    (void)user;
    StateIn  in = {data, (const uint8_t *)data + len, false};
    uint32_t w = state_get_u32(&in), h = state_get_u32(&in);
    if (in.bad || w > 4096 || h > 4096 || (size_t)(in.end - in.p) != (size_t)w * h * 4)
        return JME_E_FORMAT;
    if (w * h == 0) {
        shown_w = shown_h = 0;
        return JME_OK;
    }
    if (!keep_shown((const uint32_t *)in.p, (int)w, (int)h))
        return JME_E_NOMEM;
    reshow = true;
    return JME_OK;
}

void midp_jme_screen_resync(void)
{
    if (reshow && shown && shown_w > 0)
        pal_show(shown, shown_w, shown_h);
    reshow = false;
}

/* static void softLabels(String left, String right) */
static int screen_soft_labels(JmeCall *c)
{
    JmeVM *vm = jme_call_vm(c);
    char  *l = jme_arg_ref(c, 0) ? jme_string_utf8(vm, jme_arg_ref(c, 0)) : NULL;
    char  *r = jme_arg_ref(c, 1) ? jme_string_utf8(vm, jme_arg_ref(c, 1)) : NULL;
    pal_soft_labels(l, r);
    free(l);
    free(r);
    return JME_NATIVE_RETURN;
}

static const JmeNative natives[] = {
    {G, "fillRect", NULL, g_fill_rect},
    {G, "drawRect", NULL, g_draw_rect},
    {G, "drawLine", NULL, g_draw_line},
    {G, "drawRoundRect", NULL, g_draw_round_rect},
    {G, "fillRoundRect", NULL, g_fill_round_rect},
    {G, "drawArc", NULL, g_draw_arc},
    {G, "fillArc", NULL, g_fill_arc},
    {G, "fillTriangle", NULL, g_fill_triangle},
    {G, "drawString0", NULL, g_draw_string0},
    {G, "drawChars0", NULL, g_draw_chars0},
    {G, "drawRegion0", NULL, g_draw_region0},
    {G, "copyArea0", NULL, g_copy_area0},
    {G, "drawRGB0", NULL, g_draw_rgb0},
    {IMG, "decode", NULL, image_decode},
    {IMG, "initRGB", NULL, image_init_rgb},
    {IMG, "initBlank", NULL, image_init_blank},
    {IMG, "initRegion", NULL, image_init_region},
    {FONT, "charWidth0", NULL, font_char_width0},
    {FONT, "stringWidth0", NULL, font_string_width0},
    {FONT, "charsWidth0", NULL, font_chars_width0},
    {FONT, "height0", NULL, font_height0},
    {FONT, "baseline0", NULL, font_baseline0},
    {"jmevm/midp/Screen", "width", NULL, screen_width},
    {"jmevm/midp/Screen", "height", NULL, screen_height},
    {"jmevm/midp/Screen", "present", NULL, screen_present},
    {"jmevm/midp/Screen", "softLabels", NULL, screen_soft_labels},
};

void midp_jme_lcdui_init(JmeVM *vm)
{
    fields_ready = false;
    free(shown);
    shown = NULL;
    shown_w = shown_h = 0;
    reshow = false;
    jme_register_natives(vm, natives, sizeof natives / sizeof natives[0]);
    jme_register_state(vm, "screen", screen_save, screen_load, NULL);
}
