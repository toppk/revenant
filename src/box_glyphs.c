#include "box_glyphs.h"

#include "utf8.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
        ARM_NONE = 0,
        ARM_LIGHT = 1,
        ARM_HEAVY = 2,
        ARM_DOUBLE = 3
};

#define ARMS(left, right, up, down) (uint8_t)(((left) << 6) | ((right) << 4) | ((up) << 2) | (down))

/* Arm styles for U+2500..U+257F; dashes, arcs and diagonals are zero and handled separately. */
static const uint8_t box_arms[128] = {
    ARMS(1, 1, 0, 0),
    ARMS(2, 2, 0, 0),
    ARMS(0, 0, 1, 1),
    ARMS(0, 0, 2, 2), /* 2500 */
    0,
    0,
    0,
    0, /* 2504 dashes */
    0,
    0,
    0,
    0, /* 2508 dashes */
    ARMS(0, 1, 0, 1),
    ARMS(0, 2, 0, 1),
    ARMS(0, 1, 0, 2),
    ARMS(0, 2, 0, 2), /* 250C */
    ARMS(1, 0, 0, 1),
    ARMS(2, 0, 0, 1),
    ARMS(1, 0, 0, 2),
    ARMS(2, 0, 0, 2), /* 2510 */
    ARMS(0, 1, 1, 0),
    ARMS(0, 2, 1, 0),
    ARMS(0, 1, 2, 0),
    ARMS(0, 2, 2, 0), /* 2514 */
    ARMS(1, 0, 1, 0),
    ARMS(2, 0, 1, 0),
    ARMS(1, 0, 2, 0),
    ARMS(2, 0, 2, 0), /* 2518 */
    ARMS(0, 1, 1, 1),
    ARMS(0, 2, 1, 1),
    ARMS(0, 1, 2, 1),
    ARMS(0, 1, 1, 2), /* 251C */
    ARMS(0, 1, 2, 2),
    ARMS(0, 2, 2, 1),
    ARMS(0, 2, 1, 2),
    ARMS(0, 2, 2, 2), /* 2520 */
    ARMS(1, 0, 1, 1),
    ARMS(2, 0, 1, 1),
    ARMS(1, 0, 2, 1),
    ARMS(1, 0, 1, 2), /* 2524 */
    ARMS(1, 0, 2, 2),
    ARMS(2, 0, 2, 1),
    ARMS(2, 0, 1, 2),
    ARMS(2, 0, 2, 2), /* 2528 */
    ARMS(1, 1, 0, 1),
    ARMS(2, 1, 0, 1),
    ARMS(1, 2, 0, 1),
    ARMS(2, 2, 0, 1), /* 252C */
    ARMS(1, 1, 0, 2),
    ARMS(2, 1, 0, 2),
    ARMS(1, 2, 0, 2),
    ARMS(2, 2, 0, 2), /* 2530 */
    ARMS(1, 1, 1, 0),
    ARMS(2, 1, 1, 0),
    ARMS(1, 2, 1, 0),
    ARMS(2, 2, 1, 0), /* 2534 */
    ARMS(1, 1, 2, 0),
    ARMS(2, 1, 2, 0),
    ARMS(1, 2, 2, 0),
    ARMS(2, 2, 2, 0), /* 2538 */
    ARMS(1, 1, 1, 1),
    ARMS(2, 1, 1, 1),
    ARMS(1, 2, 1, 1),
    ARMS(2, 2, 1, 1), /* 253C */
    ARMS(1, 1, 2, 1),
    ARMS(1, 1, 1, 2),
    ARMS(1, 1, 2, 2),
    ARMS(2, 1, 2, 1), /* 2540 */
    ARMS(1, 2, 2, 1),
    ARMS(2, 1, 1, 2),
    ARMS(1, 2, 1, 2),
    ARMS(2, 2, 2, 1), /* 2544 */
    ARMS(2, 2, 1, 2),
    ARMS(2, 1, 2, 2),
    ARMS(1, 2, 2, 2),
    ARMS(2, 2, 2, 2), /* 2548 */
    0,
    0,
    0,
    0, /* 254C dashes */
    ARMS(3, 3, 0, 0),
    ARMS(0, 0, 3, 3),
    ARMS(0, 3, 0, 1),
    ARMS(0, 1, 0, 3), /* 2550 */
    ARMS(0, 3, 0, 3),
    ARMS(3, 0, 0, 1),
    ARMS(1, 0, 0, 3),
    ARMS(3, 0, 0, 3), /* 2554 */
    ARMS(0, 3, 1, 0),
    ARMS(0, 1, 3, 0),
    ARMS(0, 3, 3, 0),
    ARMS(3, 0, 1, 0), /* 2558 */
    ARMS(1, 0, 3, 0),
    ARMS(3, 0, 3, 0),
    ARMS(0, 3, 1, 1),
    ARMS(0, 1, 3, 3), /* 255C */
    ARMS(0, 3, 3, 3),
    ARMS(3, 0, 1, 1),
    ARMS(1, 0, 3, 3),
    ARMS(3, 0, 3, 3), /* 2560 */
    ARMS(3, 3, 0, 1),
    ARMS(1, 1, 0, 3),
    ARMS(3, 3, 0, 3),
    ARMS(3, 3, 1, 0), /* 2564 */
    ARMS(1, 1, 3, 0),
    ARMS(3, 3, 3, 0),
    ARMS(3, 3, 1, 1),
    ARMS(1, 1, 3, 3), /* 2568 */
    ARMS(3, 3, 3, 3),
    0,
    0,
    0, /* 256C, arcs */
    0,
    0,
    0,
    0, /* 2570 arc, diagonals */
    ARMS(1, 0, 0, 0),
    ARMS(0, 0, 1, 0),
    ARMS(0, 1, 0, 0),
    ARMS(0, 0, 0, 1), /* 2574 */
    ARMS(2, 0, 0, 0),
    ARMS(0, 0, 2, 0),
    ARMS(0, 2, 0, 0),
    ARMS(0, 0, 0, 2), /* 2578 */
    ARMS(1, 2, 0, 0),
    ARMS(0, 0, 1, 2),
    ARMS(2, 1, 0, 0),
    ARMS(0, 0, 2, 1), /* 257C */
};

