#ifndef XTERM_PLUS_VT_FONT_H
#define XTERM_PLUS_VT_FONT_H

#include "vt_widgetP.h"

Boolean VtFontRequested(const Vt100Rec *vt);
XtpEmojiPolicy VtFontParseEmojiPolicy(const char *value);
unsigned int VtBitmapFontWidth(const XFontStruct *font);
unsigned int VtBitmapFontHeight(const XFontStruct *font);
void VtFontUniverseInitialize(Vt100Rec *vt);
Boolean VtFontEnsureSlot(Vt100Rec *vt, int slot);
Boolean VtFontUniverseReload(Vt100Rec *vt);
void VtFontUniverseDestroy(Vt100Rec *vt, XtpFontUniverse *universe);
void VtFontUniverseClose(Vt100Rec *vt);
Boolean VtFontEnsureXftDraw(Vt100Rec *vt);
Boolean VtFontEnsureCairoDraw(Vt100Rec *vt);
XftFont *VtFontRoleStyle(Vt100Rec *vt, XtpFontRoleIndex role, int slot, Boolean bold,
                         Boolean italic);
void VtFontEnsureSystemFallbacks(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot,
                                 unsigned int style);
XftFont *VtOpenNormalizedXftPattern(Vt100Rec *vt, FcPattern *pattern, int slot, double *scale_out);

#endif
