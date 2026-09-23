#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void
Pause(void)
{
        const struct timespec delay = {0, 100000000L};

        (void)nanosleep(&delay, NULL);
}

static Window
FindWindow(Display *display, Window parent, Bool popup)
{
        Window root;
        Window ancestor;
        Window *children = NULL;
        unsigned int count = 0;
        unsigned int index;
        Window result = None;

        if (!XQueryTree(display, parent, &root, &ancestor, &children, &count))
                return None;
        for (index = 0; index < count; ++index) {
                XWindowAttributes attrs;

                if (XGetWindowAttributes(display, children[index], &attrs) &&
                    attrs.map_state == IsViewable && (!popup || attrs.override_redirect)) {
                        result = children[index];
                        break;
                }
        }
        if (children != NULL)
                XFree(children);
        return result;
}

int
main(int argc, char **argv)
{
        Display *display;
        Window terminal;
        Window menu = None;
        unsigned long shell;
        char *end;
        XWindowAttributes attrs;
        XEvent event = {0};
        int attempt;
        int reverse_mode;

        if (argc < 2 || argc > 3 ||
            (argc == 3 && strcmp(argv[2], "title") != 0 && strcmp(argv[2], "color") != 0 &&
             strcmp(argv[2], "mouse") != 0 && strcmp(argv[2], "tcap") != 0 &&
             strcmp(argv[2], "linedrawing") != 0 && strcmp(argv[2], "utf8title") != 0 &&
             strcmp(argv[2], "reverse") != 0)) {
                fprintf(stderr,
                        "usage: %s SHELL-WINDOW "
                        "[title|color|mouse|tcap|linedrawing|utf8title|reverse]\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        reverse_mode = argc == 3 && strcmp(argv[2], "reverse") == 0;
        errno = 0;
        shell = strtoul(argv[1], &end, 0);
        if (errno != 0 || end == argv[1] || *end != '\0' || shell == 0)
                return EXIT_FAILURE;
        display = XOpenDisplay(NULL);
        if (display == NULL)
                return EXIT_FAILURE;
        terminal = FindWindow(display, shell, False);
        if (terminal == None)
                return EXIT_FAILURE;
        event.xbutton.type = ButtonPress;
        event.xbutton.display = display;
        event.xbutton.window = terminal;
        event.xbutton.root = DefaultRootWindow(display);
        event.xbutton.time = CurrentTime;
        event.xbutton.x = event.xbutton.y = 20;
        event.xbutton.x_root = event.xbutton.y_root = 20;
        event.xbutton.state = ControlMask;
        event.xbutton.button = reverse_mode ? Button2 : Button3;
        event.xbutton.same_screen = True;
        (void)XSendEvent(display, terminal, True, ButtonPressMask, &event);
        XSync(display, False);
        for (attempt = 0; attempt < 30 && menu == None; ++attempt) {
                Pause();
                menu = FindWindow(display, DefaultRootWindow(display), True);
        }
        if (menu == None || !XGetWindowAttributes(display, menu, &attrs)) {
                fprintf(stderr, "cannot find the font menu\n");
                return EXIT_FAILURE;
        }
        /* The menu can become viewable before Xaw finishes laying it out. */
        for (attempt = 0; attempt < 30; ++attempt) {
                XWindowAttributes settled;

                Pause();
                if (!XGetWindowAttributes(display, menu, &settled))
                        return EXIT_FAILURE;
                if (settled.height == attrs.height && settled.width == attrs.width &&
                    settled.y == attrs.y)
                        break;
                attrs = settled;
        }
        printf("menu 0x%lx geometry %dx%d+%d+%d\n", (unsigned long)menu, attrs.width, attrs.height,
               attrs.x, attrs.y);
        (void)fflush(stdout);
        /* Allow Window Ops is the last entry in xterm's font menu. */
        event.xmotion.type = MotionNotify;
        event.xmotion.window = menu;
        event.xmotion.x = attrs.width / 2;
        event.xmotion.y = attrs.height - 5;
        /* Menu font is fixed with vertSpace 0; count rows and separators up from Window Ops. */
        if (argc == 3 && !reverse_mode) {
                XFontStruct *font = XLoadQueryFont(display, "fixed");
                int row_height;
                int separator_height;
                int rows = strcmp(argv[2], "linedrawing") == 0 ? 13
                           : strcmp(argv[2], "utf8title") == 0 ? 6
                           : strcmp(argv[2], "color") == 0     ? 5
                           : strcmp(argv[2], "mouse") == 0     ? 3
                           : strcmp(argv[2], "tcap") == 0      ? 2
                                                               : 1;

                if (font == NULL)
                        return EXIT_FAILURE;
                row_height = font->ascent + font->descent;
                separator_height = ((int)attrs.height - 25 * row_height) / 3;
                if (separator_height < 0)
                        separator_height = 0;
                event.xmotion.y -= row_height * rows;
                if (rows == 13)
                        event.xmotion.y -= 2 * separator_height;
                else if (rows == 6)
                        event.xmotion.y -= separator_height;
                XFreeFont(display, font);
        }
        /* Reverse Video is the third row of the VT Options menu, counted from the top. */
        if (reverse_mode) {
                XFontStruct *font = XLoadQueryFont(display, "fixed");

                if (font == NULL)
                        return EXIT_FAILURE;
                event.xmotion.y =
                    (font->ascent + font->descent) * 2 + (font->ascent + font->descent) / 2;
                XFreeFont(display, font);
        }
        event.xmotion.x_root = attrs.x + event.xmotion.x;
        event.xmotion.y_root = attrs.y + event.xmotion.y;
        event.xmotion.state = ControlMask | (reverse_mode ? Button2Mask : Button3Mask);
        event.xmotion.same_screen = True;
        XWarpPointer(display, None, menu, 0, 0, 0, 0, event.xmotion.x, event.xmotion.y);
        XSync(display, False);
        (void)XSendEvent(display, menu, True, ButtonMotionMask, &event);
        XSync(display, False);
        Pause();
        event.xbutton.type = ButtonRelease;
        event.xbutton.button = reverse_mode ? Button2 : Button3;
        (void)XSendEvent(display, menu, True, ButtonReleaseMask, &event);
        XSync(display, False);
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
