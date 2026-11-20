/* The fullscreen part of an EWMH window manager, for xvfb-fullscreen.sh. */

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>

static Atom net_wm_state;
static Atom net_wm_state_fullscreen;
static XWindowAttributes saved;
static Bool have_saved;

static Bool
MarkedFullscreen(Display *display, Window window)
{
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned long index;
        unsigned char *data = NULL;
        Bool marked = False;

        if (XGetWindowProperty(display, window, net_wm_state, 0, 64, False, XA_ATOM, &type, &format,
                               &count, &remaining, &data) != Success)
                return False;
        if (type == XA_ATOM && format == 32) {
                for (index = 0; index < count && !marked; ++index)
                        marked = ((Atom *)(void *)data)[index] == net_wm_state_fullscreen;
        }
        if (data != NULL)
                XFree(data);
        return marked;
}

static void
Apply(Display *display, Window window, Bool fullscreen, const char *reason)
{
        Screen *screen = DefaultScreenOfDisplay(display);

        if (fullscreen) {
                if (!have_saved && XGetWindowAttributes(display, window, &saved) != 0)
                        have_saved = True;
                XChangeProperty(display, window, net_wm_state, XA_ATOM, 32, PropModeReplace,
                                (const unsigned char *)&net_wm_state_fullscreen, 1);
                XMoveResizeWindow(display, window, 0, 0, (unsigned int)WidthOfScreen(screen),
                                  (unsigned int)HeightOfScreen(screen));
        } else {
                XDeleteProperty(display, window, net_wm_state);
                if (have_saved)
                        XMoveResizeWindow(display, window, saved.x, saved.y,
                                          (unsigned int)saved.width, (unsigned int)saved.height);
                have_saved = False;
        }
        printf("%s window=0x%lx fullscreen=%s\n", reason, window, fullscreen ? "true" : "false");
        fflush(stdout);
}

int
main(void)
{
        Display *display = XOpenDisplay(NULL);
        Window root;
        Atom supported[2];

        if (display == NULL) {
                fprintf(stderr, "xtp-ewmh-fullscreen: cannot open display\n");
                return EXIT_FAILURE;
        }
        root = DefaultRootWindow(display);
        net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
        net_wm_state_fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
        supported[0] = net_wm_state;
        supported[1] = net_wm_state_fullscreen;
        XChangeProperty(display, root, XInternAtom(display, "_NET_SUPPORTED", False), XA_ATOM, 32,
                        PropModeReplace, (const unsigned char *)supported, 2);
        XSelectInput(display, root, SubstructureNotifyMask);
        XSync(display, False);
        printf("ready root=%dx%d\n", DisplayWidth(display, DefaultScreen(display)),
               DisplayHeight(display, DefaultScreen(display)));
        fflush(stdout);
        for (;;) {
                XEvent event;

                XNextEvent(display, &event);
                if (event.type == MapNotify && event.xmap.event == root &&
                    MarkedFullscreen(display, event.xmap.window)) {
                        Apply(display, event.xmap.window, True, "initial");
                } else if (event.type == ClientMessage &&
                           event.xclient.message_type == net_wm_state &&
                           (Atom)event.xclient.data.l[1] == net_wm_state_fullscreen) {
                        Bool current = MarkedFullscreen(display, event.xclient.window);
                        long action = event.xclient.data.l[0];

                        Apply(display, event.xclient.window, action == 2 ? !current : action == 1,
                              action == 1   ? "add"
                              : action == 0 ? "remove"
                                            : "toggle");
                }
        }
}
