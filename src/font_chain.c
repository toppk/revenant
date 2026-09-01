#include "font_chain.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void
XtpFontChainClear(XtpFontChain *chain)
{
        size_t index;

        if (chain == NULL)
                return;
        for (index = 0; index < XTP_FONT_CHAIN_CAPACITY; ++index) {
                free(chain->entries[index]);
                chain->entries[index] = NULL;
        }
        chain->count = 0;
        chain->discarded = 0;
}

static int
ParseFontChain(const char *configured, XtpFontChain *chain, int xft_default)
{
        XtpFontChain parsed = {0};
        char *copy;
        char *cursor;

        if (chain == NULL)
                return -1;
        if (configured == NULL || *configured == '\0') {
                *chain = parsed;
                return 0;
        }
        copy = strdup(configured);
        if (copy == NULL)
                return -1;
        cursor = copy;
        while (cursor != NULL) {
                char *item = cursor;
                char *comma = strchr(cursor, ',');
                char *end;
                char *entry;

                if (comma != NULL) {
                        *comma = '\0';
                        cursor = comma + 1;
                } else {
                        cursor = NULL;
                }
                while (isspace((unsigned char)*item))
                        ++item;
                end = item + strlen(item);
                while (end > item && isspace((unsigned char)end[-1]))
                        --end;
                *end = '\0';
                if (*item == '\0' || strncmp(item, "x:", 2) == 0 || strncmp(item, "xlfd:", 5) == 0)
                        continue;
                if (strncmp(item, "xft:", 4) == 0) {
                        item += 4;
                        while (isspace((unsigned char)*item))
                                ++item;
                        end = item + strlen(item);
                        while (end > item && isspace((unsigned char)end[-1]))
                                --end;
                        *end = '\0';
                        if (*item == '\0')
                                continue;
                } else if (!xft_default)
                        continue;
                if (parsed.count == XTP_FONT_CHAIN_CAPACITY) {
                        ++parsed.discarded;
                        continue;
                }
                entry = strdup(item);
                if (entry == NULL) {
                        free(copy);
                        XtpFontChainClear(&parsed);
                        return -1;
                }
                parsed.entries[parsed.count++] = entry;
        }
        free(copy);
        *chain = parsed;
        return 0;
}

int
XtpFontChainParse(const char *configured, XtpFontChain *chain)
{
        return ParseFontChain(configured, chain, 1);
}

int
XtpFontChainParseXftEntries(const char *configured, XtpFontChain *chain)
{
        return ParseFontChain(configured, chain, 0);
}
