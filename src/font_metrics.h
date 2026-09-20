#ifndef XTERM_PLUS_FONT_METRICS_H
#define XTERM_PLUS_FONT_METRICS_H

#include <stdbool.h>
#include <stddef.h>

/* One fitted face instance, keyed by the face it was shrunk from and the span
 * it was shrunk to.  `fitted` is an XftFont *; this header stays free of Xft so
 * the table's boundary behavior can be unit-tested. */
typedef struct
{
        const void *source;
        void *fitted;
        unsigned int span;
} XtpFittedFaceSlot;

double XtpFontHeightScale(unsigned int target_height, unsigned int source_height);
bool XtpFontFallbackAdvanceFits(double advance, unsigned int cell_width,
                                unsigned int committed_width, int limit_percent);
int XtpFontCenteredOrigin(double minimum, double maximum, int area_x, unsigned int area_width);

/* Index of the entry for (source, span), or -1 when it is not present. */
int XtpFittedFaceFind(const XtpFittedFaceSlot *slots, size_t count, const void *source,
                      unsigned int span);
/* Index of a newly appended entry, or -1 when the table is full.  Entries are
 * never reused or evicted: callers hand out the fitted pointers. */
int XtpFittedFaceAppend(XtpFittedFaceSlot *slots, size_t *count, size_t capacity,
                        const void *source, unsigned int span);

#endif
