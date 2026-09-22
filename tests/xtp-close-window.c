#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* Sends the WM_DELETE_WINDOW client message a window manager sends when a user closes WINDOW. */
int
main(int argc, char **argv)
{
        Display *display;
        XEvent event = {0};
        Window window;
        char *end = NULL;

        if (argc != 2) {
                fprintf(stderr, "usage: %s WINDOW-ID\n", argv[0]);
                return EXIT_FAILURE;
        }
        errno = 0;
        window = (Window)strtoul(argv[1], &end, 0);
        if (errno != 0 || end == argv[1] || *end != '\0' || window == None) {
                fprintf(stderr, "%s: invalid window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = (long)XInternAtom(display, "WM_DELETE_WINDOW", False);
        event.xclient.data.l[1] = CurrentTime;
        (void)XSendEvent(display, window, False, NoEventMask, &event);
        XSync(display, False);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
