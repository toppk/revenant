#include <X11/Xlib.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
ParseUnsignedLong(const char *text, unsigned long *value)
{
        char *end = NULL;
        unsigned long parsed;

        errno = 0;
        parsed = strtoul(text, &end, 0);
        if (errno != 0 || text == end || end == NULL || *end != '\0')
                return -1;
        *value = parsed;
        return 0;
}

int
main(int argc, char **argv)
{
        Display *display;
        Window target;
        Window root;
        Window parent;
        Window *children = NULL;
        unsigned int child_count = 0;
        unsigned long target_value;
        unsigned long count_value = 1;
        unsigned int button = Button4;
        unsigned long tick;

        if (argc < 2 || argc > 4) {
                fprintf(stderr, "usage: %s WINDOW-ID [up|down] [COUNT]\n", argv[0]);
                return EXIT_FAILURE;
        }
        if (ParseUnsignedLong(argv[1], &target_value) != 0 || target_value == 0) {
                fprintf(stderr, "%s: invalid X11 window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        if (argc >= 3) {
                if (strcmp(argv[2], "down") == 0)
                        button = Button5;
                else if (strcmp(argv[2], "up") != 0) {
                        fprintf(stderr, "%s: direction must be up or down\n", argv[0]);
                        return EXIT_FAILURE;
                }
        }
        if (argc == 4 && (ParseUnsignedLong(argv[3], &count_value) != 0 || count_value == 0 ||
                          count_value > UINT_MAX)) {
                fprintf(stderr, "%s: invalid event count: %s\n", argv[0], argv[3]);
                return EXIT_FAILURE;
        }

        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        target = (Window)target_value;
        if (XQueryTree(display, target, &root, &parent, &children, &child_count) == 0) {
                fprintf(stderr, "%s: cannot query window 0x%lx\n", argv[0], target);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (child_count != 0)
                target = children[0];
        if (children != NULL)
                XFree(children);

        for (tick = 0; tick < count_value; ++tick) {
                XEvent event = {0};

                event.xbutton.display = display;
                event.xbutton.window = target;
                event.xbutton.root = DefaultRootWindow(display);
                event.xbutton.subwindow = None;
                event.xbutton.time = CurrentTime;
                event.xbutton.x = 20;
                event.xbutton.y = 20;
                event.xbutton.x_root = 20;
                event.xbutton.y_root = 20;
                event.xbutton.same_screen = True;
                event.xbutton.button = button;
                event.xbutton.type = ButtonPress;
                (void)XSendEvent(display, target, True, ButtonPressMask, &event);
                event.xbutton.type = ButtonRelease;
                (void)XSendEvent(display, target, True, ButtonReleaseMask, &event);
                XSync(display, False);
        }

        printf("sent %lu wheel-%s tick%s to 0x%lx\n", count_value,
               button == Button4 ? "up" : "down", count_value == 1 ? "" : "s", target);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
