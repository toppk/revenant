#include "command_options.h"

#include <X11/Intrinsic.h>

XrmOptionDescRec XtpCommandOptions[] = {
    {"-geometry", "*geometry", XrmoptionSepArg, NULL},
    {"-fn", "*vt100.font", XrmoptionSepArg, NULL},
    {"-fa", "*vt100.faceName", XrmoptionSepArg, NULL},
    {"-fd", "*vt100.faceNameDoublesize", XrmoptionSepArg, NULL},
    {"-fs", "*vt100.faceSize", XrmoptionSepArg, NULL},
    {"-b", "*vt100.internalBorder", XrmoptionSepArg, NULL},
    {"-sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"+sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sl", "*vt100.saveLines", XrmoptionSepArg, NULL},
    {"-cc", "*vt100.charClass", XrmoptionSepArg, NULL},
    {"-mc", "*vt100.multiClickTime", XrmoptionSepArg, NULL},
    {"-rightbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"-leftbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "true"},
    {"+sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "false"},
    {"-si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "false"},
    {"+si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "true"},
    {"-ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "true"},
    {"+ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "false"},
    {"-rv", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "true"},
    {"+rv", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "false"},
    {"-bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "true"},
    {"+bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "false"},
    {"-bcf", "*vt100.cursorOffTime", XrmoptionSepArg, NULL},
    {"-bcn", "*vt100.cursorOnTime", XrmoptionSepArg, NULL},
    {"-debug", "*debug", XrmoptionNoArg, (XPointer) "true"},
    {"+debug", "*debug", XrmoptionNoArg, (XPointer) "false"},
    {"-report-config", "*reportConfig", XrmoptionNoArg, (XPointer) "true"},
};

const int XtpCommandOptionCount = XtNumber(XtpCommandOptions);
