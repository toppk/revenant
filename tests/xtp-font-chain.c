#include "font_chain.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
        XtpFontChain chain = {0};
        size_t index;

        if (argc != 2) {
                fprintf(stderr, "usage: %s FONT-LIST\n", argv[0]);
                return EXIT_FAILURE;
        }
        if (XtpFontChainParse(argv[1], &chain) != 0) {
                fprintf(stderr, "%s: cannot parse font list\n", argv[0]);
                return EXIT_FAILURE;
        }
        printf("count=%zu\n", chain.count);
        printf("discarded=%zu\n", chain.discarded);
        for (index = 0; index < chain.count; ++index)
                printf("entry%zu=%s\n", index + 1U, chain.entries[index]);
        XtpFontChainClear(&chain);
        return EXIT_SUCCESS;
}
