#ifndef XTERM_PLUS_SME_SLIDERP_H
#define XTERM_PLUS_SME_SLIDERP_H

#include "sme_slider.h"

#include <X11/IntrinsicP.h>
#include <X11/RectObjP.h>
#include <X11/Xaw/SmeP.h>

typedef struct
{
        XtPointer extension;
} XtpSmeSliderClassPart;

typedef struct _XtpSmeSliderClassRec
{
        RectObjClassPart rect_class;
        SmeClassPart sme_class;
        XtpSmeSliderClassPart slider_class;
} XtpSmeSliderClassRec;

extern XtpSmeSliderClassRec xtpSmeSliderClassRec;

typedef struct
{
        String label;
        int value;
        XFontStruct *font;
        Pixel foreground;
        XtCallbackList callbacks;
        GC gc;
        GC background_gc;
        Boolean highlighted;
} XtpSmeSliderPart;

typedef struct _XtpSmeSliderRec
{
        ObjectPart object;
        RectObjPart rectangle;
        SmePart sme;
        XtpSmeSliderPart slider;
} XtpSmeSliderRec;

#endif
