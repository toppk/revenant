#ifndef XTERM_PLUS_X11_OPACITY_H
#define XTERM_PLUS_X11_OPACITY_H

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
        unsigned long mask;
        int shift;
        unsigned long red_mask;
        int red_shift;
        unsigned long green_mask;
        int green_shift;
        unsigned long blue_mask;
        int blue_shift;
} XtpX11AlphaFormat;

int XtpBackgroundOpacityParse(const char *value, uint16_t *alpha);
bool XtpX11CompositorPresent(Display *display, int screen);
bool XtpX11FindArgbVisual(Display *display, int screen, XVisualInfo *visual_info,
                          XtpX11AlphaFormat *alpha_format);
bool XtpX11VisualAlphaFormat(Display *display, Visual *visual, XtpX11AlphaFormat *alpha_format);
Pixel XtpX11PixelWithAlpha(Pixel pixel, const XtpX11AlphaFormat *format, uint16_t alpha);
Pixel XtpX11OpaquePixel(Pixel pixel, const XtpX11AlphaFormat *format);
uint16_t XtpX11PixelAlpha(Pixel pixel, const XtpX11AlphaFormat *format);

#endif