enum
{
        QUAD_UL = 1,
        QUAD_UR = 2,
        QUAD_LL = 4,
        QUAD_LR = 8
};

static const uint8_t quadrants[10] = {
    QUAD_LL,                     /* 2596 */
    QUAD_LR,                     /* 2597 */
    QUAD_UL,                     /* 2598 */
    QUAD_UL | QUAD_LL | QUAD_LR, /* 2599 */
    QUAD_UL | QUAD_LR,           /* 259A */
    QUAD_UL | QUAD_UR | QUAD_LL, /* 259B */
    QUAD_UL | QUAD_UR | QUAD_LR, /* 259C */
    QUAD_UR,                     /* 259D */
    QUAD_UR | QUAD_LL,           /* 259E */
    QUAD_UR | QUAD_LL | QUAD_LR, /* 259F */
};

static XtpBoxGlyphRealloc box_realloc = realloc;

void
XtpBoxGlyphSetAllocator(XtpBoxGlyphRealloc allocator)
{
        box_realloc = allocator != NULL ? allocator : realloc;
}

bool
XtpBoxGlyphCodepoint(uint32_t codepoint)
{
        return (codepoint >= 0x2500U && codepoint <= 0x259FU) ||
               (codepoint >= 0x2800U && codepoint <= 0x28FFU) ||
               (codepoint >= 0xE0B0U && codepoint <= 0xE0BFU);
}

bool
XtpBoxGlyphText(const char *text, size_t length, uint32_t *codepoint_out)
{
        uint32_t codepoint;
        size_t consumed;

        if (text == NULL || length == 0 || !XtpUtf8Decode(text, length, &codepoint, &consumed) ||
            consumed != length || !XtpBoxGlyphCodepoint(codepoint))
                return false;
        if (codepoint_out != NULL)
                *codepoint_out = codepoint;
        return true;
}

unsigned int
XtpBoxGlyphThickness(unsigned int width, unsigned int height, bool bold)
{
        unsigned int thickness = height / 16U;

        if (thickness > width / 8U)
                thickness = width / 8U;
        if (thickness == 0)
                thickness = 1;
        return bold ? thickness + 1U : thickness;
}

