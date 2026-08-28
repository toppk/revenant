#include "x11_opacity.h"

#include <X11/extensions/Xrender.h>

#include <ctype.h>
#include <stdio.h>

int
XtpBackgroundOpacityParse(const char *value, uint16_t *alpha)
{
        const char *cursor;
        double opacity;
        double place = 0.1;
        bool digits = false;
        bool one = false;

        if (alpha == NULL)
                return -1;
        if (value == NULL) {
                *alpha = UINT16_MAX;
                return 0;
        }
        while (isspace((unsigned char)*value))
                ++value;
        if (*value == '\0')
                return -1;
        cursor = value;
        if (*cursor == '+')
                ++cursor;
        if (*cursor == '0' || *cursor == '1') {
                opacity = (double)(*cursor - '0');
                one = *cursor == '1';
                digits = true;
                ++cursor;
        } else {
                opacity = 0.0;
        }
        if (*cursor == '.') {
                ++cursor;
                while (isdigit((unsigned char)*cursor)) {
                        if (one && *cursor != '0')
                                return -1;
                        opacity += (double)(*cursor - '0') * place;
                        place *= 0.1;
                        digits = true;
                        ++cursor;
                }
        }
        if (!digits || opacity > 1.0)
                return -1;
        while (isspace((unsigned char)*cursor))
                ++cursor;
        if (*cursor != '\0')
                return -1;
        *alpha = (uint16_t)(opacity * UINT16_MAX + 0.5);
        return 0;
}

bool
XtpX11CompositorPresent(Display *display, int screen)
{
        char name[64];
        Atom selection;

        if (display == NULL || screen < 0 || screen >= ScreenCount(display))
                return false;
        (void)snprintf(name, sizeof(name), "_NET_WM_CM_S%d", screen);
        selection = XInternAtom(display, name, False);
        return XGetSelectionOwner(display, selection) != None;
}

bool
XtpX11VisualAlphaFormat(Display *display, Visual *visual, XtpX11AlphaFormat *alpha_format)
{
        XRenderPictFormat *format;

        if (display == NULL || visual == NULL || alpha_format == NULL)
                return false;
        format = XRenderFindVisualFormat(display, visual);
        if (format == NULL || format->type != PictTypeDirect || format->direct.alphaMask == 0)
                return false;
        alpha_format->mask = (unsigned long)format->direct.alphaMask;
        alpha_format->shift = format->direct.alpha;
        alpha_format->red_mask = (unsigned long)format->direct.redMask;
        alpha_format->red_shift = format->direct.red;
        alpha_format->green_mask = (unsigned long)format->direct.greenMask;
        alpha_format->green_shift = format->direct.green;
        alpha_format->blue_mask = (unsigned long)format->direct.blueMask;
        alpha_format->blue_shift = format->direct.blue;
        return true;
}

bool
XtpX11FindArgbVisual(Display *display, int screen, XVisualInfo *visual_info,
                     XtpX11AlphaFormat *alpha_format)
{
        XVisualInfo request = {0};
        XVisualInfo *visuals;
        int count;
        int index;
        bool found = false;

        if (display == NULL || visual_info == NULL || alpha_format == NULL)
                return false;
        request.screen = screen;
        request.depth = 32;
        request.class = TrueColor;
        visuals = XGetVisualInfo(display, VisualScreenMask | VisualDepthMask | VisualClassMask,
                                 &request, &count);
        if (visuals == NULL)
                return false;
        for (index = 0; index < count; ++index) {
                if (XtpX11VisualAlphaFormat(display, visuals[index].visual, alpha_format)) {
                        *visual_info = visuals[index];
                        found = true;
                        break;
                }
        }
        XFree(visuals);
        return found;
}

Pixel
XtpX11PixelWithAlpha(Pixel pixel, const XtpX11AlphaFormat *format, uint16_t alpha)
{
        const unsigned long masks[] = {format != NULL ? format->red_mask : 0,
                                       format != NULL ? format->green_mask : 0,
                                       format != NULL ? format->blue_mask : 0};
        const int shifts[] = {format != NULL ? format->red_shift : 0,
                              format != NULL ? format->green_shift : 0,
                              format != NULL ? format->blue_shift : 0};
        unsigned long alpha_value;
        size_t index;

        if (format == NULL || format->mask == 0)
                return pixel;
        alpha_value = (unsigned long)(((unsigned long long)alpha * format->mask + UINT16_MAX / 2U) /
                                      UINT16_MAX);
        pixel = (pixel & ~(format->mask << format->shift)) |
                ((alpha_value & format->mask) << format->shift);
        for (index = 0; index < sizeof(masks) / sizeof(masks[0]); ++index) {
                unsigned long component;

                if (masks[index] == 0)
                        continue;
                component = (pixel >> shifts[index]) & masks[index];
                component = (unsigned long)(((unsigned long long)component * alpha_value +
                                             format->mask / 2U) /
                                            format->mask);
                pixel = (pixel & ~(masks[index] << shifts[index])) |
                        ((component & masks[index]) << shifts[index]);
        }
        return pixel;
}

Pixel
XtpX11OpaquePixel(Pixel pixel, const XtpX11AlphaFormat *format)
{
        const unsigned long masks[] = {format != NULL ? format->red_mask : 0,
                                       format != NULL ? format->green_mask : 0,
                                       format != NULL ? format->blue_mask : 0};
        const int shifts[] = {format != NULL ? format->red_shift : 0,
                              format != NULL ? format->green_shift : 0,
                              format != NULL ? format->blue_shift : 0};
        unsigned long alpha_value;
        size_t index;

        if (format == NULL || format->mask == 0)
                return pixel;
        alpha_value = (pixel >> format->shift) & format->mask;
        if (alpha_value != 0 && alpha_value != format->mask) {
                for (index = 0; index < sizeof(masks) / sizeof(masks[0]); ++index) {
                        unsigned long component;

                        if (masks[index] == 0)
                                continue;
                        component = (pixel >> shifts[index]) & masks[index];
                        component = (unsigned long)(((unsigned long long)component * format->mask +
                                                     alpha_value / 2U) /
                                                    alpha_value);
                        if (component > masks[index])
                                component = masks[index];
                        pixel = (pixel & ~(masks[index] << shifts[index])) |
                                (component << shifts[index]);
                }
        }
        return (pixel & ~(format->mask << format->shift)) | (format->mask << format->shift);
}

uint16_t
XtpX11PixelAlpha(Pixel pixel, const XtpX11AlphaFormat *format)
{
        unsigned long value;

        if (format == NULL || format->mask == 0)
                return UINT16_MAX;
        value = (pixel >> format->shift) & format->mask;
        return (uint16_t)((value * UINT16_MAX + format->mask / 2U) / format->mask);
}
