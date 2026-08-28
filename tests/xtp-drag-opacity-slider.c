#include <X11/Xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void
ShortDelay(void)
{
        const struct timespec delay = {0, 100000000L};

        (void)nanosleep(&delay, NULL);
}

static Window
FirstChild(Display *display, Window window)
{
        Window root;
        Window parent;
        Window *children = NULL;
        unsigned int count = 0;
        Window result = None;

        if (XQueryTree(display, window, &root, &parent, &children, &count) && count != 0)
                result = children[0];
        if (children != NULL)
                XFree(children);
        return result;
}

static Window
FindPopupMenu(Display *display, Window shell)
{
        Window root = DefaultRootWindow(display);
        Window parent;
        Window *children = NULL;
        unsigned int count = 0;
        unsigned int index;
        Window result = None;
        unsigned long largest_area = 0;

        if (!XQueryTree(display, root, &root, &parent, &children, &count))
                return None;
        for (index = 0; index < count; ++index) {
                XWindowAttributes attributes;
                unsigned long area;

                if (children[index] == shell ||
                    !XGetWindowAttributes(display, children[index], &attributes) ||
                    attributes.map_state != IsViewable || !attributes.override_redirect)
                        continue;
                area = (unsigned long)attributes.width * (unsigned long)attributes.height;
                if (area > largest_area) {
                        largest_area = area;
                        result = children[index];
                }
        }
        if (children != NULL)
                XFree(children);
        return result;
}

int
main(int argc, char **argv)
{
        char *end;
        unsigned long parsed;
        Display *display;
        Window shell;
        Window terminal;
        Window menu;
        XWindowAttributes attributes;
        XEvent event = {0};
        int x;
        int y;

        if (argc != 2) {
                fprintf(stderr, "usage: %s SHELL-WINDOW\n", argv[0]);
                return EXIT_FAILURE;
        }
        errno = 0;
        parsed = strtoul(argv[1], &end, 0);
        if (errno != 0 || end == argv[1] || *end != '\0' || parsed == 0) {
                fprintf(stderr, "invalid shell window: %s\n", argv[1]);
                return EXIT_FAILURE;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        shell = (Window)parsed;
        terminal = FirstChild(display, shell);
        if (terminal == None) {
                fprintf(stderr, "cannot find terminal child\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }

        event.xbutton.type = ButtonPress;
        event.xbutton.display = display;
        event.xbutton.window = terminal;
        event.xbutton.root = DefaultRootWindow(display);
        event.xbutton.time = CurrentTime;
        event.xbutton.x = 20;
        event.xbutton.y = 20;
        event.xbutton.x_root = 20;
        event.xbutton.y_root = 20;
        event.xbutton.state = ControlMask;
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        (void)XSendEvent(display, terminal, True, ButtonPressMask, &event);
        XSync(display, False);
        ShortDelay();

        menu = FindPopupMenu(display, shell);
        if (menu == None || !XGetWindowAttributes(display, menu, &attributes)) {
                fprintf(stderr, "cannot find mapped Main Options menu\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        x = attributes.width / 2;
        /* The slider precedes line2; 19 SmeBSB and three SmeLine objects follow it. */
        y = attributes.height - 19 * 21 - 3 * 8 - 11;

        event.xmotion.type = MotionNotify;
        event.xmotion.window = menu;
        event.xmotion.x = x;
        event.xmotion.y = y;
        event.xmotion.x_root = attributes.x + x;
        event.xmotion.y_root = attributes.y + y;
        event.xmotion.state = ControlMask | Button1Mask;
        (void)XSendEvent(display, menu, True, ButtonMotionMask, &event);
        XSync(display, False);
        ShortDelay();

        event.xbutton.type = ButtonRelease;
        event.xbutton.window = menu;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.x_root = attributes.x + x;
        event.xbutton.y_root = attributes.y + y;
        event.xbutton.state = ControlMask | Button1Mask;
        (void)XSendEvent(display, menu, True, ButtonReleaseMask, &event);
        XSync(display, False);
        printf("dragged opacity slider in 0x%lx at %d,%d\n", menu, x, y);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