static void
AddRect(XtpBoxGlyph *glyph, unsigned int x, unsigned int y, unsigned int width, unsigned int height,
        unsigned int cell_width, unsigned int cell_height)
{
        XtpBoxRect *rect;

        if (glyph->failed || x >= cell_width || y >= cell_height || width == 0 || height == 0)
                return;
        if (glyph->count == glyph->capacity) {
                size_t capacity = glyph->capacity == 0 ? 16U : glyph->capacity * 2U;
                XtpBoxRect *grown = box_realloc(glyph->rects, capacity * sizeof(*grown));

                if (grown == NULL) {
                        glyph->failed = true;
                        return;
                }
                glyph->rects = grown;
                glyph->capacity = capacity;
        }
        if (width > cell_width - x)
                width = cell_width - x;
        if (height > cell_height - y)
                height = cell_height - y;
        rect = &glyph->rects[glyph->count++];
        rect->x = x;
        rect->y = y;
        rect->width = width;
        rect->height = height;
}

static void
Band(unsigned int size, unsigned int thickness, unsigned int *start, unsigned int *end)
{
        if (thickness > size)
                thickness = size;
        *start = (size - thickness) / 2U;
        *end = *start + thickness;
}

static unsigned int
ArmThickness(unsigned int style, unsigned int light)
{
        if (style == ARM_HEAVY)
                return light * 2U;
        return style == ARM_NONE ? 0U : light;
}

static unsigned int
Max(unsigned int left, unsigned int right)
{
        return left > right ? left : right;
}

static void
PlanSingleArms(unsigned int left, unsigned int right, unsigned int up, unsigned int down,
               unsigned int w, unsigned int h, unsigned int t, XtpBoxGlyph *glyph)
{
        unsigned int horizontal = Max(ArmThickness(left, t), ArmThickness(right, t));
        unsigned int vertical = Max(ArmThickness(up, t), ArmThickness(down, t));
        unsigned int junction_v = vertical != 0 ? vertical : horizontal;
        unsigned int junction_h = horizontal != 0 ? horizontal : vertical;
        unsigned int vx0;
        unsigned int vx1;
        unsigned int hy0;
        unsigned int hy1;
        unsigned int a0;
        unsigned int a1;

        Band(w, junction_v, &vx0, &vx1);
        Band(h, junction_h, &hy0, &hy1);
        if (left != ARM_NONE) {
                Band(h, ArmThickness(left, t), &a0, &a1);
                AddRect(glyph, 0, a0, vx1, a1 - a0, w, h);
        }
        if (right != ARM_NONE) {
                Band(h, ArmThickness(right, t), &a0, &a1);
                AddRect(glyph, vx0, a0, w - vx0, a1 - a0, w, h);
        }
        if (up != ARM_NONE) {
                Band(w, ArmThickness(up, t), &a0, &a1);
                AddRect(glyph, a0, 0, a1 - a0, hy1, w, h);
        }
        if (down != ARM_NONE) {
                Band(w, ArmThickness(down, t), &a0, &a1);
                AddRect(glyph, a0, hy0, a1 - a0, h - hy0, w, h);
        }
}

typedef struct
{
        unsigned int vl0, vl1, vr0, vr1; /* double vertical strips */
        unsigned int ht0, ht1, hb0, hb1; /* double horizontal strips */
        unsigned int sx0, sx1, sy0, sy1; /* single strips */
} DoubleGeometry;

