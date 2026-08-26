#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int
ParseUnsignedLong(const char *text, unsigned long maximum, unsigned long *value)
{
        char *end = NULL;
        unsigned long parsed;

        errno = 0;
        parsed = strtoul(text, &end, 0);
        if (errno != 0 || text == end || end == NULL || *end != '\0' || parsed == 0 ||
            parsed > maximum)
                return -1;
        *value = parsed;
        return 0;
}

static void
Usage(const char *program)
{
        fprintf(stderr,
                "usage: %s WINDOW-ID [CYCLES [DELAY-MS "
                "[SMALL-WIDTH SMALL-HEIGHT LARGE-WIDTH LARGE-HEIGHT]]]\n"
                "       %s WINDOW-ID --grid SMALL-COLS LARGE-COLS ROWS DELAY-MS\n",
                program, program);
}

static int
GridPixels(Display *display, Window target, unsigned long columns, unsigned long rows,
           unsigned long *width, unsigned long *height)
{
        XSizeHints hints;
        long supplied = 0;
        unsigned long base_width;
        unsigned long base_height;

        if (XGetWMNormalHints(display, target, &hints, &supplied) == 0 ||
            (hints.flags & PResizeInc) == 0 || hints.width_inc <= 0 || hints.height_inc <= 0) {
                fprintf(stderr, "window does not publish usable resize increments\n");
                return -1;
        }
        if ((hints.flags & PBaseSize) != 0) {
                base_width = (unsigned long)hints.base_width;
                base_height = (unsigned long)hints.base_height;
        } else if ((hints.flags & PMinSize) != 0) {
                base_width = (unsigned long)hints.min_width;
                base_height = (unsigned long)hints.min_height;
        } else {
                base_width = 0;
                base_height = 0;
        }
        if (columns > (UINT_MAX - base_width) / (unsigned long)hints.width_inc ||
            rows > (UINT_MAX - base_height) / (unsigned long)hints.height_inc) {
                fprintf(stderr, "requested grid is too large\n");
                return -1;
        }
        *width = base_width + columns * (unsigned long)hints.width_inc;
        *height = base_height + rows * (unsigned long)hints.height_inc;
        return 0;
}

int
main(int argc, char **argv)
{
        Display *display;
        Window target;
        unsigned long target_value;
        unsigned long cycles = 4;
        unsigned long delay_ms = 100;
        unsigned long small_width = 400;
        unsigned long small_height = 300;
        unsigned long large_width = 1000;
        unsigned long large_height = 700;
        unsigned long small_columns = 0;
        unsigned long large_columns = 0;
        unsigned long grid_rows = 0;
        struct timespec delay;
        unsigned long cycle;
        bool grid_mode = false;

        if (argc == 7 && strcmp(argv[2], "--grid") == 0) {
                grid_mode = true;
        } else if (argc != 2 && argc != 3 && argc != 4 && argc != 8) {
                Usage(argv[0]);
                return EXIT_FAILURE;
        }
        if (ParseUnsignedLong(argv[1], ULONG_MAX, &target_value) != 0) {
                fprintf(stderr, "%s: invalid X11 window id: %s\n", argv[0], argv[1]);
                return EXIT_FAILURE;
        }
        if (grid_mode && (ParseUnsignedLong(argv[3], USHRT_MAX, &small_columns) != 0 ||
                          ParseUnsignedLong(argv[4], USHRT_MAX, &large_columns) != 0 ||
                          ParseUnsignedLong(argv[5], USHRT_MAX, &grid_rows) != 0 ||
                          ParseUnsignedLong(argv[6], 60000, &delay_ms) != 0)) {
                fprintf(stderr, "%s: invalid grid resize argument\n", argv[0]);
                return EXIT_FAILURE;
        }
        if (!grid_mode && argc >= 3 && ParseUnsignedLong(argv[2], UINT_MAX, &cycles) != 0) {
                fprintf(stderr, "%s: invalid cycle count: %s\n", argv[0], argv[2]);
                return EXIT_FAILURE;
        }
        if (!grid_mode && argc >= 4 && ParseUnsignedLong(argv[3], 60000, &delay_ms) != 0) {
                fprintf(stderr, "%s: invalid delay: %s\n", argv[0], argv[3]);
                return EXIT_FAILURE;
        }
        if (argc == 8 && (ParseUnsignedLong(argv[4], USHRT_MAX, &small_width) != 0 ||
                          ParseUnsignedLong(argv[5], USHRT_MAX, &small_height) != 0 ||
                          ParseUnsignedLong(argv[6], USHRT_MAX, &large_width) != 0 ||
                          ParseUnsignedLong(argv[7], USHRT_MAX, &large_height) != 0)) {
                fprintf(stderr, "%s: window dimensions must be between 1 and %u\n", argv[0],
                        USHRT_MAX);
                return EXIT_FAILURE;
        }

        display = XOpenDisplay(NULL);
        if (display == NULL) {
                fprintf(stderr, "%s: cannot open X display\n", argv[0]);
                return EXIT_FAILURE;
        }
        target = (Window)target_value;
        if (grid_mode) {
                cycles = 1;
                if (GridPixels(display, target, small_columns, grid_rows, &small_width,
                               &small_height) != 0 ||
                    GridPixels(display, target, large_columns, grid_rows, &large_width,
                               &large_height) != 0) {
                        XCloseDisplay(display);
                        return EXIT_FAILURE;
                }
        }
        delay.tv_sec = (time_t)(delay_ms / 1000);
        delay.tv_nsec = (long)(delay_ms % 1000) * 1000000L;

        for (cycle = 0; cycle < cycles; ++cycle) {
                XResizeWindow(display, target, (unsigned int)small_width,
                              (unsigned int)small_height);
                XSync(display, False);
                (void)nanosleep(&delay, NULL);
                XResizeWindow(display, target, (unsigned int)large_width,
                              (unsigned int)large_height);
                XSync(display, False);
                (void)nanosleep(&delay, NULL);
        }

        if (grid_mode) {
                printf("resized 0x%lx once: %lux%lu -> %lux%lu cells (%lux%lu -> %lux%lu pixels), "
                       "%lu ms delay\n",
                       target_value, small_columns, grid_rows, large_columns, grid_rows,
                       small_width, small_height, large_width, large_height, delay_ms);
        } else {
                printf("resized 0x%lx for %lu cycle%s: %lux%lu <-> %lux%lu, %lu ms delay\n",
                       target_value, cycles, cycles == 1 ? "" : "s", small_width, small_height,
                       large_width, large_height, delay_ms);
        }
        XCloseDisplay(display);
        return EXIT_SUCCESS;
}
