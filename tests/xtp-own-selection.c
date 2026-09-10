#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Own SELECTION with TEXT and serve UTF8_STRING, STRING, and TARGETS until
 * killed, sleeping DELAY-MS before each reply when given. */
int
main(int argc, char **argv)
{
        Display *display;
        Window owner;
        Atom selection;
        Atom utf8;
        Atom targets;
        Atom text;
        const char *value;
        size_t length;
        struct timespec delay = {0, 0};

        if (argc != 3 && argc != 4) {
                fprintf(stderr, "usage: %s SELECTION TEXT [DELAY-MS]\n", argv[0]);
                return EXIT_FAILURE;
        }
        if (argc == 4) {
                long delay_ms = strtol(argv[3], NULL, 10);

                delay.tv_sec = delay_ms / 1000;
                delay.tv_nsec = (delay_ms % 1000) * 1000000L;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        value = argv[2];
        length = strlen(value);
        selection = XInternAtom(display, argv[1], False);
        utf8 = XInternAtom(display, "UTF8_STRING", False);
        targets = XInternAtom(display, "TARGETS", False);
        text = XInternAtom(display, "TEXT", False);
        owner = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
        XSetSelectionOwner(display, selection, owner, CurrentTime);
        XSync(display, False);
        if (XGetSelectionOwner(display, selection) != owner) {
                fprintf(stderr, "%s: cannot own %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        puts("ready");
        (void)fflush(stdout);
        for (;;) {
                XEvent event;

                XNextEvent(display, &event);
                if (event.type == SelectionClear) {
                        puts("lost");
                        (void)fflush(stdout);
                        continue;
                }
                if (event.type != SelectionRequest)
                        continue;
                if (delay.tv_sec != 0 || delay.tv_nsec != 0)
                        (void)nanosleep(&delay, NULL);
                {
                        XSelectionRequestEvent *request = &event.xselectionrequest;
                        XSelectionEvent reply = {0};
                        Atom property =
                            request->property != None ? request->property : request->target;

                        reply.type = SelectionNotify;
                        reply.display = display;
                        reply.requestor = request->requestor;
                        reply.selection = request->selection;
                        reply.target = request->target;
                        reply.time = request->time;
                        reply.property = None;
                        if (request->target == targets) {
                                Atom available[3] = {targets, utf8, XA_STRING};

                                XChangeProperty(display, request->requestor, property, XA_ATOM, 32,
                                                PropModeReplace, (const unsigned char *)available,
                                                3);
                                reply.property = property;
                        } else if (request->target == utf8 || request->target == text ||
                                   request->target == XA_STRING) {
                                XChangeProperty(display, request->requestor, property,
                                                request->target == XA_STRING ? XA_STRING : utf8, 8,
                                                PropModeReplace, (const unsigned char *)value,
                                                (int)length);
                                reply.property = property;
                        }
                        XSendEvent(display, request->requestor, False, 0, (XEvent *)&reply);
                        XFlush(display);
                }
        }
}
