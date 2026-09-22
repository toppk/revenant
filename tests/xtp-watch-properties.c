#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prints one line per PropertyNotify on WINDOW: "ATOM new VALUE" or "ATOM deleted". */
int
main(int argc, char **argv)
{
        Display *display;
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
        XSelectInput(display, window, PropertyChangeMask | StructureNotifyMask);
        XSync(display, False);
        setvbuf(stdout, NULL, _IOLBF, 0);
        printf("ready\n");
        for (;;) {
                XEvent event;
                char *name;

                XNextEvent(display, &event);
                if (event.type == DestroyNotify)
                        break;
                if (event.type != PropertyNotify)
                        continue;
                name = XGetAtomName(display, event.xproperty.atom);
                if (event.xproperty.state == PropertyDelete) {
                        printf("%s deleted\n", name != NULL ? name : "?");
                } else {
                        Atom type = None;
                        int format = 0;
                        unsigned long count = 0;
                        unsigned long after = 0;
                        unsigned char *value = NULL;

                        if (XGetWindowProperty(display, window, event.xproperty.atom, 0, 1024,
                                               False, AnyPropertyType, &type, &format, &count,
                                               &after, &value) == Success &&
                            format == 8 && value != NULL)
                                printf("%s new %.*s\n", name != NULL ? name : "?", (int)count,
                                       (const char *)value);
                        else
                                printf("%s new\n", name != NULL ? name : "?");
                        if (value != NULL)
                                XFree(value);
                }
                if (name != NULL)
                        XFree(name);
        }
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
