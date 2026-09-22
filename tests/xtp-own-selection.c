#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --serve mode: each TARGET=TYPE:HEX answers requests for TARGET with property type
 * TYPE and exactly those bytes, so a requestor can be given bytes whose declared
 * encoding is not what they would look like, or a target answered with an
 * unexpected type. TYPE INCR announces an incremental transfer of HEX bytes'
 * length and never completes it; TYPE:@FILE serves a file's bytes. Targets not
 * listed are refused. */
typedef struct
{
        Atom target;
        Atom type;
        unsigned char *bytes;
        size_t length;
} Served;

static int
HexValue(int c)
{
        if (c >= '0' && c <= '9')
                return c - '0';
        if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
        return -1;
}

static int
Serve(Display *display, int argc, char **argv)
{
        Served served[8];
        int count = 0;
        Atom targets = XInternAtom(display, "TARGETS", False);
        Atom incr = XInternAtom(display, "INCR", False);
        Atom selection = XInternAtom(display, argv[0], False);
        Window owner;
        int index;

        for (index = 1; index < argc && count < 8; ++index, ++count) {
                char *equals = strchr(argv[index], '=');
                char *colon = equals != NULL ? strchr(equals, ':') : NULL;
                size_t digits;
                size_t at;

                if (equals == NULL || colon == NULL)
                        return EXIT_FAILURE;
                *equals = '\0';
                *colon = '\0';
                served[count].target = XInternAtom(display, argv[index], False);
                served[count].type = XInternAtom(display, equals + 1, False);
                /* TYPE:@FILE serves a file's raw bytes: large payloads do not fit an
                 * argument as hex. */
                if (colon[1] == '@') {
                        FILE *file = fopen(colon + 2, "rb");
                        long size;

                        if (file == NULL || fseek(file, 0, SEEK_END) != 0 ||
                            (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
                                return EXIT_FAILURE;
                        served[count].length = (size_t)size;
                        served[count].bytes = malloc(served[count].length + 1U);
                        if (served[count].bytes == NULL ||
                            fread(served[count].bytes, 1, served[count].length, file) !=
                                served[count].length)
                                return EXIT_FAILURE;
                        fclose(file);
                        continue;
                }
                digits = strlen(colon + 1);
                served[count].length = digits / 2U;
                served[count].bytes = malloc(served[count].length + 1U);
                if (served[count].bytes == NULL || digits % 2U != 0)
                        return EXIT_FAILURE;
                for (at = 0; at < served[count].length; ++at) {
                        int high = HexValue(colon[1 + 2 * at]);
                        int low = HexValue(colon[2 + 2 * at]);

                        if (high < 0 || low < 0)
                                return EXIT_FAILURE;
                        served[count].bytes[at] = (unsigned char)(high * 16 + low);
                }
        }
        owner = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
        XSetSelectionOwner(display, selection, owner, CurrentTime);
        XSync(display, False);
        if (XGetSelectionOwner(display, selection) != owner)
                return EXIT_FAILURE;
        puts("ready");
        (void)fflush(stdout);
        for (;;) {
                XEvent event;
                XSelectionRequestEvent *request;
                XSelectionEvent reply = {0};
                Atom property;

                XNextEvent(display, &event);
                if (event.type == SelectionClear) {
                        puts("lost");
                        (void)fflush(stdout);
                        continue;
                }
                if (event.type != SelectionRequest)
                        continue;
                request = &event.xselectionrequest;
                property = request->property != None ? request->property : request->target;
                reply.type = SelectionNotify;
                reply.display = display;
                reply.requestor = request->requestor;
                reply.selection = request->selection;
                reply.target = request->target;
                reply.time = request->time;
                reply.property = None;
                if (request->target == targets) {
                        Atom available[9];

                        available[0] = targets;
                        for (index = 0; index < count; ++index)
                                available[index + 1] = served[index].target;
                        XChangeProperty(display, request->requestor, property, XA_ATOM, 32,
                                        PropModeReplace, (const unsigned char *)available,
                                        count + 1);
                        reply.property = property;
                }
                for (index = 0; index < count && reply.property == None; ++index) {
                        if (request->target != served[index].target)
                                continue;
                        if (served[index].type == incr) {
                                long size = (long)served[index].length;

                                XChangeProperty(display, request->requestor, property, incr, 32,
                                                PropModeReplace, (const unsigned char *)&size, 1);
                        } else {
                                XChangeProperty(display, request->requestor, property,
                                                served[index].type, 8, PropModeReplace,
                                                served[index].bytes, (int)served[index].length);
                        }
                        reply.property = property;
                }
                XSendEvent(display, request->requestor, False, 0, (XEvent *)&reply);
                XFlush(display);
        }
}

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

        if (argc >= 3 && strcmp(argv[1], "--serve") == 0) {
                display = XOpenDisplay(NULL);
                if (display == NULL) {
                        fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                        return EXIT_FAILURE;
                }
                return Serve(display, argc - 2, argv + 2);
        }
        if (argc != 3 && argc != 4) {
                fprintf(stderr,
                        "usage: %s SELECTION TEXT [DELAY-MS]\n"
                        "       %s --serve SELECTION TARGET=TYPE:HEX...\n",
                        argv[0], argv[0]);
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
