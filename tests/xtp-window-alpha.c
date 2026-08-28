#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

#include <errno.h>
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
ComponentByte(unsigned long pixel, unsigned long mask, int shift)
{
        unsigned long value;

        if (mask == 0)
                return 0;
        value = (pixel >> shift) & mask;
        return (value * 255UL + mask / 2UL) / mask;
}

int
main(int argc, char **argv)
{
        char *end;
        unsigned long parsed;
        Display *display;
        Window window;
        XWindowAttributes attributes;
        XRenderPictFormat *format;
        XImage *image;
        unsigned long pixel;
        unsigned long alpha;
        long sample_x = -1;
        long sample_y = -1;
        int print_argb;

        print_argb = (argc == 4 || argc == 6) && strcmp(argv[3], "--argb") == 0;
        if ((argc != 3 && !print_argb) || strcmp(argv[2], "--expose") != 0) {
                fprintf(stderr, "usage: %s WINDOW --expose [--argb [X Y]]\n", argv[0]);
                return EXIT_FAILURE;
        }
        errno = 0;
        parsed = strtoul(argv[1], &end, 0);
        if (errno != 0 || end == argv[1] || *end != '\0') {
                fprintf(stderr, "invalid window id: %s\n", argv[1]);
                return EXIT_FAILURE;
        }
        if (argc == 6) {
                errno = 0;
                sample_x = strtol(argv[4], &end, 10);
                if (errno != 0 || end == argv[4] || *end != '\0' || sample_x < 0) {
                        fprintf(stderr, "invalid sample x: %s\n", argv[4]);
                        return EXIT_FAILURE;
                }
                errno = 0;
                sample_y = strtol(argv[5], &end, 10);
                if (errno != 0 || end == argv[5] || *end != '\0' || sample_y < 0) {
                        fprintf(stderr, "invalid sample y: %s\n", argv[5]);
                        return EXIT_FAILURE;
                }
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        window = FirstChild(display, (Window)parsed);
        if (!XGetWindowAttributes(display, window, &attributes)) {
                fprintf(stderr, "cannot query window attributes\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        format = XRenderFindVisualFormat(display, attributes.visual);
        if (format == NULL || format->type != PictTypeDirect ||
            (!print_argb && format->direct.alphaMask == 0)) {
                fprintf(stderr, "window does not use a compatible visual\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (sample_x < 0)
                sample_x = attributes.width - 2;
        if (sample_y < 0)
                sample_y = attributes.height - 2;
        if (sample_x >= attributes.width || sample_y >= attributes.height) {
                fprintf(stderr, "sample coordinate outside window: %ld,%ld\n", sample_x, sample_y);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        XClearArea(display, window, 0, 0, 0, 0, True);
        XSync(display, False);
        {
                const struct timespec delay = {0, 200000000L};

                (void)nanosleep(&delay, NULL);
        }
        XSync(display, False);
        image = XGetImage(display, window, (int)sample_x, (int)sample_y, 1, 1, AllPlanes, ZPixmap);
        if (image == NULL) {
                fprintf(stderr, "cannot sample window\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        pixel = XGetPixel(image, 0, 0);
        alpha = format->direct.alphaMask != 0
                    ? ComponentByte(pixel, (unsigned long)format->direct.alphaMask,
                                    format->direct.alpha)
                    : 255UL;
        if (print_argb) {
                unsigned long red =
                    ComponentByte(pixel, (unsigned long)format->direct.redMask, format->direct.red);
                unsigned long green = ComponentByte(pixel, (unsigned long)format->direct.greenMask,
                                                    format->direct.green);
                unsigned long blue = ComponentByte(pixel, (unsigned long)format->direct.blueMask,
                                                   format->direct.blue);

                printf("0x%02lx%02lx%02lx%02lx\n", alpha, red, green, blue);
        } else {
                printf("%lu\n", (alpha * 65535UL + 127UL) / 255UL);
        }
        XDestroyImage(image);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
