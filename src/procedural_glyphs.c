#include "procedural_glyphs.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/*
 * The mosaic boundary masks and octant ordering are derived from the Unicode
 * character names.  They were cross-checked against Ghostty's MIT-licensed
 * sprite tables (src/font/sprite/draw/symbols_for_legacy_computing*.zig).
 */
static const uint16_t smooth_mosaics[44] = {
    0x01c, 0x02c, 0x01a, 0x02a, 0x019, 0x32a, 0x12a, 0x32c, 0x12c, 0x328, 0x0ac,
    0x070, 0x068, 0x0b0, 0x0a8, 0x130, 0x2a9, 0x0a9, 0x269, 0x069, 0x229, 0x06a,
    0x135, 0x125, 0x133, 0x123, 0x131, 0x203, 0x103, 0x205, 0x105, 0x209, 0x185,
    0x159, 0x149, 0x199, 0x189, 0x119, 0x380, 0x181, 0x340, 0x141, 0x320, 0x143,
};

static const uint8_t octants[230] = {
    0x04, 0x06, 0x07, 0x08, 0x09, 0x0b, 0x0c, 0x0d, 0x0e, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
    0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x51, 0x52, 0x53, 0x54, 0x56, 0x57, 0x58, 0x59, 0x5b, 0x5c, 0x5d,
    0x5e, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
    0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
    0x7f, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa1, 0xa2, 0xa3, 0xa4, 0xa6, 0xa7, 0xa8, 0xa9, 0xab, 0xac, 0xad, 0xae, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc1, 0xc2, 0xc3, 0xc4,
    0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4,
    0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf1, 0xf2, 0xf3, 0xf4, 0xf6,
    0xf7, 0xf8, 0xf9, 0xfb, 0xfd, 0xfe,
};

typedef struct
{
        double x;
        double y;
} Point;

static unsigned int
Part(unsigned int size, unsigned int index, unsigned int parts)
{
        return (unsigned int)(((uint64_t)size * index) / parts);
}

static void
Rect(uint8_t *mask, unsigned int w, unsigned int h, unsigned int x0, unsigned int y0,
     unsigned int x1, unsigned int y1)
{
        unsigned int x;
        unsigned int y;

        if (x1 > w)
                x1 = w;
        if (y1 > h)
                y1 = h;
        for (y = y0; y < y1; ++y)
                for (x = x0; x < x1; ++x)
                        mask[(size_t)y * w + x] = 1;
}

static void
GridMask(uint8_t *mask, unsigned int w, unsigned int h, unsigned int columns, unsigned int rows,
         unsigned int bits)
{
        unsigned int bit;

        for (bit = 0; bit < columns * rows; ++bit)
                if ((bits >> bit) & 1U) {
                        unsigned int x = bit % columns;
                        unsigned int y = bit / columns;

                        Rect(mask, w, h, Part(w, x, columns), Part(h, y, rows),
                             Part(w, x + 1U, columns), Part(h, y + 1U, rows));
                }
}

static bool
InsidePolygon(double x, double y, const Point *points, size_t count)
{
        bool inside = false;
        size_t i;
        size_t j = count - 1U;

        for (i = 0; i < count; j = i++) {
                if ((points[i].y > y) != (points[j].y > y) &&
                    x < (points[j].x - points[i].x) * (y - points[i].y) /
                                (points[j].y - points[i].y) +
                            points[i].x)
                        inside = !inside;
        }
        return inside;
}

static void
Polygon(uint8_t *mask, unsigned int w, unsigned int h, const Point *points, size_t count,
        bool invert, unsigned int shade)
{
        unsigned int x;
        unsigned int y;

        for (y = 0; y < h; ++y)
                for (x = 0; x < w; ++x) {
                        bool inside =
                            InsidePolygon((double)x + 0.5, (double)y + 0.5, points, count);

                        if (invert)
                                inside = !inside;
                        if (inside && (shade == 0U || ((x + y) & 1U) == 0U))
                                mask[(size_t)y * w + x] = 1;
                }
}

