#ifndef XTERM_PLUS_GLYPH_CAIRO_H
#define XTERM_PLUS_GLYPH_CAIRO_H

#include "glyph_shape.h"

#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>

#include <stdint.h>

typedef struct XtpCairo XtpCairo;

Boolean XtpCairoFontIsColor(const XftFont *font);
XtpCairo *XtpCairoCreate(Display *display, Drawable drawable, Visual *visual, int width,
                         int height);
void XtpCairoResize(XtpCairo *renderer, int width, int height);
void XtpCairoDestroy(XtpCairo *renderer);
Boolean XtpCairoGlyphRunHasInk(XtpCairo *renderer, XftFont *font, const XtpGlyphRun *run,
                               Boolean color_glyphs, unsigned int available_width,
                               unsigned int cell_height);
Boolean XtpCairoDrawGlyphRun(XtpCairo *renderer, XftFont *font, const XtpGlyphRun *run,
                             Boolean color_glyphs, const XRenderColor *foreground,
                             const XRectangle *area, const XRectangle *clip);

#endif
