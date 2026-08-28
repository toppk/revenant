#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
        Display *display = XOpenDisplay(NULL);
        int screen;
        char selection_name[64];
        Atom selection;
        Window owner;

        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        screen = DefaultScreen(display);
        (void)snprintf(selection_name, sizeof(selection_name), "_NET_WM_CM_S%d", screen);
        selection = XInternAtom(display, selection_name, False);
        owner = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, 0, 0);
        XSetSelectionOwner(display, selection, owner, CurrentTime);
        XSync(display, False);
        if (XGetSelectionOwner(display, selection) != owner) {
                fprintf(stderr, "cannot own %s\n", selection_name);
                XDestroyWindow(display, owner);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        puts("ready");
        (void)fflush(stdout);
        for (;;)
                pause();
}