static double
SegmentDistance(double px, double py, Point a, Point b)
{
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double denominator = dx * dx + dy * dy;
        double u = denominator == 0.0 ? 0.0 : ((px - a.x) * dx + (py - a.y) * dy) / denominator;
        double x;
        double y;

        if (u < 0.0)
                u = 0.0;
        if (u > 1.0)
                u = 1.0;
        x = a.x + u * dx;
        y = a.y + u * dy;
        return hypot(px - x, py - y);
}

static void
Line(uint8_t *mask, unsigned int w, unsigned int h, Point a, Point b, unsigned int thickness)
{
        double radius = (double)thickness / 2.0;
        unsigned int x;
        unsigned int y;

        for (y = 0; y < h; ++y)
                for (x = 0; x < w; ++x) {
                        bool ink =
                            SegmentDistance((double)x + 0.5, (double)y + 0.5, a, b) <= radius;

                        if (ink)
                                mask[(size_t)y * w + x] = 1;
                }
}

static void
CutLine(uint8_t *mask, unsigned int w, unsigned int h, Point a, Point b, unsigned int thickness)
{
        double radius = (double)thickness / 2.0;
        unsigned int x;
        unsigned int y;

        for (y = 0; y < h; ++y)
                for (x = 0; x < w; ++x)
                        if (SegmentDistance((double)x + 0.5, (double)y + 0.5, a, b) <= radius)
                                mask[(size_t)y * w + x] = 0;
}

static void
Ellipse(uint8_t *mask, unsigned int w, unsigned int h, double cx, double cy, double rx, double ry,
        unsigned int thickness, bool filled)
{
        unsigned int x;
        unsigned int y;
        double edge = 2.0 * thickness / (rx < ry ? rx : ry);

        for (y = 0; y < h; ++y)
                for (x = 0; x < w; ++x) {
                        double dx = ((double)x + 0.5 - cx) / rx;
                        double dy = ((double)y + 0.5 - cy) / ry;
                        double d = dx * dx + dy * dy;

                        if ((filled && d <= 1.0) || (!filled && d <= 1.0 && d >= 1.0 - edge))
                                mask[(size_t)y * w + x] = 1;
                }
}

static void
CirclePiece(uint8_t *mask, unsigned int w, unsigned int h, double x, double y, double sx, double sy,
            unsigned int corner, unsigned int thickness)
{
        double rx = w * sx;
        double ry = h * sy;
        double cx = rx - w * x;
        double cy = ry - h * y;

        (void)corner;
        Ellipse(mask, w, h, cx, cy, rx, ry, thickness, false);
}

