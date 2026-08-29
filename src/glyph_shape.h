#ifndef XTERM_PLUS_GLYPH_SHAPE_H
#define XTERM_PLUS_GLYPH_SHAPE_H

#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>

#include <stdint.h>

#define XTP_GLYPH_RUN_CAPACITY 64

typedef struct
{
        unsigned long index;
        int32_t x_advance;
        int32_t y_advance;
        int32_t x_offset;
        int32_t y_offset;
        uint32_t cluster;
} XtpShapedGlyph;

typedef struct
{
        XtpShapedGlyph glyphs[XTP_GLYPH_RUN_CAPACITY];
        unsigned int count;
        unsigned int units_per_em;
        double x_pixel_scale;
        double y_pixel_scale;
        Boolean missing;
} XtpGlyphRun;

typedef struct XtpShaper XtpShaper;

XtpShaper *XtpShaperCreate(void);
void XtpShaperDestroy(XtpShaper *shaper);
Boolean XtpShapeUtf8(XtpShaper *shaper, XftFont *font, const char *text, size_t length,
                     XtpGlyphRun *run);
Boolean XtpShapeUtf8ForComposition(XtpShaper *shaper, XftFont *font, const char *text,
                                   size_t length, XtpGlyphRun *run);

#endif
