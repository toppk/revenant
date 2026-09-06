#include <X11/Xlib.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
ParseNumber(const char *text, unsigned long *value)
{
        char *end = NULL;
        unsigned long parsed;

        errno = 0;
        parsed = strtoul(text, &end, 0);
        if (errno != 0 || text == end || end == NULL || *end != '\0' || parsed > INT_MAX)
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
        unsigned long values[3];
        XEvent event = {0};
        unsigned int state;
        int index;

        if (argc != 5 || (strcmp(argv[4], "plain") != 0 && strcmp(argv[4], "shift") != 0)) {
                fprintf(stderr, "usage: %s WINDOW-ID X Y plain|shift\n", argv[0]);
                return EXIT_FAILURE;
        }
        for (index = 0; index < 3; ++index) {
                if (ParseNumber(argv[index + 1], &values[index]) != 0 ||
                    (index == 0 && values[index] == 0)) {
                        fprintf(stderr, "%s: invalid numeric argument: %s\n", argv[0],
                                argv[index + 1]);
                        return EXIT_FAILURE;
                }
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        target = (Window)values[0];
        if (XQueryTree(display, target, &root, &parent, &children, &child_count) == 0) {
                fprintf(stderr, "%s: cannot query window 0x%lx\n", argv[0], target);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (child_count != 0)
                target = children[0];
        if (children != NULL)
                XFree(children);
        state = strcmp(argv[4], "shift") == 0 ? ShiftMask : 0;
        event.xmotion.type = MotionNotify;
        event.xmotion.display = display;
        event.xmotion.window = target;
        event.xmotion.root = DefaultRootWindow(display);
        event.xmotion.time = CurrentTime;
        event.xmotion.x = (int)values[1];
        event.xmotion.y = (int)values[2];
        event.xmotion.x_root = event.xmotion.x;
        event.xmotion.y_root = event.xmotion.y;
        event.xmotion.state = state;
        event.xmotion.same_screen = True;
        (void)XSendEvent(display, target, True, PointerMotionMask, &event);
        XSync(display, False);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
