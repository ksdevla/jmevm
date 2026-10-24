/*
 * gfx.c - shapes and images on 0xAARRGGBB surfaces.
 *
 * Everything clips to the intersection of the clip rectangle and the
 * surface. Curves are decided per pixel with exact integer tests in
 * doubled coordinates (so pixel centres are integers too), and arc angles
 * with a fixed-point sine table: no floating point, no libm.
 */
#include "gfx.h"

#include <stdlib.h>
#include <string.h>

/* sin(d) * 65536 for whole degrees 0..90. */
static const int32_t sine[91] = {
    0, 1144, 2287, 3430, 4572, 5712, 6850, 7987, 9121, 10252, 11380, 12505,
    13626, 14742, 15855, 16962, 18064, 19161, 20252, 21336, 22415, 23486, 24550, 25607,
    26656, 27697, 28729, 29753, 30767, 31772, 32768, 33754, 34729, 35693, 36647, 37590,
    38521, 39441, 40348, 41243, 42126, 42995, 43852, 44695, 45525, 46341, 47143, 47930,
    48703, 49461, 50203, 50931, 51643, 52339, 53020, 53684, 54332, 54963, 55578, 56175,
    56756, 57319, 57865, 58393, 58903, 59396, 59870, 60326, 60764, 61183, 61584, 61966,
    62328, 62672, 62997, 63303, 63589, 63856, 64104, 64332, 64540, 64729, 64898, 65048,
    65177, 65287, 65376, 65446, 65496, 65526, 65536,
};

/* sin and cos of whole degrees, any angle, * 65536. */
static int32_t sin_deg(int d)
{
    d %= 360;
    if (d < 0)
        d += 360;
    if (d <= 90)
        return sine[d];
    if (d <= 180)
        return sine[180 - d];
    if (d <= 270)
        return -sine[d - 180];
    return -sine[360 - d];
}

static int32_t cos_deg(int d)
{
    return sin_deg(d + 90);
}

/* The drawable area: clip and surface together. False if it is empty. */
static bool area(const Surface *s, const Clip *c, Clip *out)
{
    out->x1 = c->x1 > 0 ? c->x1 : 0;
    out->y1 = c->y1 > 0 ? c->y1 : 0;
    out->x2 = c->x2 < s->w ? c->x2 : s->w;
    out->y2 = c->y2 < s->h ? c->y2 : s->h;
    return out->x1 < out->x2 && out->y1 < out->y2;
}

static inline void plot(Surface *s, const Clip *a, int x, int y, uint32_t color)
{
    if (x >= a->x1 && x < a->x2 && y >= a->y1 && y < a->y2)
        s->px[(size_t)y * s->w + x] = color;
}

/* Pixels x1..x2 (inclusive) of row y. */
static void span(Surface *s, const Clip *a, int y, int x1, int x2, uint32_t color)
{
    if (y < a->y1 || y >= a->y2)
        return;
    if (x1 < a->x1)
        x1 = a->x1;
    if (x2 >= a->x2)
        x2 = a->x2 - 1;
    uint32_t *p = s->px + (size_t)y * s->w;
    for (int x = x1; x <= x2; x++)
        p[x] = color;
}

void gfx_fill_rect(Surface *s, const Clip *c, int x, int y, int w, int h, uint32_t color)
{
    Clip a;
    if (w <= 0 || h <= 0 || !area(s, c, &a))
        return;
    for (int row = y; row < y + h; row++)
        span(s, &a, row, x, x + w - 1, color);
}

/* ---- lines ---- */

