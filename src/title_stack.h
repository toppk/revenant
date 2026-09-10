#ifndef XTERM_PLUS_TITLE_STACK_H
#define XTERM_PLUS_TITLE_STACK_H

#include <stdbool.h>

/* xterm's MAX_SAVED_TITLES ring for XTWINOPS 22/23. */
#define XTP_TITLE_STACK_DEPTH 10U

typedef struct
{
        char *icon_name;
        char *window_name;
} XtpTitleEntry;

typedef struct
{
        XtpTitleEntry data[XTP_TITLE_STACK_DEPTH];
        unsigned int used;
} XtpTitleStack;

void XtpTitleEntryFree(XtpTitleEntry *entry);
/* slot 0 pushes; 1..depth writes that slot directly. Copies the strings. */
bool XtpTitleStackPush(XtpTitleStack *stack, const XtpTitleEntry *entry, unsigned int slot);
/* slot 0 pops; 1..depth reads that slot. Returns copies the caller frees. */
bool XtpTitleStackPop(XtpTitleStack *stack, unsigned int slot, XtpTitleEntry *entry);
void XtpTitleStackClear(XtpTitleStack *stack);

#endif
