#include "title_stack.h"

#include <stdlib.h>
#include <string.h>

void
XtpTitleEntryFree(XtpTitleEntry *entry)
{
        free(entry->icon_name);
        free(entry->window_name);
        entry->icon_name = NULL;
        entry->window_name = NULL;
}

static char *
Copy(const char *text)
{
        return text != NULL ? strdup(text) : NULL;
}

/* Mirrors xtermPushTitle: the ring wraps and `used` keeps counting. */
bool
XtpTitleStackPush(XtpTitleStack *stack, const XtpTitleEntry *entry, unsigned int slot)
{
        XtpTitleEntry copy = {Copy(entry->icon_name), Copy(entry->window_name)};
        unsigned int index;

        if ((entry->icon_name != NULL && copy.icon_name == NULL) ||
            (entry->window_name != NULL && copy.window_name == NULL)) {
                XtpTitleEntryFree(&copy);
                return false;
        }
        if (slot == 0)
                index = stack->used++ % XTP_TITLE_STACK_DEPTH;
        else
                index = (slot - 1U) % XTP_TITLE_STACK_DEPTH;
        XtpTitleEntryFree(&stack->data[index]);
        stack->data[index] = copy;
        return true;
}

/* xterm's TryHigher: a missing member is taken from the nearest older slot. */
static char *
NearestOlder(const XtpTitleStack *stack, unsigned int index, bool icon)
{
        unsigned int step;

        for (step = 1; step < XTP_TITLE_STACK_DEPTH; ++step) {
                unsigned int older = (index + XTP_TITLE_STACK_DEPTH - step) % XTP_TITLE_STACK_DEPTH;
                const char *candidate =
                    icon ? stack->data[older].icon_name : stack->data[older].window_name;

                if (candidate != NULL)
                        return strdup(candidate);
        }
        return NULL;
}

bool
XtpTitleStackPop(XtpTitleStack *stack, unsigned int slot, XtpTitleEntry *entry)
{
        unsigned int index;
        bool popped = false;

        entry->icon_name = NULL;
        entry->window_name = NULL;
        if (slot != 0) {
                index = (slot - 1U) % XTP_TITLE_STACK_DEPTH;
        } else if (stack->used > 0) {
                index = --stack->used % XTP_TITLE_STACK_DEPTH;
                popped = true;
        } else {
                return false;
        }
        entry->icon_name = stack->data[index].icon_name != NULL
                               ? strdup(stack->data[index].icon_name)
                               : NearestOlder(stack, index, true);
        entry->window_name = stack->data[index].window_name != NULL
                                 ? strdup(stack->data[index].window_name)
                                 : NearestOlder(stack, index, false);
        if (popped)
                XtpTitleEntryFree(&stack->data[index]);
        return true;
}

void
XtpTitleStackClear(XtpTitleStack *stack)
{
        unsigned int index;

        for (index = 0; index < XTP_TITLE_STACK_DEPTH; ++index)
                XtpTitleEntryFree(&stack->data[index]);
        stack->used = 0;
}
