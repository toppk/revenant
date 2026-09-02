#ifndef XTERM_PLUS_FONT_ROUTER_H
#define XTERM_PLUS_FONT_ROUTER_H

#include "font_universe.h"

typedef struct _Vt100Rec Vt100Rec;

XftFont *VtSelectXftFont(Vt100Rec *vt, const char *text, size_t length, unsigned int width,
                         Boolean bold, Boolean italic, const char **role_name, uint32_t *base_out,
                         XtpEmojiStyle *style_out, XtpGlyphRun *run_out);

#endif
