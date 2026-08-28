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

        if (argc != 3 || strcmp(argv[2], "--expose") != 0) {
                fprintf(stderr, "usage: %s WINDOW --expose\n", argv[0]);
                return EXIT_FAILURE;
        }
        errno = 0;
        parsed = strtoul(argv[1], &end, 0);
        if (errno != 0 || end == argv[1] || *end != '\0') {
                fprintf(stderr, "invalid window id: %s\n", argv[1]);
                return EXIT_FAILURE;
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
        if (format == NULL || format->type != PictTypeDirect || format->direct.alphaMask == 0) {
                fprintf(stderr, "window does not use an alpha visual\n");
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
        image = XGetImage(display, window, attributes.width - 2, attributes.height - 2, 1, 1,
                          AllPlanes, ZPixmap);
        if (image == NULL) {
                fprintf(stderr, "cannot sample window\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        pixel = XGetPixel(image, 0, 0);
        alpha = (pixel >> format->direct.alpha) & format->direct.alphaMask;
        printf("%lu\n",
               (alpha * 65535UL + format->direct.alphaMask / 2UL) / format->direct.alphaMask);
        XDestroyImage(image);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
