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

static int
SendKeyCycle(Display *display, Window target, KeySym keysym, unsigned int state, int repeat)
{
        KeyCode keycode = XKeysymToKeycode(display, keysym);
        XEvent event = {0};

        if (keycode == 0)
                return -1;
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
        if (repeat)
                (void)XSendEvent(display, target, True, KeyPressMask, &event);
        event.xkey.type = KeyRelease;
        (void)XSendEvent(display, target, True, KeyReleaseMask, &event);
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
             strcmp(argv[2], "shift-insert-cycle") != 0 && strcmp(argv[2], "up") != 0 &&
             strcmp(argv[2], "shift-up") != 0 && strcmp(argv[2], "ctrl-up") != 0 &&
             strcmp(argv[2], "alt-up") != 0 && strcmp(argv[2], "super-up") != 0 &&
             strcmp(argv[2], "f1") != 0 && strcmp(argv[2], "f5") != 0 &&
             strcmp(argv[2], "f12") != 0 && strcmp(argv[2], "f13") != 0 &&
             strcmp(argv[2], "f13-cycle") != 0 && strcmp(argv[2], "home") != 0 &&
             strcmp(argv[2], "delete") != 0 && strcmp(argv[2], "kp-1") != 0 &&
             strcmp(argv[2], "app-kp-1") != 0 && strcmp(argv[2], "kp-enter") != 0 &&
             strcmp(argv[2], "adiaeresis") != 0 && strcmp(argv[2], "compose-e-acute") != 0)) {
                fprintf(stderr,
                        "usage: %s WINDOW-ID "
                        "{ctrl-i|tab|a-cycle|shift-a-cycle|ctrl-cycle|up-cycle|"
                        "shift-insert-cycle|up|shift-up|ctrl-up|alt-up|super-up|"
                        "f1|f5|f12|f13|f13-cycle|home|delete|kp-1|app-kp-1|kp-enter|adiaeresis|"
                        "compose-e-acute}\n",
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
        } else if (strcmp(argv[2], "up") == 0) {
                keysym = XK_Up;
                state = 0;
        } else if (strcmp(argv[2], "shift-up") == 0) {
                keysym = XK_Up;
                state = ShiftMask;
        } else if (strcmp(argv[2], "ctrl-up") == 0) {
                keysym = XK_Up;
                state = ControlMask;
        } else if (strcmp(argv[2], "alt-up") == 0) {
                keysym = XK_Up;
                state = Mod1Mask;
        } else if (strcmp(argv[2], "super-up") == 0) {
                keysym = XK_Up;
                state = Mod4Mask;
        } else if (strcmp(argv[2], "f1") == 0) {
                keysym = XK_F1;
                state = 0;
        } else if (strcmp(argv[2], "f5") == 0) {
                keysym = XK_F5;
                state = 0;
        } else if (strcmp(argv[2], "f12") == 0) {
                keysym = XK_F12;
                state = 0;
        } else if (strcmp(argv[2], "f13") == 0 || strcmp(argv[2], "f13-cycle") == 0) {
                keysym = XK_F13;
                state = 0;
        } else if (strcmp(argv[2], "home") == 0) {
                keysym = XK_Home;
                state = 0;
        } else if (strcmp(argv[2], "delete") == 0) {
                keysym = XK_Delete;
                state = 0;
        } else if (strcmp(argv[2], "kp-1") == 0) {
                keysym = XK_KP_1;
                state = Mod2Mask;
        } else if (strcmp(argv[2], "app-kp-1") == 0) {
                keysym = XK_KP_1;
                state = 0;
        } else if (strcmp(argv[2], "kp-enter") == 0) {
                keysym = XK_KP_Enter;
                state = 0;
        } else if (strcmp(argv[2], "adiaeresis") == 0) {
                keysym = XK_a;
                state = 0;
        } else if (strcmp(argv[2], "compose-e-acute") == 0) {
                keysym = XK_a;
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
        if (strcmp(argv[2], "adiaeresis") == 0) {
                KeySym mapping = XK_adiaeresis;

                keycode = XKeysymToKeycode(display, XK_a);
                if (keycode == 0) {
                        fprintf(stderr, "%s: display has no keycode for a\n", argv[0]);
                        XCloseDisplay(display);
                        return EXIT_FAILURE;
                }
                XChangeKeyboardMapping(display, keycode, 1, &mapping, 1);
                XSync(display, False);
                keysym = XK_adiaeresis;
        } else if ((strcmp(argv[2], "f13") == 0 || strcmp(argv[2], "f13-cycle") == 0) &&
                   XKeysymToKeycode(display, XK_F13) == 0) {
                KeySym mapping = XK_F13;

                keycode = XKeysymToKeycode(display, XK_F12);
                if (keycode == 0) {
                        fprintf(stderr, "%s: display has no spare function-key code\n", argv[0]);
                        XCloseDisplay(display);
                        return EXIT_FAILURE;
                }
                XChangeKeyboardMapping(display, keycode, 1, &mapping, 1);
                XSync(display, False);
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
        if (strcmp(argv[2], "compose-e-acute") == 0) {
                KeySym mapping = XK_Multi_key;
                KeyCode compose_keycode = XKeysymToKeycode(display, XK_F12);

                if (compose_keycode == 0) {
                        fprintf(stderr, "%s: display has no spare compose keycode\n", argv[0]);
                        XCloseDisplay(display);
                        return EXIT_FAILURE;
                }
                XChangeKeyboardMapping(display, compose_keycode, 1, &mapping, 1);
                XSync(display, False);
                if (SendKeyCycle(display, target, XK_Multi_key, 0, 0) != 0 ||
                    SendKeyCycle(display, target, XK_apostrophe, 0, 0) != 0 ||
                    SendKeyCycle(display, target, XK_e, 0, 0) != 0) {
                        fprintf(stderr, "%s: cannot send compose sequence\n", argv[0]);
                        XCloseDisplay(display);
                        return EXIT_FAILURE;
                }
                XSync(display, False);
                printf("sent %s to 0x%lx\n", argv[2], target);
                XCloseDisplay(display);
                return EXIT_SUCCESS;
        }
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
