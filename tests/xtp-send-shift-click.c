#include <X11/Xlib.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

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
        int index;

        if (argc != 4) {
                fprintf(stderr, "usage: %s WINDOW-ID X Y\n", argv[0]);
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

        event.xmotion.type = MotionNotify;
        event.xmotion.display = display;
        event.xmotion.window = target;
        event.xmotion.root = DefaultRootWindow(display);
        event.xmotion.time = CurrentTime;
        event.xmotion.x = (int)values[1];
        event.xmotion.y = (int)values[2];
        event.xmotion.x_root = event.xmotion.x;
        event.xmotion.y_root = event.xmotion.y;
        event.xmotion.state = ShiftMask;
        event.xmotion.same_screen = True;
        (void)XSendEvent(display, target, True, PointerMotionMask, &event);

        event.xbutton.type = ButtonPress;
        event.xbutton.button = Button1;
        event.xbutton.state = ShiftMask;
        (void)XSendEvent(display, target, True, ButtonPressMask, &event);

        event.xbutton.type = ButtonRelease;
        event.xbutton.state = ShiftMask | Button1Mask;
        (void)XSendEvent(display, target, True, ButtonReleaseMask, &event);
        XSync(display, False);
        printf("shift-clicked 0x%lx at %lu,%lu\n", target, values[1], values[2]);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
