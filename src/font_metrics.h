#ifndef XTERM_PLUS_FONT_METRICS_H
#define XTERM_PLUS_FONT_METRICS_H

#include <stdbool.h>

double XtpFontHeightScale(unsigned int target_height, unsigned int source_height);
bool XtpFontFallbackAdvanceFits(double advance, unsigned int cell_width,
                                unsigned int committed_width, int limit_percent);
int XtpFontCenteredOrigin(double minimum, double maximum, int area_x, unsigned int area_width);

#endif