static void
LegacyBlocks(uint32_t cp, uint8_t *mask, unsigned int w, unsigned int h)
{
        unsigned int n;

        if (cp <= 0x1FB75U) {
                n = cp - 0x1FB6FU;
                Rect(mask, w, h, Part(w, n, 8), 0, Part(w, n + 1U, 8), h);
        } else if (cp <= 0x1FB7BU) {
                n = cp - 0x1FB75U;
                Rect(mask, w, h, 0, Part(h, n, 8), w, Part(h, n + 1U, 8));
        } else if (cp <= 0x1FB80U) {
                bool left = cp == 0x1FB7CU || cp == 0x1FB7DU;
                bool top = cp == 0x1FB7DU || cp == 0x1FB7EU || cp == 0x1FB80U;
                bool right = cp == 0x1FB7EU || cp == 0x1FB7FU;
                bool bottom = cp == 0x1FB7CU || cp == 0x1FB7FU || cp == 0x1FB80U;

                if (left)
                        Rect(mask, w, h, 0, 0, Part(w, 1, 8), h);
                if (right)
                        Rect(mask, w, h, Part(w, 7, 8), 0, w, h);
                if (top)
                        Rect(mask, w, h, 0, 0, w, Part(h, 1, 8));
                if (bottom)
                        Rect(mask, w, h, 0, Part(h, 7, 8), w, h);
        } else if (cp == 0x1FB81U) {
                const unsigned int rows[] = {0, 2, 4, 7};
                size_t i;

                for (i = 0; i < 4; ++i)
                        Rect(mask, w, h, 0, Part(h, rows[i], 8), w, Part(h, rows[i] + 1U, 8));
        } else if (cp <= 0x1FB86U) {
                const unsigned int eighths[] = {2, 3, 5, 6, 7};
                n = eighths[cp - 0x1FB82U];
                Rect(mask, w, h, 0, 0, w, Part(h, n, 8));
        } else if (cp <= 0x1FB8BU) {
                const unsigned int eighths[] = {2, 3, 5, 6, 7};
                n = eighths[cp - 0x1FB87U];
                Rect(mask, w, h, Part(w, 8U - n, 8), 0, w, h);
        } else if (cp <= 0x1FB94U) {
                unsigned int x;
                unsigned int y;

                for (y = 0; y < h; ++y)
                        for (x = 0; x < w; ++x) {
                                bool shaded = ((x + y) & 1U) == 0U;
                                bool solid = (cp == 0x1FB91U && y < h / 2U) ||
                                             (cp == 0x1FB92U && y >= h / 2U) ||
                                             (cp == 0x1FB94U && x >= w / 2U);
                                bool half = (cp == 0x1FB8CU && x < w / 2U) ||
                                            (cp == 0x1FB8DU && x >= w / 2U) ||
                                            (cp == 0x1FB8EU && y < h / 2U) ||
                                            (cp == 0x1FB8FU && y >= h / 2U) || cp >= 0x1FB90U;

                                if (half && (solid || shaded))
                                        mask[(size_t)y * w + x] = 1;
                        }
        } else if (cp <= 0x1FB96U) {
                unsigned int x;
                unsigned int y;
                unsigned int rows = w == 0 ? 4U : (4U * h + w / 2U) / w;

                if (rows == 0)
                        rows = 1;
                for (y = 0; y < h; ++y)
                        for (x = 0; x < w; ++x)
                                if (((x * 4U / w) + (y * rows / h)) % 2U == cp - 0x1FB95U)
                                        mask[(size_t)y * w + x] = 1;
        } else {
                Rect(mask, w, h, 0, Part(h, 1, 4), w, Part(h, 2, 4));
                Rect(mask, w, h, 0, Part(h, 3, 4), w, h);
        }
}

static void
SmoothMosaic(uint32_t cp, uint8_t *mask, unsigned int w, unsigned int h)
{
        static const double nx[10] = {0, 0, 0, 0, .5, 1, 1, 1, 1, .5};
        static const double ny[10] = {0, 1.0 / 3.0, 2.0 / 3.0, 1, 1, 1, 2.0 / 3.0, 1.0 / 3.0, 0, 0};
        uint16_t bits = smooth_mosaics[cp - 0x1FB3CU];
        Point points[10];
        size_t count = 0;
        unsigned int i;

        for (i = 0; i < 10U; ++i)
                if ((bits >> i) & 1U) {
                        points[count].x = nx[i] * w;
                        points[count].y = ny[i] * h;
                        ++count;
                }
        Polygon(mask, w, h, points, count, false, 0);
}

static void
Triangle(uint8_t *mask, unsigned int w, unsigned int h, unsigned int corner, bool invert,
         bool outline, unsigned int thickness, unsigned int shade)
{
        Point p[3];

        p[0] =
            (Point){corner == 1U || corner == 2U ? (double)w : 0.0, corner >= 2U ? (double)h : 0.0};
        p[1] =
            (Point){corner == 1U || corner == 2U ? (double)w : 0.0, corner >= 2U ? 0.0 : (double)h};
        p[2] =
            (Point){corner == 1U || corner == 2U ? 0.0 : (double)w, corner >= 2U ? (double)h : 0.0};
        if (!outline) {
                Polygon(mask, w, h, p, 3, invert, shade);
        } else {
                Line(mask, w, h, p[0], p[1], thickness);
                Line(mask, w, h, p[0], p[2], thickness);
                Line(mask, w, h, p[1], p[2], thickness);
        }
}