static void
PlanDoubleArms(unsigned int left, unsigned int right, unsigned int up, unsigned int down,
               unsigned int w, unsigned int h, unsigned int t, XtpBoxGlyph *glyph)
{
        DoubleGeometry g;
        unsigned int span0;
        unsigned int span1;
        bool vertical_double = up == ARM_DOUBLE || down == ARM_DOUBLE;
        bool vertical_both = up == ARM_DOUBLE && down == ARM_DOUBLE;
        bool vertical_single = up == ARM_LIGHT || down == ARM_LIGHT;
        bool horizontal_double = left == ARM_DOUBLE || right == ARM_DOUBLE;
        bool horizontal_both = left == ARM_DOUBLE && right == ARM_DOUBLE;
        bool horizontal_single = left == ARM_LIGHT || right == ARM_LIGHT;

        Band(w, 3U * t, &span0, &span1);
        g.vl0 = span0;
        g.vl1 = span0 + t;
        g.vr1 = span1;
        g.vr0 = span1 - t;
        Band(h, 3U * t, &span0, &span1);
        g.ht0 = span0;
        g.ht1 = span0 + t;
        g.hb1 = span1;
        g.hb0 = span1 - t;
        Band(w, t, &g.sx0, &g.sx1);
        Band(h, t, &g.sy0, &g.sy1);

        if (left == ARM_DOUBLE) {
                unsigned int top = w;
                unsigned int bottom = w;

                if (vertical_both) {
                        top = bottom = g.vl1;
                } else if (down == ARM_DOUBLE) {
                        top = g.vr1;
                        bottom = g.vl1;
                } else if (up == ARM_DOUBLE) {
                        top = g.vl1;
                        bottom = g.vr1;
                } else if (vertical_single) {
                        top = bottom = g.sx1;
                }
                AddRect(glyph, 0, g.ht0, top, t, w, h);
                AddRect(glyph, 0, g.hb0, bottom, t, w, h);
        } else if (left == ARM_LIGHT) {
                unsigned int stop = w;

                if (right != ARM_LIGHT && vertical_double)
                        stop = vertical_both ? g.vl1 : g.vr1;
                AddRect(glyph, 0, g.sy0, stop, t, w, h);
        }
        if (right == ARM_DOUBLE) {
                unsigned int top = 0;
                unsigned int bottom = 0;

                if (vertical_both) {
                        top = bottom = g.vr0;
                } else if (down == ARM_DOUBLE) {
                        top = g.vl0;
                        bottom = g.vr0;
                } else if (up == ARM_DOUBLE) {
                        top = g.vr0;
                        bottom = g.vl0;
                } else if (vertical_single) {
                        top = bottom = g.sx0;
                }
                AddRect(glyph, top, g.ht0, w - top, t, w, h);
                AddRect(glyph, bottom, g.hb0, w - bottom, t, w, h);
        } else if (right == ARM_LIGHT) {
                unsigned int start = 0;

                if (left != ARM_LIGHT && vertical_double)
                        start = vertical_both ? g.vr0 : g.vl0;
                AddRect(glyph, start, g.sy0, w - start, t, w, h);
        }
        if (up == ARM_DOUBLE) {
                unsigned int left_stop = h;
                unsigned int right_stop = h;

                if (horizontal_both) {
                        left_stop = right_stop = g.ht1;
                } else if (right == ARM_DOUBLE) {
                        left_stop = g.hb1;
                        right_stop = g.ht1;
                } else if (left == ARM_DOUBLE) {
                        left_stop = g.ht1;
                        right_stop = g.hb1;
                } else if (horizontal_single) {
                        left_stop = right_stop = g.sy1;
                }
                AddRect(glyph, g.vl0, 0, t, left_stop, w, h);
                AddRect(glyph, g.vr0, 0, t, right_stop, w, h);
        } else if (up == ARM_LIGHT) {
                unsigned int stop = h;

                if (down != ARM_LIGHT && horizontal_double)
                        stop = horizontal_both ? g.ht1 : g.hb1;
                AddRect(glyph, g.sx0, 0, t, stop, w, h);
        }
        if (down == ARM_DOUBLE) {
                unsigned int left_start = 0;
                unsigned int right_start = 0;

                if (horizontal_both) {
                        left_start = right_start = g.hb0;
                } else if (right == ARM_DOUBLE) {
                        left_start = g.ht0;
                        right_start = g.hb0;
                } else if (left == ARM_DOUBLE) {
                        left_start = g.hb0;
                        right_start = g.ht0;
                } else if (horizontal_single) {
                        left_start = right_start = g.sy0;
                }
                AddRect(glyph, g.vl0, left_start, t, h - left_start, w, h);
                AddRect(glyph, g.vr0, right_start, t, h - right_start, w, h);
        } else if (down == ARM_LIGHT) {
                unsigned int start = 0;

                if (up != ARM_LIGHT && horizontal_double)
                        start = horizontal_both ? g.hb0 : g.ht0;
                AddRect(glyph, g.sx0, start, t, h - start, w, h);
        }
}

