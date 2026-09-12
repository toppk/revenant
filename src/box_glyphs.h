#ifndef XTERM_PLUS_BOX_GLYPHS_H
#define XTERM_PLUS_BOX_GLYPHS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
        unsigned int x;
        unsigned int y;
        unsigned int width;
        unsigned int height;
} XtpBoxRect;

typedef enum
{
        XTP_BOX_SHADE_NONE,
        XTP_BOX_SHADE_LIGHT,
        XTP_BOX_SHADE_MEDIUM,
        XTP_BOX_SHADE_DARK
} XtpBoxShade;

typedef struct
{
        XtpBoxRect *rects;
        size_t count;
        size_t capacity;
        bool failed;
        XtpBoxShade shade;
} XtpBoxGlyph;

typedef void *(*XtpBoxGlyphRealloc)(void *pointer, size_t size);

/* NULL restores realloc; the self-test injects allocation failures. */
void XtpBoxGlyphSetAllocator(XtpBoxGlyphRealloc allocator);
bool XtpBoxGlyphCodepoint(uint32_t codepoint);
bool XtpBoxGlyphText(const char *text, size_t length, uint32_t *codepoint_out);
unsigned int XtpBoxGlyphThickness(unsigned int width, unsigned int height, bool bold);
bool XtpBoxGlyphPlan(uint32_t codepoint, unsigned int width, unsigned int height, bool bold,
                     XtpBoxGlyph *glyph);
void XtpBoxGlyphFree(XtpBoxGlyph *glyph);
bool XtpBoxShadePixel(XtpBoxShade shade, unsigned int x, unsigned int y);
void XtpBoxGlyphRasterize(const XtpBoxGlyph *glyph, unsigned int width, unsigned int height,
                          uint8_t *mask);

#endif
