#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
ParseWindow(const char *text, Window *window)
{
        char *end = NULL;
        unsigned long value;

        errno = 0;
        value = strtoul(text, &end, 0);
        if (errno != 0 || text == end || end == NULL || *end != '\0' || value == 0)
                return -1;
        *window = (Window)value;
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
        KeySym keysym;
        unsigned int state;
        KeyCode keycode;
        XEvent event = {0};

        if (argc != 3 ||
            (strcmp(argv[2], "ctrl-i") != 0 && strcmp(argv[2], "tab") != 0 &&
             strcmp(argv[2], "a-cycle") != 0 && strcmp(argv[2], "shift-a-cycle") != 0 &&
             strcmp(argv[2], "ctrl-cycle") != 0 && strcmp(argv[2], "up-cycle") != 0 &&
             strcmp(argv[2], "shift-insert-cycle") != 0)) {
                fprintf(stderr,
                        "usage: %s WINDOW-ID "
                        "{ctrl-i|tab|a-cycle|shift-a-cycle|ctrl-cycle|up-cycle|"
                        "shift-insert-cycle}\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        if (ParseWindow(argv[1], &target) != 0) {
                fprintf(stderr, "%s: invalid X11 window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        if (strcmp(argv[2], "ctrl-i") == 0) {
                keysym = XK_i;
                state = ControlMask;
        } else if (strcmp(argv[2], "a-cycle") == 0) {
                keysym = XK_a;
                state = 0;
        } else if (strcmp(argv[2], "shift-a-cycle") == 0) {
                keysym = XK_a;
                state = ShiftMask;
        } else if (strcmp(argv[2], "ctrl-cycle") == 0) {
                keysym = XK_Control_L;
                state = ControlMask;
        } else if (strcmp(argv[2], "up-cycle") == 0) {
                keysym = XK_Up;
                state = 0;
        } else if (strcmp(argv[2], "shift-insert-cycle") == 0) {
                keysym = XK_Insert;
                state = ShiftMask;
        } else {
                keysym = XK_Tab;
                state = 0;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        if (XQueryTree(display, target, &root, &parent, &children, &child_count) == 0) {
                fprintf(stderr, "%s: cannot query window 0x%lx\n", argv[0], target);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (child_count != 0)
                target = children[0];
        if (children != NULL)
                XFree(children);
        keycode = XKeysymToKeycode(display, keysym);
        if (keycode == 0) {
                fprintf(stderr, "%s: display has no keycode for %s\n", argv[0], argv[2]);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        event.xkey.display = display;
        event.xkey.window = target;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.time = CurrentTime;
        event.xkey.x = 1;
        event.xkey.y = 1;
        event.xkey.x_root = 1;
        event.xkey.y_root = 1;
        event.xkey.state = state;
        event.xkey.keycode = keycode;
        event.xkey.same_screen = True;
        event.xkey.type = KeyPress;
        (void)XSendEvent(display, target, True, KeyPressMask, &event);
        if (strstr(argv[2], "-cycle") != NULL)
                (void)XSendEvent(display, target, True, KeyPressMask, &event);
        event.xkey.type = KeyRelease;
        (void)XSendEvent(display, target, True, KeyReleaseMask, &event);
        XSync(display, False);
        printf("sent %s to 0x%lx\n", argv[2], target);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
