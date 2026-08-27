#ifndef XTERM_PLUS_VT_WIDGET_H
#define XTERM_PLUS_VT_WIDGET_H

#include <X11/Intrinsic.h>

#include <stddef.h>
#include <stdint.h>

typedef struct XtpTerminal XtpTerminal;

#define XtNfontChangedCallback "fontChangedCallback"
#define XtCFontChangedCallback "FontChangedCallback"
#define XtNpopupMenuCallback "popupMenuCallback"
#define XtCPopupMenuCallback "PopupMenuCallback"
#define XtNpasteCallback "pasteCallback"
#define XtCPasteCallback "PasteCallback"
#define XtNinputCallback "inputCallback"
#define XtCInputCallback "InputCallback"
#define XtNsizeChangedCallback "sizeChangedCallback"
#define XtCSizeChangedCallback "SizeChangedCallback"

typedef struct
{
        int slot;
        unsigned int cell_width;
        unsigned int cell_height;
} XtpFontChanged;

typedef struct
{
        unsigned int columns;
        unsigned int rows;
        unsigned int cell_width;
        unsigned int cell_height;
} XtpSizeChanged;

typedef struct
{
        Boolean loaded;
        unsigned int cell_width;
        unsigned int cell_height;
        double point_size;
} XtpFontSlotInfo;

typedef struct
{
        const char *name;
        XEvent *event;
} XtpPopupMenu;

typedef struct
{
        const uint8_t *bytes;
        size_t length;
} XtpPaste;

typedef struct
{
        const uint8_t *bytes;
        size_t length;
} XtpEncodedInput;

extern WidgetClass vt100WidgetClass;

unsigned int XtpVtCellWidth(Widget widget);
unsigned int XtpVtCellHeight(Widget widget);
unsigned int XtpVtColumns(Widget widget);
unsigned int XtpVtRows(Widget widget);
Boolean XtpVtFontSlotInfo(Widget widget, int slot, XtpFontSlotInfo *info);
const char *XtpVtRendererName(Widget widget);
Boolean XtpVtUsingXft(Widget widget);
Boolean XtpVtXftAvailable(Widget widget);
Boolean XtpVtSetRenderFont(Widget widget, Boolean enabled);
Dimension XtpVtNaturalWidth(Widget widget);
Dimension XtpVtNaturalHeight(Widget widget);
Boolean XtpVtSelectFont(Widget widget, int slot);
Boolean XtpVtScrollbarVisible(Widget widget);
void XtpVtSetScrollbar(Widget widget, Boolean visible);
Boolean XtpVtScrollKey(Widget widget);
void XtpVtSetScrollKey(Widget widget, Boolean enabled);
Boolean XtpVtScrollTtyOutput(Widget widget);
void XtpVtSetScrollTtyOutput(Widget widget, Boolean enabled);
Boolean XtpVtSelectToClipboard(Widget widget);
void XtpVtSetSelectToClipboard(Widget widget, Boolean enabled);
void XtpVtScrollOnKeypress(Widget widget);
void XtpVtSetTerminal(Widget widget, XtpTerminal *terminal);
void XtpVtSetFocus(Widget widget, Boolean focused);
void XtpVtUpdate(Widget widget);
void XtpVtRedraw(Widget widget);

#endif