static void
PlanDashes(uint32_t codepoint, unsigned int w, unsigned int h, unsigned int t, XtpBoxGlyph *glyph)
{
        unsigned int dashes = codepoint >= 0x254CU ? 2U : (codepoint >= 0x2508U ? 4U : 3U);
        bool vertical = (codepoint & 2U) != 0;
        unsigned int thickness = (codepoint & 1U) != 0 ? 2U * t : t;
        unsigned int length = vertical ? h : w;
        unsigned int gap = length / (dashes * 3U);
        unsigned int band0;
        unsigned int band1;
        unsigned int index;

        if (gap == 0)
                gap = 1;
        Band(vertical ? w : h, thickness, &band0, &band1);
        for (index = 0; index < dashes; ++index) {
                unsigned int start = index * length / dashes + gap / 2U;
                unsigned int end = (index + 1U) * length / dashes - (gap - gap / 2U);

                if (end <= start)
                        end = start + 1U;
                if (vertical)
                        AddRect(glyph, band0, start, band1 - band0, end - start, w, h);
                else
                        AddRect(glyph, start, band0, end - start, band1 - band0, w, h);
        }
}

static void
AddRuns(XtpBoxGlyph *glyph, const uint8_t *row, unsigned int y, unsigned int w, unsigned int h)
{
        unsigned int x = 0;

        while (x < w) {
                unsigned int start;

                while (x < w && row[x] == 0)
                        ++x;
                start = x;
                while (x < w && row[x] != 0)
                        ++x;
                if (x > start)
                        AddRect(glyph, start, y, x - start, 1U, w, h);
        }
}

static void
PlanArc(uint32_t codepoint, unsigned int w, unsigned int h, unsigned int t, XtpBoxGlyph *glyph)
{
        bool arm_down = codepoint == 0x256DU || codepoint == 0x256EU;
        bool arm_right = codepoint == 0x256DU || codepoint == 0x2570U;
        unsigned int sx0;
        unsigned int sx1;
        unsigned int sy0;
        unsigned int sy1;
        double radius = (double)((w < h ? w : h) / 2U);
        double px;
        double py;
        double cx;
        double cy;
        double half = (double)t / 2.0;
        unsigned int y;
        uint8_t *row = box_realloc(NULL, w);

        if (row == NULL) {
                glyph->failed = true;
                return;
        }
        if (radius < 1.0)
                radius = 1.0;
        Band(w, t, &sx0, &sx1);
        Band(h, t, &sy0, &sy1);
        px = (double)sx0 + half;
        py = (double)sy0 + half;
        cx = arm_right ? px + radius : px - radius;
        cy = arm_down ? py + radius : py - radius;
        for (y = 0; y < h; ++y) {
                unsigned int x;
                double yc = (double)y + 0.5;

                for (x = 0; x < w; ++x) {
                        double xc = (double)x + 0.5;
                        bool ink = false;

                        if (fabs(xc - px) <= half && (arm_down ? yc >= cy : yc <= cy))
                                ink = true;
                        else if (fabs(yc - py) <= half && (arm_right ? xc >= cx : xc <= cx))
                                ink = true;
                        else if ((arm_right ? xc <= cx : xc >= cx) &&
                                 (arm_down ? yc <= cy : yc >= cy)) {
                                double distance = hypot(xc - cx, yc - cy);

                                ink = fabs(distance - radius) <= half;
                        }
                        row[x] = ink ? 1U : 0U;
                }
                AddRuns(glyph, row, y, w, h);
        }
        free(row);
}

