#include <X11/Xlib.h>
#include <X11/keysym.h>
#ifdef XTP_HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static Window
EventWindow(Display *display, Window target)
{
        Window root;
        Window parent;
        Window *children = NULL;
        unsigned int child_count = 0;

        if (XQueryTree(display, target, &root, &parent, &children, &child_count) == 0)
                return None;
        if (child_count != 0)
                target = children[0];
        if (children != NULL)
                XFree(children);
        return target;
}

static void
SendButton(Display *display, Window target, int type, unsigned int button, unsigned int state,
           int x, int y)
{
        XEvent event = {0};

        event.xbutton.type = type;
        event.xbutton.display = display;
        event.xbutton.window = target;
        event.xbutton.root = DefaultRootWindow(display);
        event.xbutton.time = CurrentTime;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.x_root = x;
        event.xbutton.y_root = y;
        event.xbutton.same_screen = True;
        event.xbutton.button = button;
        event.xbutton.state = state;
        (void)XSendEvent(display, target, True,
                         type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &event);
}

#ifdef XTP_HAVE_XTEST
static void
MovePointer(Display *display, Window target, int x, int y)
{
        Window child;
        int root_x;
        int root_y;

        (void)XTranslateCoordinates(display, target, DefaultRootWindow(display), x, y, &root_x,
                                    &root_y, &child);
        (void)XTestFakeMotionEvent(display, DefaultScreen(display), root_x, root_y, CurrentTime);
}

/* Real (XTest) input; steps: press:B:X:Y release:B:X:Y motion:X:Y wheel:up|down[:X:Y]
 * shift:on|off pause:MS. */
static int
RunSteps(Display *display, Window target, int count, char **steps)
{
        KeyCode shift = XKeysymToKeycode(display, XK_Shift_L);

        for (int index = 0; index < count; index++) {
                char kind[16];
                char word[16];
                unsigned int button;
                int x;
                int y;

                if (sscanf(steps[index], "%15[a-z]:%u:%d:%d", kind, &button, &x, &y) == 4 &&
                    button >= 1 && button <= 3 &&
                    (strcmp(kind, "press") == 0 || strcmp(kind, "release") == 0)) {
                        MovePointer(display, target, x, y);
                        (void)XTestFakeButtonEvent(display, button, strcmp(kind, "press") == 0,
                                                   CurrentTime);
                } else if (sscanf(steps[index], "motion:%d:%d", &x, &y) == 2) {
                        MovePointer(display, target, x, y);
                } else if (sscanf(steps[index], "wheel:%15[a-z]:%d:%d", word, &x, &y) == 3 &&
                           (strcmp(word, "up") == 0 || strcmp(word, "down") == 0)) {
                        button = strcmp(word, "up") == 0 ? Button4 : Button5;
                        MovePointer(display, target, x, y);
                        (void)XTestFakeButtonEvent(display, button, True, CurrentTime);
                        (void)XTestFakeButtonEvent(display, button, False, CurrentTime);
                } else if (strcmp(steps[index], "wheel:up") == 0 ||
                           strcmp(steps[index], "wheel:down") == 0) {
                        button = strcmp(steps[index], "wheel:up") == 0 ? Button4 : Button5;
                        (void)XTestFakeButtonEvent(display, button, True, CurrentTime);
                        (void)XTestFakeButtonEvent(display, button, False, CurrentTime);
                } else if (sscanf(steps[index], "shift:%15[a-z]", word) == 1 &&
                           (strcmp(word, "on") == 0 || strcmp(word, "off") == 0)) {
                        (void)XTestFakeKeyEvent(display, shift, strcmp(word, "on") == 0,
                                                CurrentTime);
                } else if (sscanf(steps[index], "pause:%d", &x) == 1 && x >= 0) {
                        struct timespec delay = {x / 1000, (long)(x % 1000) * 1000000L};

                        XSync(display, False);
                        (void)nanosleep(&delay, NULL);
                } else {
                        fprintf(stderr, "invalid step: %s\n", steps[index]);
                        return -1;
                }
                XSync(display, False);
        }
        return 0;
}
#else
static int
RunSteps(Display *display, Window target, int count, char **steps)
{
        (void)display;
        (void)target;
        (void)count;
        (void)steps;
        fprintf(stderr, "built without XTest; --steps is unavailable\n");
        return 77;
}
#endif

/* 0 when scripted steps can run: XTest compiled in and offered by the server; 77 otherwise. */
static int
CheckXTest(void)
{
        Display *display = XOpenDisplay(NULL);
        int available = 0;

        if (display == NULL) {
                fprintf(stderr, "cannot open X display\n");
                return EXIT_FAILURE;
        }
#ifdef XTP_HAVE_XTEST
        {
                int event_base;
                int error_base;
                int major;
                int minor;

                available = XTestQueryExtension(display, &event_base, &error_base, &major, &minor);
        }
#endif
        XCloseDisplay(display);
        if (!available) {
                fprintf(stderr,
                        "XTest is unavailable (not built in or not offered by the server)\n");
                return 77;
        }
        return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
        Display *display;
        Window target;
        unsigned long target_value;
        unsigned long count_value = 1;
        unsigned int button = Button4;
        unsigned long tick;
        int steps = argc >= 3 && strcmp(argv[2], "--steps") == 0;

        if (argc == 2 && strcmp(argv[1], "--xtest") == 0)
                return CheckXTest();

        if (argc < 2 || (!steps && argc > 4)) {
                fprintf(stderr,
                        "usage: %s WINDOW-ID [up|down] [COUNT]\n"
                        "       %s WINDOW-ID --steps STEP...\n"
                        "       %s --xtest\n",
                        argv[0], argv[0], argv[0]);
                return EXIT_FAILURE;
        }
        if (ParseUnsignedLong(argv[1], &target_value) != 0 || target_value == 0) {
                fprintf(stderr, "%s: invalid X11 window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        if (!steps && argc >= 3) {
                if (strcmp(argv[2], "down") == 0)
                        button = Button5;
                else if (strcmp(argv[2], "up") != 0) {
                        fprintf(stderr, "%s: direction must be up or down\n", argv[0]);
                        return EXIT_FAILURE;
                }
        }
        if (!steps && argc == 4 &&
            (ParseUnsignedLong(argv[3], &count_value) != 0 || count_value == 0 ||
             count_value > UINT_MAX)) {
                fprintf(stderr, "%s: invalid event count: %s\n", argv[0], argv[3]);
                return EXIT_FAILURE;
        }

        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        target = EventWindow(display, (Window)target_value);
        if (target == None) {
                fprintf(stderr, "%s: cannot query window 0x%lx\n", argv[0], target_value);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        if (steps) {
                int result = RunSteps(display, target, argc - 3, argv + 3);

                XCloseDisplay(display);
                return result == 0 ? EXIT_SUCCESS : result == 77 ? 77 : EXIT_FAILURE;
        }

        for (tick = 0; tick < count_value; ++tick) {
                SendButton(display, target, ButtonPress, button, 0, 20, 20);
                SendButton(display, target, ButtonRelease, button, 0, 20, 20);
                XSync(display, False);
        }

        printf("sent %lu wheel-%s tick%s to 0x%lx\n", count_value,
               button == Button4 ? "up" : "down", count_value == 1 ? "" : "s", target);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