static void line_in(Surface *s, const Clip *a, int x1, int y1, int x2, int y2, uint32_t color,
                    bool dotted)
{
    /* Bresenham; DOTTED draws two pixels, then skips two. */
    int dx = abs(x2 - x1), dy = -abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for (int step = 0;; step++) {
        if (!dotted || (step & 2) == 0)
            plot(s, a, x1, y1, color);
        if (x1 == x2 && y1 == y2)
            return;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void gfx_line(Surface *s, const Clip *c, int x1, int y1, int x2, int y2, uint32_t color, bool dotted)
{
    Clip a;
    if (area(s, c, &a))
        line_in(s, &a, x1, y1, x2, y2, color, dotted);
}

void gfx_rect(Surface *s, const Clip *c, int x, int y, int w, int h, uint32_t color, bool dotted)
{
    Clip a;
    if (w < 0 || h < 0 || !area(s, c, &a))
        return;
    if (w == 0 || h == 0) {
        line_in(s, &a, x, y, x + w, y + h, color, dotted);
        return;
    }
    line_in(s, &a, x, y, x + w, y, color, dotted);
    line_in(s, &a, x, y + h, x + w, y + h, color, dotted);
    if (h > 1) {
        line_in(s, &a, x, y + 1, x, y + h - 1, color, dotted);
        line_in(s, &a, x + w, y + 1, x + w, y + h - 1, color, dotted);
    }
}

/* ---- triangles and polygons ---- */

/* a / b rounded to the nearest integer (halves away from zero); b > 0. */
static int div_round(int64_t a, int64_t b)
{
    return (int)(a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b));
}

void gfx_fill_triangle(Surface *s, const Clip *c, int x1, int y1, int x2, int y2, int x3, int y3,
                       uint32_t color)
{
    Clip a;
    if (!area(s, c, &a))
        return;
    int xs[3] = {x1, x2, x3}, ys[3] = {y1, y2, y3};
    int top = y1 < y2 ? (y1 < y3 ? y1 : y3) : (y2 < y3 ? y2 : y3);
    int bottom = y1 > y2 ? (y1 > y3 ? y1 : y3) : (y2 > y3 ? y2 : y3);
    if (top < a.y1)
        top = a.y1;
    if (bottom >= a.y2)
        bottom = a.y2 - 1;
    /* A triangle is convex: each row is one span, from the leftmost to the
     * rightmost point of its edges on that row (edges included). */
    for (int y = top; y <= bottom; y++) {
        int lo = 0x7FFFFFFF, hi = -0x7FFFFFFF;
        for (int e = 0; e < 3; e++) {
            int xa = xs[e], ya = ys[e], xb = xs[(e + 1) % 3], yb = ys[(e + 1) % 3];
            if (y < (ya < yb ? ya : yb) || y > (ya > yb ? ya : yb))
                continue;
            int p, q;
            if (ya == yb) {
                p = xa;
                q = xb;
            } else {
                p = q = xa + div_round((int64_t)(y - ya) * (xb - xa), yb - ya);
            }
            if (p > q) {
                int t = p;
                p = q;
                q = t;
            }
            if (p < lo)
                lo = p;
            if (q > hi)
                hi = q;
        }
        if (lo <= hi)
            span(s, &a, y, lo, hi, color);
    }
}

static int compare_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return x < y ? -1 : x > y;
}

void gfx_fill_polygon(Surface *s, const Clip *c, const int *xs, const int *ys, int n, uint32_t color)
{
    Clip a;
    if (n < 3 || !area(s, c, &a))
        return;
    int top = ys[0], bottom = ys[0];
    for (int i = 1; i < n; i++) {
        top = ys[i] < top ? ys[i] : top;
        bottom = ys[i] > bottom ? ys[i] : bottom;
    }
    top = top < a.y1 ? a.y1 : top;
    bottom = bottom >= a.y2 ? a.y2 - 1 : bottom;
    int *cross = malloc(sizeof(int) * (size_t)n);
    if (!cross)
        return;
    /* Even-odd: on each row, the spans between pairs of edge crossings.
     * An edge counts from its upper end to just above its lower end, so a
     * shared vertex is crossed once. */
    for (int y = top; y <= bottom; y++) {
        int k = 0;
        for (int i = 0; i < n; i++) {
            int xa = xs[i], ya = ys[i], xb = xs[(i + 1) % n], yb = ys[(i + 1) % n];
            if (ya == yb || y < (ya < yb ? ya : yb) || y >= (ya > yb ? ya : yb))
                continue;
            cross[k++] = xa + div_round((int64_t)(y - ya) * (xb - xa), yb - ya);
        }
        qsort(cross, (size_t)k, sizeof(int), compare_int);
        for (int i = 0; i + 1 < k; i += 2)
            span(s, &a, y, cross[i], cross[i + 1], color);
    }
    free(cross);
    for (int i = 0; i < n; i++)
        line_in(s, &a, xs[i], ys[i], xs[(i + 1) % n], ys[(i + 1) % n], color, false);
}

