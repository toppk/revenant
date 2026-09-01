#include "font_metrics.h"

#include <math.h>

double
XtpFontHeightScale(unsigned int target_height, unsigned int source_height)
{
        if (target_height == 0 || source_height == 0)
                return 1.0;
        return (double)target_height / (double)source_height;
}

bool
XtpFontFallbackAdvanceFits(double advance, unsigned int cell_width, unsigned int committed_width,
                           int limit_percent)
{
        double threshold;

        if (committed_width != 1U || cell_width == 0)
                return true;
        threshold = (double)(100 + limit_percent) * (double)cell_width;
        return fabs(advance) * 100.0 < threshold;
}

int
XtpFontCenteredOrigin(double minimum, double maximum, int area_x, unsigned int area_width)
{
        double origin = (double)area_x + ((double)area_width - (maximum - minimum)) / 2.0 - minimum;

        return (int)lround(origin);
}