static void
PlanDiagonal(uint32_t codepoint, unsigned int w, unsigned int h, unsigned int t, XtpBoxGlyph *glyph)
{
        bool falling = codepoint != 0x2571U;
        bool rising = codepoint != 0x2572U;
        double spread = (double)(t - 1U) / 2.0;
        unsigned int y;
        uint8_t *row = box_realloc(NULL, w);

        if (row == NULL) {
                glyph->failed = true;
                return;
        }
        for (y = 0; y < h; ++y) {
                double xa = (double)y * (double)w / (double)h;
                double xb = (double)(y + 1U) * (double)w / (double)h;
                int start = (int)floor(xa - spread);
                int end = (int)ceil(xb + spread);
                int x;

                if (end <= start)
                        end = start + 1;
                memset(row, 0, w);
                for (x = start; x < end; ++x) {
                        if (falling && x >= 0 && (unsigned int)x < w)
                                row[x] = 1U;
                        if (rising && x >= 0 && (unsigned int)x < w)
                                row[w - 1U - (unsigned int)x] = 1U;
                }
                AddRuns(glyph, row, y, w, h);
        }
        free(row);
}

static void
PlanBlock(uint32_t codepoint, unsigned int w, unsigned int h, XtpBoxGlyph *glyph)
{
        unsigned int half_w = w / 2U;
        unsigned int half_h = h / 2U;

        if (codepoint == 0x2580U) {
                AddRect(glyph, 0, 0, w, half_h, w, h);
        } else if (codepoint <= 0x2588U) {
                unsigned int top = h * (0x2588U - codepoint) / 8U;

                if (top >= h)
                        top = h - 1U;
                AddRect(glyph, 0, top, w, h - top, w, h);
        } else if (codepoint <= 0x258FU) {
                unsigned int columns = w * (0x2590U - codepoint) / 8U;

                if (columns == 0)
                        columns = 1;
                AddRect(glyph, 0, 0, columns, h, w, h);
        } else if (codepoint == 0x2590U) {
                AddRect(glyph, half_w, 0, w - half_w, h, w, h);
        } else if (codepoint <= 0x2593U) {
                glyph->shade = (XtpBoxShade)(codepoint - 0x2590U);
        } else if (codepoint == 0x2594U) {
                unsigned int rows = h / 8U;

                AddRect(glyph, 0, 0, w, rows == 0 ? 1U : rows, w, h);
        } else if (codepoint == 0x2595U) {
                unsigned int columns = w / 8U;

                if (columns == 0)
                        columns = 1;
                AddRect(glyph, w - columns, 0, columns, h, w, h);
        } else {
                uint8_t mask = quadrants[codepoint - 0x2596U];

                if (mask & QUAD_UL)
                        AddRect(glyph, 0, 0, half_w, half_h, w, h);
                if (mask & QUAD_UR)
                        AddRect(glyph, half_w, 0, w - half_w, half_h, w, h);
                if (mask & QUAD_LL)
                        AddRect(glyph, 0, half_h, half_w, h - half_h, w, h);
                if (mask & QUAD_LR)
                        AddRect(glyph, half_w, half_h, w - half_w, h - half_h, w, h);
        }
}

/* Dot size and spacing follow Ghostty's braille sprite; false when dots would vanish. */
static bool
PlanBraille(uint32_t codepoint, unsigned int width, unsigned int height, XtpBoxGlyph *glyph)
{
        int w = (int)(width / 4U < height / 8U ? width / 4U : height / 8U);
        int x_spacing = (int)(width / 4U);
        int y_spacing = (int)(height / 8U);
        int x_margin = x_spacing / 2;
        int y_margin = y_spacing / 2;
        int x_left = (int)width - 2 * x_margin - x_spacing - 2 * w;
        int y_left = (int)height - 2 * y_margin - 3 * y_spacing - 4 * w;
        int x[2];
        int y[4];
        unsigned int dot;

        if (x_left >= 2 && y_left >= 4 && w == 0) {
                w = 1;
                x_left -= 2;
                y_left -= 4;
        }
        if (w == 0)
                return false;
        if (x_left >= 2 && x_margin == 0) {
                x_margin = 1;
                x_left -= 2;
        }
        if (y_left >= 2 && y_margin == 0) {
                y_margin = 1;
                y_left -= 2;
        }
        if (x_left >= 1) {
                x_spacing += 1;
                x_left -= 1;
        }
        if (y_left >= 3) {
                y_spacing += 1;
                y_left -= 3;
        }
        if (x_left >= 2) {
                x_margin += 1;
                x_left -= 2;
        }
        if (y_left >= 2) {
                y_margin += 1;
                y_left -= 2;
        }
        if (x_left >= 2 && y_left >= 4)
                w += 1;
        x[0] = x_margin;
        x[1] = x_margin + w + x_spacing;
        y[0] = y_margin;
        for (dot = 1; dot < 4; ++dot)
                y[dot] = y[dot - 1] + w + y_spacing;
        /* Bits 0-2 and 6 are the left column top to bottom; bits 3-5 and 7 the right. */
        for (dot = 0; dot < 8; ++dot) {
                static const uint8_t column[8] = {0, 0, 0, 1, 1, 1, 0, 1};
                static const uint8_t row[8] = {0, 1, 2, 0, 1, 2, 3, 3};

                if ((codepoint >> dot) & 1U)
                        AddRect(glyph, (unsigned int)x[column[dot]], (unsigned int)y[row[dot]],
                                (unsigned int)w, (unsigned int)w, width, height);
        }
        return true;
}