/* ---- arcs and rounded rectangles ---- */

/* An angular range; full when it is a whole turn. */
typedef struct Sector {
    bool    full;
    bool    wide;     /* more than half a turn */
    int64_t sx, sy;   /* unit vectors (* 65536) of its start and end */
    int64_t ex, ey;
} Sector;

static Sector sector(int start, int arc)
{
    Sector r = {0};
    if (arc >= 360 || arc <= -360) {
        r.full = true;
        return r;
    }
    if (arc < 0) {   /* clockwise: the same range, counter-clockwise from its end */
        start += arc;
        arc = -arc;
    }
    r.wide = arc > 180;
    r.sx = cos_deg(start);
    r.sy = sin_deg(start);
    r.ex = cos_deg(start + arc);
    r.ey = sin_deg(start + arc);
    return r;
}

/* Whether direction (u, v) (v up) lies in the sector. */
static bool in_sector(const Sector *r, int64_t u, int64_t v)
{
    if (r->full || (u == 0 && v == 0))
        return true;
    int64_t after_start = r->sx * v - r->sy * u;   /* > 0: counter-clockwise of start */
    int64_t before_end = u * r->ey - v * r->ex;    /* > 0: clockwise of end */
    if (!r->wide)
        return after_start >= 0 && before_end >= 0;
    return !(after_start < 0 && before_end < 0);
}

/* Ellipse in doubled coordinates: centre (cx, cy), half-axes rx, ry. */
typedef struct Ellipse {
    int64_t cx, cy, rx, ry;
} Ellipse;

static bool in_ellipse(const Ellipse *e, int64_t px, int64_t py)
{
    int64_t dx = px - e->cx, dy = py - e->cy;
    if (e->rx == 0 || e->ry == 0)
        return (e->rx == 0 ? dx == 0 && dy * dy <= e->ry * e->ry : dy == 0 && dx * dx <= e->rx * e->rx);
    return dx * dx * e->ry * e->ry + dy * dy * e->rx * e->rx <= e->rx * e->rx * e->ry * e->ry;
}

void gfx_arc(Surface *s, const Clip *c, int x, int y, int w, int h, int start, int arc,
             uint32_t color, bool fill)
{
    Clip a;
    if (w < 0 || h < 0 || arc == 0 || !area(s, c, &a) || (fill && (w == 0 || h == 0)))
        return;
    Sector r = sector(start, arc);
    /* Doubled coordinates: a filled arc covers pixels x..x+w-1, whose
     * centres are 2i+1; an outline goes through the centres of x..x+w. */
    int     ox = fill ? 1 : 0;
    Ellipse e = {2 * (int64_t)x + w, 2 * (int64_t)y + h, w, h};
    int     x1 = x, y1 = y, x2 = fill ? x + w - 1 : x + w, y2 = fill ? y + h - 1 : y + h;
    if (x1 < a.x1) x1 = a.x1;
    if (y1 < a.y1) y1 = a.y1;
    if (x2 >= a.x2) x2 = a.x2 - 1;
    if (y2 >= a.y2) y2 = a.y2 - 1;
    for (int j = y1; j <= y2; j++) {
        int64_t py = 2 * (int64_t)j + ox;
        for (int i = x1; i <= x2; i++) {
            int64_t px = 2 * (int64_t)i + ox;
            if (!in_ellipse(&e, px, py))
                continue;
            /* An outline keeps the pixels with a neighbour outside. */
            if (!fill && in_ellipse(&e, px - 2, py) && in_ellipse(&e, px + 2, py)
                && in_ellipse(&e, px, py - 2) && in_ellipse(&e, px, py + 2))
                continue;
            /* The angle in the box's own proportions (u, v up). */
            if (in_sector(&r, (px - e.cx) * (e.ry ? e.ry : 1), -(py - e.cy) * (e.rx ? e.rx : 1)))
                s->px[(size_t)j * s->w + i] = color;
        }
    }
}

