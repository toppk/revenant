#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
        Display *display;
        Colormap colormap;
        unsigned long pixels[256];
        unsigned long requested;
        char *end;
        unsigned int count = 0;

        if (argc != 2) {
                fprintf(stderr, "usage: %s COUNT\n", argv[0]);
                return EXIT_FAILURE;
        }
        requested = strtoul(argv[1], &end, 10);
        if (*argv[1] == '\0' || *end != '\0' || requested == 0 ||
            requested > sizeof(pixels) / sizeof(pixels[0])) {
                fprintf(stderr, "invalid color-cell count: %s\n", argv[1]);
                return EXIT_FAILURE;
        }
        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "cannot open display\n");
                return EXIT_FAILURE;
        }
        if (DefaultVisual(display, DefaultScreen(display))->class != PseudoColor) {
                fprintf(stderr, "default visual is not PseudoColor\n");
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        colormap = DefaultColormap(display, DefaultScreen(display));
        while (count < requested &&
               XAllocColorCells(display, colormap, False, NULL, 0, &pixels[count], 1))
                ++count;
        XSync(display, False);
        printf("held_colors=%u\n", count);
        fflush(stdout);
        if (count != requested) {
                fprintf(stderr, "could allocate only %u of %lu requested color cells\n", count,
                        requested);
                XCloseDisplay(display);
                return EXIT_FAILURE;
        }
        for (;;)
                (void)pause();
}