enum
{
        POWERLINE_TRIANGLE,
        POWERLINE_ROUND,
        POWERLINE_SLANT
};

/* Inked columns of row y measured from the flat side, at least one so segments join. */
static unsigned int
PowerlineExtent(unsigned int shape, unsigned int y, unsigned int w, unsigned int h)
{
        uint64_t extent;

        if (shape == POWERLINE_SLANT) {
                extent = ((uint64_t)w * (2U * (uint64_t)y + 1U) + 2U * (uint64_t)h - 1U) /
                         (2U * (uint64_t)h);
        } else {
                uint64_t top = 2U * (uint64_t)y + 1U;
                uint64_t bottom = 2U * (uint64_t)h - 2U * (uint64_t)y - 1U;
                uint64_t edge = top < bottom ? top : bottom;

                if (shape == POWERLINE_TRIANGLE) {
                        extent = ((uint64_t)w * edge + h - 1U) / h;
                } else {
                        double d = (double)(h - edge) / (double)h;

                        extent = (uint64_t)ceil((double)w * sqrt(1.0 - d * d) - 1e-9);
                }
        }
        if (extent < 1U)
                extent = 1U;
        return extent > w ? w : (unsigned int)extent;
}

static void
AddRowRun(XtpBoxGlyph *glyph, unsigned int x, unsigned int y, unsigned int width, bool mirror,
          unsigned int w, unsigned int h)
{
        XtpBoxRect *last = glyph->count != 0 ? &glyph->rects[glyph->count - 1U] : NULL;

        if (mirror)
                x = w - x - width;
        if (last != NULL && !glyph->failed && last->x == x && last->width == width &&
            last->y + last->height == y) {
                last->height += 1U;
                return;
        }
        AddRect(glyph, x, y, width, 1U, w, h);
}

static void
PlanPowerline(uint32_t codepoint, unsigned int w, unsigned int h, unsigned int t,
              XtpBoxGlyph *glyph)
{
        unsigned int offset = codepoint - 0xE0B0U;
        unsigned int shape;
        bool mirror;
        bool flip = false;
        bool thin = false;
        unsigned int y;

        if (offset == 0x9U || offset == 0xFU) {
                PlanDiagonal(0x2572U, w, h, t, glyph);
                return;
        }
        if (offset == 0xBU || offset == 0xDU) {
                PlanDiagonal(0x2571U, w, h, t, glyph);
                return;
        }
        if (offset < 8U) {
                shape = offset < 4U ? POWERLINE_TRIANGLE : POWERLINE_ROUND;
                mirror = (offset & 2U) != 0;
                thin = (offset & 1U) != 0;
        } else {
                shape = POWERLINE_SLANT;
                mirror = offset == 0xAU || offset == 0xEU;
                flip = offset >= 0xCU;
        }
        for (y = 0; y < h; ++y) {
                unsigned int extent = PowerlineExtent(shape, flip ? h - 1U - y : y, w, h);
                unsigned int start = 0;

                /* A thin stroke overlaps its neighbor rows; the edge rows reach the flat side. */
                if (thin) {
                        unsigned int nearest = extent;

                        if (y == 0 || y + 1U == h)
                                nearest = 0;
                        if (y > 0 && PowerlineExtent(shape, y - 1U, w, h) < nearest)
                                nearest = PowerlineExtent(shape, y - 1U, w, h);
                        if (y + 1U < h && PowerlineExtent(shape, y + 1U, w, h) < nearest)
                                nearest = PowerlineExtent(shape, y + 1U, w, h);
                        start = nearest > t ? nearest - t : 0;
                }
                AddRowRun(glyph, start, y, extent - start, mirror, w, h);
        }
}