/* Whether doubled point (px, py) is inside a rounded rectangle from
 * (x1, y1) to (x2, y2) (doubled) with corner half-axes rx, ry. */
static bool in_round_rect(int64_t x1, int64_t y1, int64_t x2, int64_t y2, int64_t rx, int64_t ry,
                          int64_t px, int64_t py)
{
    if (px < x1 || px > x2 || py < y1 || py > y2)
        return false;
    int64_t dx = px < x1 + rx ? px - (x1 + rx) : px > x2 - rx ? px - (x2 - rx) : 0;
    int64_t dy = py < y1 + ry ? py - (y1 + ry) : py > y2 - ry ? py - (y2 - ry) : 0;
    if (dx == 0 || dy == 0)
        return true;
    return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
}

void gfx_round_rect(Surface *s, const Clip *c, int x, int y, int w, int h, int aw, int ah,
                    uint32_t color, bool fill)
{
    Clip a;
    if (w < 0 || h < 0 || !area(s, c, &a) || (fill && (w == 0 || h == 0)))
        return;
    aw = abs(aw) < w ? abs(aw) : w;
    ah = abs(ah) < h ? abs(ah) : h;
    if (aw == 0 || ah == 0) {
        if (fill)
            gfx_fill_rect(s, c, x, y, w, h, color);
        else
            gfx_rect(s, c, x, y, w, h, color, false);
        return;
    }
    /* As for arcs: filled pixel centres at 2i+1 inside the edges 2x and
     * 2(x+w); outline centres at 2i on the edges 2x and 2(x+w). */
    int     ox = fill ? 1 : 0;
    int64_t ex1 = 2 * (int64_t)x, ey1 = 2 * (int64_t)y, ex2 = ex1 + 2 * (int64_t)w, ey2 = ey1 + 2 * (int64_t)h;
    int     x1 = x, y1 = y, x2 = fill ? x + w - 1 : x + w, y2 = fill ? y + h - 1 : y + h;
    if (x1 < a.x1) x1 = a.x1;
    if (y1 < a.y1) y1 = a.y1;
    if (x2 >= a.x2) x2 = a.x2 - 1;
    if (y2 >= a.y2) y2 = a.y2 - 1;
    for (int j = y1; j <= y2; j++) {
        int64_t py = 2 * (int64_t)j + ox;
        for (int i = x1; i <= x2; i++) {
            int64_t px = 2 * (int64_t)i + ox;
            if (!in_round_rect(ex1, ey1, ex2, ey2, aw, ah, px, py))
                continue;
            if (!fill && in_round_rect(ex1, ey1, ex2, ey2, aw, ah, px - 2, py)
                && in_round_rect(ex1, ey1, ex2, ey2, aw, ah, px + 2, py)
                && in_round_rect(ex1, ey1, ex2, ey2, aw, ah, px, py - 2)
                && in_round_rect(ex1, ey1, ex2, ey2, aw, ah, px, py + 2))
                continue;
            s->px[(size_t)j * s->w + i] = color;
        }
    }
}

/* ---- images ---- */

/* The source pixel (within a w x h region) shown at (x, y) of the
 * transformed region. */
