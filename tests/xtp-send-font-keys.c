#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        unsigned long count_value = 2;
        KeySym keysym = XK_KP_Add;
        const char *key_name = "KP_Add";
        int count_argument = 2;
        KeyCode keycode;
        unsigned long press;

        if (argc < 2 || argc > 4) {
                fprintf(stderr, "usage: %s WINDOW-ID [+|-|insert|page-up|page-down] [COUNT]\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        if (ParseUnsignedLong(argv[1], &target_value) != 0 || target_value == 0) {
                fprintf(stderr, "%s: invalid X11 window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        if (argc >= 3 && (strcmp(argv[2], "+") == 0 || strcmp(argv[2], "-") == 0 ||
                          strcmp(argv[2], "insert") == 0 || strcmp(argv[2], "page-up") == 0 ||
                          strcmp(argv[2], "page-down") == 0)) {
                if (strcmp(argv[2], "-") == 0) {
                        keysym = XK_KP_Subtract;
                        key_name = "KP_Subtract";
                } else if (strcmp(argv[2], "insert") == 0) {
                        keysym = XK_Insert;
                        key_name = "Insert";
                } else if (strcmp(argv[2], "page-up") == 0) {
                        keysym = XK_Page_Up;
                        key_name = "Page_Up";
                } else if (strcmp(argv[2], "page-down") == 0) {
                        keysym = XK_Page_Down;
                        key_name = "Page_Down";
                }
                count_argument = 3;
        }
        if (argc > count_argument && (ParseUnsignedLong(argv[count_argument], &count_value) != 0 ||
                                      count_value == 0 || count_value > UINT_MAX)) {
                fprintf(stderr, "%s: invalid event count: %s\n", argv[0], argv[count_argument]);
                return EXIT_FAILURE;
        }
        if (argc > count_argument + 1) {
                fprintf(stderr, "usage: %s WINDOW-ID [+|-|insert|page-up|page-down] [COUNT]\n",
                        argv[0]);
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

        keycode = XKeysymToKeycode(display, keysym);
        if (keycode == 0) {
                fprintf(stderr, "%s: display has no %s keycode\n", argv[0], key_name);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        for (press = 0; press < count_value; ++press) {
                XEvent event = {0};

                event.xkey.display = display;
                event.xkey.window = target;
                event.xkey.root = DefaultRootWindow(display);
                event.xkey.subwindow = None;
                event.xkey.time = CurrentTime;
                event.xkey.x = 1;
                event.xkey.y = 1;
                event.xkey.x_root = 1;
                event.xkey.y_root = 1;
                event.xkey.same_screen = True;
                event.xkey.state = ShiftMask;
                event.xkey.keycode = keycode;
                event.xkey.type = KeyPress;
                (void)XSendEvent(display, target, True, KeyPressMask, &event);
                event.xkey.type = KeyRelease;
                (void)XSendEvent(display, target, True, KeyReleaseMask, &event);
                XSync(display, False);
                usleep(300000);
        }

        printf("sent %lu Shift+%s press%s to 0x%lx\n", count_value, key_name,
               count_value == 1 ? "" : "es", target);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