static void
EdgeTriangle(uint8_t *mask, unsigned int w, unsigned int h, unsigned int edge, bool invert)
{
        Point p[3] = {{w / 2.0, h / 2.0}, {0, 0}, {0, 0}};

        if (edge == 0U) {
                p[1] = (Point){0, 0};
                p[2] = (Point){0, (double)h};
        } else if (edge == 1U) {
                p[1] = (Point){0, 0};
                p[2] = (Point){(double)w, 0};
        } else if (edge == 2U) {
                p[1] = (Point){(double)w, 0};
                p[2] = (Point){(double)w, (double)h};
        } else {
                p[1] = (Point){0, (double)h};
                p[2] = (Point){(double)w, (double)h};
        }
        Polygon(mask, w, h, p, 3, invert, 0);
}

static Point
Anchor(unsigned int index, unsigned int w, unsigned int h)
{
        static const double x[] = {0, .5, 1, 0, .5, 1, 0, .5, 1};
        static const double y[] = {0, 0, 0, .5, .5, .5, 1, 1, 1};

        return (Point){x[index] * w, y[index] * h};
}

static void
LegacyLines(uint32_t cp, uint8_t *mask, unsigned int w, unsigned int h, unsigned int t)
{
        static const uint8_t pairs[16][6] = {
            {5, 6, 0xff},       {2, 3, 0xff},       {0, 5, 0xff},       {3, 8, 0xff},
            {0, 7, 0xff},       {1, 8, 0xff},       {2, 7, 0xff},       {1, 6, 0xff},
            {0, 4, 4, 2, 0xff}, {2, 4, 4, 8, 0xff}, {6, 4, 4, 8, 0xff}, {0, 4, 4, 6, 0xff},
            {0, 7, 7, 2, 0xff}, {2, 3, 3, 8, 0xff}, {6, 1, 1, 8, 0xff}, {0, 5, 5, 6, 0xff},
        };
        const uint8_t *path = pairs[cp - 0x1FBD0U];
        size_t i;

        for (i = 0; i + 1U < 6U && path[i] != 0xffU && path[i + 1U] != 0xffU; i += 2U)
                Line(mask, w, h, Anchor(path[i], w, h), Anchor(path[i + 1U], w, h), t);
}

static void
QuarterRect(uint8_t *mask, unsigned int w, unsigned int h, unsigned int x0, unsigned int x1,
            unsigned int y0, unsigned int y1)
{
        Rect(mask, w, h, Part(w, x0, 4), Part(h, y0, 4), Part(w, x1, 4), Part(h, y1, 4));
}

bool
XtpProceduralExtendedCodepoint(uint32_t cp)
{
        return (cp >= 0x23BAU && cp <= 0x23BDU) || (cp >= 0x25E2U && cp <= 0x25E5U) ||
               (cp >= 0x25F8U && cp <= 0x25FAU) || cp == 0x25FFU || cp == 0xE0D2U ||
               cp == 0xE0D4U || (cp >= 0x1CC1BU && cp <= 0x1CC1EU) ||
               (cp >= 0x1CC21U && cp <= 0x1CC3FU) || (cp >= 0x1CD00U && cp <= 0x1CDE5U) ||
               cp == 0x1CE00U || cp == 0x1CE01U || cp == 0x1CE0BU || cp == 0x1CE0CU ||
               (cp >= 0x1CE16U && cp <= 0x1CE19U) || (cp >= 0x1CE51U && cp <= 0x1CE8FU) ||
               (cp >= 0x1CE90U && cp <= 0x1CEAFU) || (cp >= 0x1FB00U && cp <= 0x1FB92U) ||
               (cp >= 0x1FB94U && cp <= 0x1FB9BU) || (cp >= 0x1FB9CU && cp <= 0x1FBAFU) ||
               (cp >= 0x1FBBDU && cp <= 0x1FBBFU) || (cp >= 0x1FBCEU && cp <= 0x1FBEFU);
}

