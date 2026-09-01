#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Window
FirstChild(Display *display, Window window)
{
        Window root;
        Window parent;
        Window *children = NULL;
        unsigned int count = 0;
        Window result = window;

        if (XQueryTree(display, window, &root, &parent, &children, &count) && count != 0)
                result = children[0];
        if (children != NULL)
                XFree(children);
        return result;
}

static unsigned long
ParseUnsigned(const char *value, const char *description)
{
        char *end;
        unsigned long result;

        errno = 0;
        result = strtoul(value, &end, 0);
        if (errno != 0 || end == value || *end != '\0') {
                fprintf(stderr, "invalid %s: %s\n", description, value);
                exit(EXIT_FAILURE);
        }
        return result;
}

static unsigned long
ComponentByte(unsigned long pixel, unsigned long mask, int shift)
{
        unsigned long value;

        if (mask == 0)
                return 0;
        value = (pixel >> shift) & mask;
        return (value * 255UL + mask / 2UL) / mask;
}

static int
DamageGuard(int argc, char **argv)
{
        Display *display;
        Window window;
        XWindowAttributes attributes;
        XRenderPictFormat *format;
        XImage *image;
        XColor guard_color = {0};
        XGCValues values = {0};
        GC gc;
        unsigned long parsed_window;
        unsigned long x;
        unsigned long y;
        unsigned long width;
        unsigned long height;
        unsigned long damage_width;
        unsigned long guard;
        unsigned long changed = 0;
        unsigned long image_x;
        unsigned long image_y;

        if (argc != 9) {
                fprintf(stderr,
                        "usage: %s WINDOW --damage-guard X Y WIDTH HEIGHT DAMAGE-WIDTH "
                        "0xRRGGBB\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        parsed_window = ParseUnsigned(argv[1], "window id");
        x = ParseUnsigned(argv[3], "x coordinate");
        y = ParseUnsigned(argv[4], "y coordinate");
        width = ParseUnsigned(argv[5], "width");
        height = ParseUnsigned(argv[6], "height");
        damage_width = ParseUnsigned(argv[7], "damage width");
        guard = ParseUnsigned(argv[8], "guard RGB");
        if (width == 0 || height == 0 || damage_width == 0 || damage_width >= width ||
            guard > 0xFFFFFFUL) {
                fprintf(stderr, "invalid damage-guard rectangle or color\n");
                return EXIT_FAILURE;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        window = FirstChild(display, (Window)parsed_window);
        if (!XGetWindowAttributes(display, window, &attributes) ||
            x > (unsigned long)attributes.width || width > (unsigned long)attributes.width - x ||
            y > (unsigned long)attributes.height || height > (unsigned long)attributes.height - y) {
                fprintf(stderr, "damage-guard rectangle is outside window\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        format = XRenderFindVisualFormat(display, attributes.visual);
        if (format == NULL || format->type != PictTypeDirect) {
                fprintf(stderr, "window does not use a direct-color visual\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        guard_color.red = (unsigned short)((guard >> 16) * 257UL);
        guard_color.green = (unsigned short)(((guard >> 8) & 0xFFUL) * 257UL);
        guard_color.blue = (unsigned short)((guard & 0xFFUL) * 257UL);
        guard_color.flags = DoRed | DoGreen | DoBlue;
        if (!XAllocColor(display, attributes.colormap, &guard_color)) {
                fprintf(stderr, "cannot allocate damage-guard color\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        values.foreground = guard_color.pixel;
        gc = XCreateGC(display, window, GCForeground, &values);
        XFillRectangle(display, window, gc, (int)x, (int)y, (unsigned int)width,
                       (unsigned int)height);
        XSync(display, False);
        XClearArea(display, window, (int)x, (int)y, (unsigned int)damage_width,
                   (unsigned int)height, True);
        XSync(display, False);
        {
                const struct timespec delay = {0, 200000000L};

                (void)nanosleep(&delay, NULL);
        }
        XSync(display, False);
        image = XGetImage(display, window, (int)(x + damage_width), (int)y,
                          (unsigned int)(width - damage_width), (unsigned int)height, AllPlanes,
                          ZPixmap);
        if (image == NULL) {
                fprintf(stderr, "cannot sample damage guard\n");
                XFreeGC(display, gc);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        for (image_y = 0; image_y < height; ++image_y) {
                for (image_x = 0; image_x < width - damage_width; ++image_x) {
                        unsigned long pixel = XGetPixel(image, (int)image_x, (int)image_y);
                        unsigned long red = ComponentByte(
                            pixel, (unsigned long)format->direct.redMask, format->direct.red);
                        unsigned long green = ComponentByte(
                            pixel, (unsigned long)format->direct.greenMask, format->direct.green);
                        unsigned long blue = ComponentByte(
                            pixel, (unsigned long)format->direct.blueMask, format->direct.blue);

                        if (red != (guard >> 16) || green != ((guard >> 8) & 0xFFUL) ||
                            blue != (guard & 0xFFUL))
                                ++changed;
                }
        }
        printf("guard_changed=%lu\n", changed);
        XDestroyImage(image);
        XFreeGC(display, gc);
        XFreeColors(display, attributes.colormap, &guard_color.pixel, 1, 0);
        XCloseDisplay(display);
        return changed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char **argv)
{
        Display *display;
        Window window;
        XWindowAttributes attributes;
        XRenderPictFormat *format;
        XImage *image;
        unsigned long parsed_window;
        unsigned long x;
        unsigned long y;
        unsigned long width;
        unsigned long height;
        unsigned long background;
        unsigned long background_red;
        unsigned long background_green;
        unsigned long background_blue;
        unsigned long ink = 0;
        unsigned long chromatic = 0;
        unsigned long min_x = 0;
        unsigned long min_y = 0;
        unsigned long max_x = 0;
        unsigned long max_y = 0;
        unsigned long image_x;
        unsigned long image_y;
        uint64_t hash = UINT64_C(1469598103934665603);
        const char *classification;
        int expose;

        if (argc >= 3 && strcmp(argv[2], "--damage-guard") == 0)
                return DamageGuard(argc, argv);
        expose = argc == 8 && strcmp(argv[2], "--expose") == 0;
        if (argc != 8 || (!expose && strcmp(argv[2], "--sample") != 0)) {
                fprintf(stderr, "usage: %s WINDOW --expose|--sample X Y WIDTH HEIGHT 0xRRGGBB\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        parsed_window = ParseUnsigned(argv[1], "window id");
        x = ParseUnsigned(argv[3], "x coordinate");
        y = ParseUnsigned(argv[4], "y coordinate");
        width = ParseUnsigned(argv[5], "width");
        height = ParseUnsigned(argv[6], "height");
        background = ParseUnsigned(argv[7], "background RGB");
        if (width == 0 || height == 0 || background > 0xFFFFFFUL) {
                fprintf(stderr, "invalid sample rectangle or background\n");
                return EXIT_FAILURE;
        }
        background_red = background >> 16;
        background_green = (background >> 8) & 0xFFUL;
        background_blue = background & 0xFFUL;

        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        window = FirstChild(display, (Window)parsed_window);
        if (!XGetWindowAttributes(display, window, &attributes)) {
                fprintf(stderr, "cannot query window attributes\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (x > (unsigned long)attributes.width || width > (unsigned long)attributes.width - x ||
            y > (unsigned long)attributes.height || height > (unsigned long)attributes.height - y) {
                fprintf(stderr, "sample rectangle is outside window\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        format = XRenderFindVisualFormat(display, attributes.visual);
        if (format == NULL || format->type != PictTypeDirect) {
                fprintf(stderr, "window does not use a direct-color visual\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        if (expose) {
                XClearArea(display, window, 0, 0, 0, 0, True);
                XSync(display, False);
                {
                        const struct timespec delay = {0, 200000000L};

                        (void)nanosleep(&delay, NULL);
                }
        }
        XSync(display, False);
        image = XGetImage(display, window, (int)x, (int)y, (unsigned int)width,
                          (unsigned int)height, AllPlanes, ZPixmap);
        if (image == NULL) {
                fprintf(stderr, "cannot sample window\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        for (image_y = 0; image_y < height; ++image_y) {
                for (image_x = 0; image_x < width; ++image_x) {
                        unsigned long pixel = XGetPixel(image, (int)image_x, (int)image_y);
                        unsigned long red = ComponentByte(
                            pixel, (unsigned long)format->direct.redMask, format->direct.red);
                        unsigned long green = ComponentByte(
                            pixel, (unsigned long)format->direct.greenMask, format->direct.green);
                        unsigned long blue = ComponentByte(
                            pixel, (unsigned long)format->direct.blueMask, format->direct.blue);
                        unsigned long maximum = red > green ? red : green;
                        unsigned long minimum = red < green ? red : green;

                        hash = (hash ^ red) * UINT64_C(1099511628211);
                        hash = (hash ^ green) * UINT64_C(1099511628211);
                        hash = (hash ^ blue) * UINT64_C(1099511628211);

                        maximum = maximum > blue ? maximum : blue;
                        minimum = minimum < blue ? minimum : blue;
                        if (red == background_red && green == background_green &&
                            blue == background_blue)
                                continue;
                        if (ink == 0) {
                                min_x = max_x = image_x;
                                min_y = max_y = image_y;
                        } else {
                                if (image_x < min_x)
                                        min_x = image_x;
                                if (image_x > max_x)
                                        max_x = image_x;
                                if (image_y < min_y)
                                        min_y = image_y;
                                if (image_y > max_y)
                                        max_y = image_y;
                        }
                        ++ink;
                        if (maximum - minimum >= 8)
                                ++chromatic;
                }
        }
        classification = ink == 0 ? "blank" : (chromatic == 0 ? "mono" : "color");
        if (ink == 0) {
                printf("class=%s ink=0 chromatic=0 hash=%016" PRIx64 " bounds=none\n",
                       classification, hash);
        } else {
                printf("class=%s ink=%lu chromatic=%lu hash=%016" PRIx64
                       " bounds=%lu,%lu,%lu,%lu\n",
                       classification, ink, chromatic, hash, min_x, min_y, max_x - min_x + 1,
                       max_y - min_y + 1);
        }
        XDestroyImage(image);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
