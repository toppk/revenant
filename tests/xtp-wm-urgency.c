#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* query prints every WM_HINTS field; seed stores distinctive values in all of them; focus in/out
 * moves keyboard focus to or off the shell. */
int
main(int argc, char **argv)
{
        Display *display;
        unsigned long shell;
        char *end;
        int result = EXIT_FAILURE;

        if (argc < 3 ||
            (strcmp(argv[1], "query") != 0 && strcmp(argv[1], "seed") != 0 &&
             strcmp(argv[1], "focus") != 0) ||
            (strcmp(argv[1], "focus") == 0 &&
             (argc != 4 || (strcmp(argv[3], "in") != 0 && strcmp(argv[3], "out") != 0)))) {
                fprintf(stderr,
                        "usage: %s query SHELL-WINDOW | seed SHELL-WINDOW | "
                        "focus SHELL-WINDOW {in|out}\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        errno = 0;
        shell = strtoul(argv[2], &end, 0);
        if (errno != 0 || end == argv[2] || *end != '\0' || shell == 0)
                return EXIT_FAILURE;
        display = XOpenDisplay(NULL);
        if (display == NULL)
                return EXIT_FAILURE;
        if (strcmp(argv[1], "query") == 0) {
                XWMHints *hints = XGetWMHints(display, (Window)shell);

                if (hints != NULL) {
                        printf(
                            "urgent=%d flags=0x%lx input=%d initial-state=%d icon-pixmap=0x%lx "
                            "icon-window=0x%lx icon-x=%d icon-y=%d icon-mask=0x%lx group=0x%lx\n",
                            (hints->flags & XUrgencyHint) != 0, hints->flags & ~XUrgencyHint,
                            hints->input, hints->initial_state, hints->icon_pixmap,
                            hints->icon_window, hints->icon_x, hints->icon_y, hints->icon_mask,
                            hints->window_group);
                        XFree(hints);
                        result = EXIT_SUCCESS;
                } else {
                        fprintf(stderr, "no WM_HINTS on 0x%lx\n", shell);
                }
        } else if (strcmp(argv[1], "seed") == 0) {
                XWMHints hints = {0};

                hints.flags = InputHint | StateHint | IconPixmapHint | IconWindowHint |
                              IconPositionHint | IconMaskHint | WindowGroupHint;
                hints.input = True;
                hints.initial_state = IconicState;
                hints.icon_pixmap = 0x51;
                hints.icon_window = 0x52;
                hints.icon_x = 37;
                hints.icon_y = 41;
                hints.icon_mask = 0x53;
                hints.window_group = (Window)shell;
                result =
                    XSetWMHints(display, (Window)shell, &hints) != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
                XSync(display, False);
        } else {
                Window target = strcmp(argv[3], "in") == 0 ? (Window)shell : None;

                XSetInputFocus(display, target, RevertToNone, CurrentTime);
                XSync(display, False);
                result = EXIT_SUCCESS;
        }
        XCloseDisplay(display);
        return result;
}