bool
XtpBoxGlyphPlan(uint32_t codepoint, unsigned int width, unsigned int height, bool bold,
                XtpBoxGlyph *glyph)
{
        unsigned int t;

        if (glyph == NULL)
                return false;
        memset(glyph, 0, sizeof(*glyph));
        if (!XtpBoxGlyphCodepoint(codepoint) || width == 0 || height == 0)
                return false;
        t = XtpBoxGlyphThickness(width, height, bold);
        if (codepoint >= 0xE0B0U) {
                PlanPowerline(codepoint, width, height, t, glyph);
        } else if (codepoint >= 0x2800U) {
                if (!PlanBraille(codepoint, width, height, glyph))
                        return false;
        } else if (codepoint >= 0x2580U) {
                PlanBlock(codepoint, width, height, glyph);
        } else if ((codepoint >= 0x2504U && codepoint <= 0x250BU) ||
                   (codepoint >= 0x254CU && codepoint <= 0x254FU)) {
                PlanDashes(codepoint, width, height, t, glyph);
        } else if (codepoint >= 0x256DU && codepoint <= 0x2570U) {
                PlanArc(codepoint, width, height, t, glyph);
        } else if (codepoint >= 0x2571U && codepoint <= 0x2573U) {
                PlanDiagonal(codepoint, width, height, t, glyph);
        } else {
                uint8_t arms = box_arms[codepoint - 0x2500U];
                unsigned int left = (arms >> 6) & 3U;
                unsigned int right = (arms >> 4) & 3U;
                unsigned int up = (arms >> 2) & 3U;
                unsigned int down = arms & 3U;

                if (left == ARM_DOUBLE || right == ARM_DOUBLE || up == ARM_DOUBLE ||
                    down == ARM_DOUBLE)
                        PlanDoubleArms(left, right, up, down, width, height, t, glyph);
                else
                        PlanSingleArms(left, right, up, down, width, height, t, glyph);
        }
        if (glyph->failed) {
                XtpBoxGlyphFree(glyph);
                return false;
        }
        return true;
}

void
XtpBoxGlyphFree(XtpBoxGlyph *glyph)
{
        free(glyph->rects);
        memset(glyph, 0, sizeof(*glyph));
}

bool
XtpBoxShadePixel(XtpBoxShade shade, unsigned int x, unsigned int y)
{
        switch (shade) {
        case XTP_BOX_SHADE_LIGHT:
                return (x & 1U) == 0 && (y & 1U) == 0;
        case XTP_BOX_SHADE_MEDIUM:
                return ((x + y) & 1U) == 0;
        case XTP_BOX_SHADE_DARK:
                return (x & 1U) == 0 || (y & 1U) == 0;
        case XTP_BOX_SHADE_NONE:
                break;
        }
        return false;
}

void
XtpBoxGlyphRasterize(const XtpBoxGlyph *glyph, unsigned int width, unsigned int height,
                     uint8_t *mask)
{
        size_t index;
        unsigned int x;
        unsigned int y;

        memset(mask, 0, (size_t)width * height);
        for (index = 0; index < glyph->count; ++index) {
                const XtpBoxRect *rect = &glyph->rects[index];

                for (y = rect->y; y < rect->y + rect->height && y < height; ++y)
                        for (x = rect->x; x < rect->x + rect->width && x < width; ++x)
                                mask[(size_t)y * width + x] = 1U;
        }
        if (glyph->shade == XTP_BOX_SHADE_NONE)
                return;
        for (y = 0; y < height; ++y)
                for (x = 0; x < width; ++x)
                        if (XtpBoxShadePixel(glyph->shade, x, y))
                                mask[(size_t)y * width + x] = 1U;
}
