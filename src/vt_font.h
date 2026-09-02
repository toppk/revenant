#ifndef XTERM_PLUS_VT_FONT_H
#define XTERM_PLUS_VT_FONT_H

#include "font_universe.h"

typedef struct _Vt100Rec Vt100Rec;

unsigned int VtBitmapFontWidth(const XFontStruct *font);
unsigned int VtBitmapFontHeight(const XFontStruct *font);
unsigned int VtSlotWidth(const Vt100Rec *vt, int slot);
unsigned int VtSlotHeight(const Vt100Rec *vt, int slot);
int VtSlotAscent(const Vt100Rec *vt, int slot);
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
/* Consumes pattern on every path, including open failure. */
XftFont *VtOpenNormalizedXftPattern(Vt100Rec *vt, FcPattern *pattern, int slot, double *scale_out);

#endif
