#include <X11/Xlib.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int
ParseCoordinate(const char *text, unsigned long *value)
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
        unsigned long values[5];
        XEvent event = {0};
        int index;

        if (argc != 6) {
                fprintf(stderr, "usage: %s WINDOW-ID START-X START-Y END-X END-Y\n", argv[0]);
                return EXIT_FAILURE;
        }
        for (index = 0; index < 5; ++index) {
                if (ParseCoordinate(argv[index + 1], &values[index]) != 0 ||
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

        event.xbutton.type = ButtonPress;
        event.xbutton.display = display;
        event.xbutton.window = target;
        event.xbutton.root = DefaultRootWindow(display);
        event.xbutton.time = CurrentTime;
        event.xbutton.x = (int)values[1];
        event.xbutton.y = (int)values[2];
        event.xbutton.x_root = event.xbutton.x;
        event.xbutton.y_root = event.xbutton.y;
        event.xbutton.same_screen = True;
        event.xbutton.button = Button1;
        (void)XSendEvent(display, target, True, ButtonPressMask, &event);

        event.xmotion.type = MotionNotify;
        event.xmotion.state = Button1Mask;
        event.xmotion.x = (int)values[3];
        event.xmotion.y = (int)values[4];
        event.xmotion.x_root = event.xmotion.x;
        event.xmotion.y_root = event.xmotion.y;
        (void)XSendEvent(display, target, True, ButtonMotionMask, &event);

        event.xbutton.type = ButtonRelease;
        event.xbutton.state = Button1Mask;
        event.xbutton.x = (int)values[3];
        event.xbutton.y = (int)values[4];
        event.xbutton.x_root = event.xbutton.x;
        event.xbutton.y_root = event.xbutton.y;
        (void)XSendEvent(display, target, True, ButtonReleaseMask, &event);
        XSync(display, False);
        printf("dragged selection in 0x%lx from %lu,%lu to %lu,%lu\n", target, values[1], values[2],
               values[3], values[4]);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