bool
XtpProceduralExtendedRasterize(uint32_t cp, unsigned int w, unsigned int h, unsigned int t,
                               uint8_t *mask)
{
        if (!XtpProceduralExtendedCodepoint(cp) || mask == NULL || w == 0 || h == 0)
                return false;
        memset(mask, 0, (size_t)w * h);
        if (cp >= 0x1CD00U && cp <= 0x1CDE5U) {
                GridMask(mask, w, h, 2, 4, octants[cp - 0x1CD00U]);
        } else if (cp >= 0x1FB00U && cp <= 0x1FB3BU) {
                unsigned int i = cp - 0x1FB00U;
                GridMask(mask, w, h, 2, 3, i + i / 0x14U + 1U);
        } else if (cp >= 0x1FB3CU && cp <= 0x1FB67U) {
                SmoothMosaic(cp, mask, w, h);
        } else if (cp >= 0x1FB68U && cp <= 0x1FB6FU) {
                EdgeTriangle(mask, w, h, (cp - 0x1FB68U) & 3U, cp < 0x1FB6CU);
        } else if (cp >= 0x1FB70U && cp <= 0x1FB97U && cp != 0x1FB93U) {
                LegacyBlocks(cp, mask, w, h);
        } else if (cp == 0x1FB98U || cp == 0x1FB99U) {
                int offset;
                int spacing = (int)(t * 2U);

                if (spacing < 2)
                        spacing = 2;
                for (offset = -(int)w; offset <= (int)w; offset += spacing)
                        if (cp == 0x1FB98U)
                                Line(mask, w, h, (Point){(double)offset, 0},
                                     (Point){(double)(offset + (int)w), (double)h}, t);
                        else
                                Line(mask, w, h, (Point){(double)(offset + (int)w), 0},
                                     (Point){(double)offset, (double)h}, t);
        } else if (cp == 0x1FB9AU || cp == 0x1FB9BU) {
                EdgeTriangle(mask, w, h, cp == 0x1FB9AU ? 1U : 0U, false);
                EdgeTriangle(mask, w, h, cp == 0x1FB9AU ? 3U : 2U, false);
        } else if (cp >= 0x1FB9CU && cp <= 0x1FB9FU) {
                Triangle(mask, w, h, cp - 0x1FB9CU, false, false, t, 1);
        } else if (cp >= 0x1FBA0U && cp <= 0x1FBAEU) {
                unsigned int bits = cp - 0x1FB9FU;
                const Point from[] = {
                    {w / 2.0, 0}, {w / 2.0, 0}, {w / 2.0, (double)h}, {w / 2.0, (double)h}};
                const Point to[] = {
                    {0, h / 2.0}, {(double)w, h / 2.0}, {0, h / 2.0}, {(double)w, h / 2.0}};
                unsigned int i;

                for (i = 0; i < 4U; ++i)
                        if ((bits >> i) & 1U)
                                Line(mask, w, h, from[i], to[i], t);
        } else if (cp == 0x1FBAFU) {
                Line(mask, w, h, (Point){0, h / 2.0}, (Point){(double)w, h / 2.0}, t);
                Line(mask, w, h, (Point){w / 2.0, 0}, (Point){w / 2.0, (double)h}, 2U * t);
        } else if (cp >= 0x1FBBDU && cp <= 0x1FBBFU) {
                memset(mask, 1, (size_t)w * h);
                if (cp == 0x1FBBDU) {
                        CutLine(mask, w, h, (Point){0, 0}, (Point){(double)w, (double)h}, t);
                        CutLine(mask, w, h, (Point){(double)w, 0}, (Point){0, (double)h}, t);
                } else if (cp == 0x1FBBEU) {
                        CutLine(mask, w, h, (Point){(double)w, h / 2.0},
                                (Point){w / 2.0, (double)h}, t);
                } else {
                        CutLine(mask, w, h, (Point){w / 2.0, 0}, (Point){0, h / 2.0}, t);
                        CutLine(mask, w, h, (Point){w / 2.0, 0}, (Point){(double)w, h / 2.0}, t);
                        CutLine(mask, w, h, (Point){0, h / 2.0}, (Point){w / 2.0, (double)h}, t);
                        CutLine(mask, w, h, (Point){(double)w, h / 2.0},
                                (Point){w / 2.0, (double)h}, t);
                }
        } else if (cp == 0x1FBCEU || cp == 0x1FBCFU) {
                Rect(mask, w, h, 0, 0, Part(w, cp == 0x1FBCEU ? 2U : 1U, 3), h);
        } else if (cp >= 0x1FBD0U && cp <= 0x1FBDFU) {
                LegacyLines(cp, mask, w, h, t);
        } else if (cp >= 0x1FBE0U && cp <= 0x1FBEFU) {
                unsigned int n = cp - 0x1FBE0U;

                if (n < 4U || n >= 8U) {
                        static const double cx[] = {.5, 1, .5, 0, 1, 0, 1, 0};
                        static const double cy[] = {0, .5, 1, .5, 0, 1, 1, 0};
                        unsigned int k = n < 4U ? n : n - 8U;
                        Ellipse(mask, w, h, cx[k] * w, cy[k] * h, (w < h ? w : h) / 2.0,
                                (w < h ? w : h) / 2.0, t, n >= 8U);
                } else {
                        unsigned int k = n - 4U;
                        unsigned int x0 = k >= 2U ? (k == 3U ? 2U : 0U) : 1U;
                        unsigned int y0 = k < 2U ? (k == 1U ? 2U : 0U) : 1U;
                        QuarterRect(mask, w, h, x0, x0 + 2U, y0, y0 + 2U);
                }
        } else if (cp >= 0x1CC21U && cp <= 0x1CC2FU) {
                unsigned int gap = w / 12U;
                unsigned int bits = cp - 0x1CC20U;
                unsigned int i;

                if (gap == 0)
                        gap = 1;
                for (i = 0; i < 4U; ++i)
                        if ((bits >> i) & 1U) {
                                unsigned int col = i & 1U;
                                unsigned int row = i >> 1U;
                                unsigned int x0 = col ? w / 2U + gap : gap;
                                unsigned int x1 = col ? w - gap : w / 2U - gap;
                                unsigned int y0 = row ? h / 2U + gap : gap;
                                unsigned int y1 = row ? h - gap : h / 2U - gap;
                                Rect(mask, w, h, x0, y0, x1, y1);
                        }
        } else if (cp >= 0x1CE51U && cp <= 0x1CE8FU) {
                unsigned int bits = cp - 0x1CE50U;
                unsigned int gap = w / 12U;
                unsigned int i;

                if (gap == 0)
                        gap = 1;
                for (i = 0; i < 6U; ++i)
                        if ((bits >> i) & 1U) {
                                unsigned int col = i & 1U;
                                unsigned int row = i >> 1U;
                                Rect(mask, w, h, col ? w / 2U + gap : gap, Part(h, row, 3) + gap,
                                     col ? w - gap : w / 2U - gap, Part(h, row + 1U, 3) - gap);
                        }
        } else if (cp >= 0x1CE90U && cp <= 0x1CEAFU) {
                if (cp <= 0x1CE9FU) {
                        unsigned int n = cp - 0x1CE90U;
                        QuarterRect(mask, w, h, n & 3U, (n & 3U) + 1U, n >> 2U, (n >> 2U) + 1U);
                } else {
                        static const uint8_t bounds[16][4] = {
                            {2, 4, 3, 4}, {1, 4, 3, 4}, {0, 3, 3, 4}, {0, 2, 3, 4},
                            {0, 1, 2, 4}, {0, 1, 1, 4}, {0, 1, 0, 3}, {0, 1, 0, 2},
                            {0, 2, 0, 1}, {0, 3, 0, 1}, {1, 4, 0, 1}, {2, 4, 0, 1},
                            {3, 4, 0, 2}, {3, 4, 0, 3}, {3, 4, 1, 4}, {3, 4, 2, 4},
                        };
                        const uint8_t *b = bounds[cp - 0x1CEA0U];
                        QuarterRect(mask, w, h, b[0], b[1], b[2], b[3]);
                }
        } else if (cp == 0x1CE00U || cp == 0x1CE01U) {
                double radius = (w < h ? w : h) / 2.0;

                if (cp == 0x1CE00U) {
                        Ellipse(mask, w, h, 0, h / 2.0, radius, radius, t, false);
                        Ellipse(mask, w, h, (double)w, h / 2.0, radius, radius, t, false);
                } else {
                        Ellipse(mask, w, h, w / 2.0, 0, radius, radius, t, false);
                        Ellipse(mask, w, h, w / 2.0, (double)h, radius, radius, t, false);
                }
        } else if (cp == 0x1CE0BU || cp == 0x1CE0CU) {
                Ellipse(mask, w, h, cp == 0x1CE0BU ? (double)w : 0, h / 2.0, w, h / 2.0, t, false);
        } else if (cp == 0xE0D2U || cp == 0xE0D4U) {
                Point top[] = {
                    {0, 0}, {(double)w, 0}, {w / 2.0, h / 2.0 - t / 2.0}, {0, h / 2.0 - t / 2.0}};
                Point bottom[] = {{0, (double)h},
                                  {(double)w, (double)h},
                                  {w / 2.0, h / 2.0 + t / 2.0},
                                  {0, h / 2.0 + t / 2.0}};
                unsigned int x;
                unsigned int y;

                Polygon(mask, w, h, top, 4, false, 0);
                Polygon(mask, w, h, bottom, 4, false, 0);
                if (cp == 0xE0D4U)
                        for (y = 0; y < h; ++y)
                                for (x = 0; x < w / 2U; ++x) {
                                        uint8_t swap = mask[(size_t)y * w + x];
                                        mask[(size_t)y * w + x] = mask[(size_t)y * w + w - 1U - x];
                                        mask[(size_t)y * w + w - 1U - x] = swap;
                                }
        } else if (cp >= 0x25E2U && cp <= 0x25E5U) {
                static const uint8_t corners[] = {2, 3, 0, 1};
                Triangle(mask, w, h, corners[cp - 0x25E2U], false, false, t, 0);
        } else if ((cp >= 0x25F8U && cp <= 0x25FAU) || cp == 0x25FFU) {
                unsigned int corner = cp == 0x25FFU ? 2U : cp - 0x25F8U;
                Triangle(mask, w, h, corner, false, true, t, 0);
        } else if (cp >= 0x23BAU && cp <= 0x23BDU) {
                static const unsigned int tenths[] = {1, 3, 7, 9};
                unsigned int center = Part(h, tenths[cp - 0x23BAU], 10);
                Rect(mask, w, h, 0, center > t / 2U ? center - t / 2U : 0, w,
                     center + (t + 1U) / 2U);
        } else if ((cp >= 0x1CC1BU && cp <= 0x1CC1EU) || (cp >= 0x1CE16U && cp <= 0x1CE19U)) {
                bool vertical = cp >= 0x1CE16U;
                unsigned int n = vertical ? cp - 0x1CE16U : cp - 0x1CC1BU;

                if (vertical) {
                        Rect(mask, w, h, w / 2U - (w / 2U >= t / 2U ? t / 2U : 0), 0,
                             w / 2U + (t + 1U) / 2U, h);
                        Rect(mask, w, h, n >= 2U ? 0 : w / 2U, (n & 1U) ? h - t : 0,
                             n >= 2U ? w / 2U : w, (n & 1U) ? h : t);
                } else {
                        Rect(mask, w, h, 0, h / 2U - (h / 2U >= t / 2U ? t / 2U : 0), w,
                             h / 2U + (t + 1U) / 2U);
                        Rect(mask, w, h, n >= 2U ? 0 : w - t, (n & 1U) ? h / 2U : 0,
                             n >= 2U ? t : w, (n & 1U) ? h : h / 2U);
                }
        } else if (cp >= 0x1CC30U && cp <= 0x1CC3FU) {
                static const struct
                {
                        uint8_t x, y, sx, sy, corner;
                } pieces[16] = {{0, 0, 2, 2, 0}, {1, 0, 2, 2, 0}, {2, 0, 2, 2, 1}, {3, 0, 2, 2, 1},
                                {0, 1, 2, 2, 0}, {0, 0, 1, 1, 0}, {1, 0, 1, 1, 1}, {3, 1, 2, 2, 1},
                                {0, 2, 2, 2, 2}, {0, 1, 1, 1, 2}, {1, 1, 1, 1, 3}, {3, 2, 2, 2, 3},
                                {0, 3, 2, 2, 2}, {1, 3, 2, 2, 2}, {2, 3, 2, 2, 3}, {3, 3, 2, 2, 3}};
                unsigned int n = cp - 0x1CC30U;

                CirclePiece(mask, w, h, pieces[n].x, pieces[n].y, pieces[n].sx, pieces[n].sy,
                            pieces[n].corner, t);
        }
        return true;
}
