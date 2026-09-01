#ifndef XTERM_PLUS_FONT_ROLE_H
#define XTERM_PLUS_FONT_ROLE_H

#include "glyph_shape.h"

#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>

unsigned int XtpFontStyleIndex(Boolean bold, Boolean italic);
const char *XtpFontStyleName(Boolean bold, Boolean italic);
const char *XtpFontPatternFamily(const FcPattern *pattern);
Boolean XtpFontSameFamily(const FcPattern *left, const FcPattern *right);
Boolean XtpFontStyleIsReal(XftFont *normal, XftFont *font, Boolean bold, Boolean italic);
XftFont *XtpFontRoleSelect(XftFont *normal, XftFont *bold_font, XftFont *italic_font,
                           XftFont *bold_italic_font, Boolean bold, Boolean italic);
const char *XtpFontSlantName(XftFont *font);
const char *XtpFontFileName(XftFont *font);
int XtpFontCollectionIndex(XftFont *font);
Boolean XtpGlyphRunIsPositioned(const XtpGlyphRun *run);

#endif
