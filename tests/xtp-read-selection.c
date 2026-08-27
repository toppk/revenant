#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XTP_SELECTION_TIMEOUT_MS 5000

static int
CutBufferNumber(const char *name)
{
        if (strncmp(name, "CUT_BUFFER", 10) == 0 && name[10] >= '0' && name[10] <= '7' &&
            name[11] == '\0')
                return name[10] - '0';
        return -1;
}

static int
ReadCutBuffer(Display *display, int cut_buffer)
{
        int length = 0;
        char *value = XFetchBuffer(display, &length, cut_buffer);

        if (value == NULL)
                return EXIT_FAILURE;
        if (length > 0 && fwrite(value, 1, (size_t)length, stdout) != (size_t)length) {
                XFree(value);
                return EXIT_FAILURE;
        }
        XFree(value);
        return EXIT_SUCCESS;
}

static int
ReadSelection(Display *display, const char *selection_name, const char *target_name)
{
        Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0,
                                            BlackPixel(display, DefaultScreen(display)),
                                            BlackPixel(display, DefaultScreen(display)));
        Atom selection = XInternAtom(display, selection_name, False);
        Atom target = XInternAtom(display, target_name, False);
        Atom property = XInternAtom(display, "XTP_SELECTION_RESULT", False);
        struct pollfd descriptor = {ConnectionNumber(display), POLLIN, 0};
        int result = EXIT_FAILURE;

        XConvertSelection(display, selection, target, property, window, CurrentTime);
        XFlush(display);
        for (;;) {
                XEvent event;

                while (XPending(display) != 0) {
                        XNextEvent(display, &event);
                        if (event.type == SelectionNotify && event.xselection.requestor == window) {
                                Atom actual_type = None;
                                int actual_format = 0;
                                unsigned long items = 0;
                                unsigned long after = 0;
                                unsigned char *value = NULL;

                                if (event.xselection.property == None)
                                        goto done;
                                if (XGetWindowProperty(display, window, property, 0, 1L << 24, True,
                                                       AnyPropertyType, &actual_type,
                                                       &actual_format, &items, &after,
                                                       &value) != Success ||
                                    actual_type == None || actual_format != 8 || after != 0)
                                        goto done;
                                if (items != 0 && fwrite(value, 1, items, stdout) != items) {
                                        XFree(value);
                                        goto done;
                                }
                                if (value != NULL)
                                        XFree(value);
                                result = EXIT_SUCCESS;
                                goto done;
                        }
                }
                if (poll(&descriptor, 1, XTP_SELECTION_TIMEOUT_MS) <= 0)
                        goto done;
        }

done:
        XDestroyWindow(display, window);
        return result;
}

int
main(int argc, char **argv)
{
        Display *display;
        int cut_buffer;
        int result;

        if (argc < 2 || argc > 3) {
                fprintf(stderr, "usage: %s SELECTION [TARGET]\n", argv[0]);
                return EXIT_FAILURE;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        cut_buffer = CutBufferNumber(argv[1]);
        if (cut_buffer >= 0)
                result = ReadCutBuffer(display, cut_buffer);
        else
                result = ReadSelection(display, argv[1], argc == 3 ? argv[2] : "UTF8_STRING");
        XCloseDisplay(display);
        return result;
}