static void source_of(int transform, int x, int y, int w, int h, int *sx, int *sy)
{
    switch (transform) {
    case TRANS_MIRROR:        *sx = w - 1 - x; *sy = y;         break;
    case TRANS_MIRROR_ROT180: *sx = x;         *sy = h - 1 - y; break;
    case TRANS_ROT180:        *sx = w - 1 - x; *sy = h - 1 - y; break;
    case TRANS_ROT90:         *sx = y;         *sy = h - 1 - x; break;
    case TRANS_ROT270:        *sx = w - 1 - y; *sy = x;         break;
    case TRANS_MIRROR_ROT90:  *sx = w - 1 - y; *sy = h - 1 - x; break;
    case TRANS_MIRROR_ROT270: *sx = y;         *sy = x;         break;
    default:                  *sx = x;         *sy = y;         break;
    }
}

static inline void put(uint32_t *d, uint32_t p, bool blend)
{
    if (!blend)
        *d = p;
    else if ((p >> 24) == 0xFF)
        *d = p;
    else if (p >> 24)
        *d = gfx_blend(*d, p, p >> 24);
}

void gfx_blit(Surface *dst, const Clip *c, const Surface *src, int sx, int sy, int w, int h,
              int transform, int dx, int dy, bool blend)
{
    Clip a;
    if (w <= 0 || h <= 0 || !area(dst, c, &a))
        return;
    blend = blend && src->alpha;
    int tw = transform >= 4 ? h : w, th = transform >= 4 ? w : h;   /* 4..7 turn by 90 */
    int x1 = dx > a.x1 ? dx : a.x1, y1 = dy > a.y1 ? dy : a.y1;
    int x2 = dx + tw < a.x2 ? dx + tw : a.x2, y2 = dy + th < a.y2 ? dy + th : a.y2;
    if (x1 >= x2 || y1 >= y2)
        return;

    /* Copying within one surface: read from a copy of the region. */
    const uint32_t *from = src->px;
    int             stride = src->w, ox = sx, oy = sy;
    uint32_t       *copy = NULL;
    if (src->px == dst->px) {
        copy = malloc(sizeof(uint32_t) * (size_t)w * h);
        if (!copy)
            return;
        for (int row = 0; row < h; row++)
            memcpy(copy + (size_t)row * w, src->px + (size_t)(sy + row) * src->w + sx, sizeof(uint32_t) * w);
        from = copy;
        stride = w;
        ox = oy = 0;
    }

    if (transform == TRANS_NONE && !blend) {
        for (int y = y1; y < y2; y++)
            memcpy(dst->px + (size_t)y * dst->w + x1,
                   from + (size_t)(oy + y - dy) * stride + ox + (x1 - dx), sizeof(uint32_t) * (x2 - x1));
    } else {
        for (int y = y1; y < y2; y++) {
            uint32_t *d = dst->px + (size_t)y * dst->w;
            for (int x = x1; x < x2; x++) {
                int px, py;
                source_of(transform, x - dx, y - dy, w, h, &px, &py);
                put(&d[x], from[(size_t)(oy + py) * stride + ox + px], blend);
            }
        }
    }
    free(copy);
}

void gfx_draw_rgb(Surface *dst, const Clip *c, const int32_t *rgb, int offset, int scan,
                  int x, int y, int w, int h, bool alpha)
{
    Clip a;
    if (w <= 0 || h <= 0 || !area(dst, c, &a))
        return;
    int x1 = x > a.x1 ? x : a.x1, y1 = y > a.y1 ? y : a.y1;
    int x2 = x + w < a.x2 ? x + w : a.x2, y2 = y + h < a.y2 ? y + h : a.y2;
    for (int row = y1; row < y2; row++) {
        const int32_t *p = rgb + offset + (int64_t)(row - y) * scan + (x1 - x);
        uint32_t      *d = dst->px + (size_t)row * dst->w;
        for (int col = x1; col < x2; col++, p++) {
            uint32_t v = (uint32_t)*p;
            put(&d[col], alpha ? v : v | 0xFF000000u, true);
        }
    }
}
