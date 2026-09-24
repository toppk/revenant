#include "url_match.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int
Sentence(void)
{
        static const uint8_t sentence[] = "See https://example.com/path.";
        XtpUrlMatch match;

        return XtpUrlMatchAt(sentence, sizeof(sentence) - 1U, 12U, 13U, &match) &&
                       match.start == 4U && match.end == sizeof(sentence) - 2U
                   ? 0
                   : -1;
}

static int
PointerOutside(void)
{
        static const uint8_t sentence[] = "See https://example.com/path.";
        XtpUrlMatch match;

        return XtpUrlMatchAt(sentence, sizeof(sentence) - 1U, sizeof(sentence) - 2U,
                             sizeof(sentence) - 1U, &match)
                   ? -1
                   : 0;
}

static int
BalancedPunctuation(void)
{
        static const uint8_t balanced[] = "(https://example.com/a_(b)).";
        XtpUrlMatch match;

        return XtpUrlMatchAt(balanced, sizeof(balanced) - 1U, 10U, 11U, &match) &&
                       match.start == 1U && match.end == sizeof(balanced) - 3U
                   ? 0
                   : -1;
}

static int
UnmatchedPunctuation(void)
{
        static const uint8_t unmatched[] = "https://example.com/a).";
        XtpUrlMatch match;

        return XtpUrlMatchAt(unmatched, sizeof(unmatched) - 1U, 10U, 11U, &match) &&
                       match.start == 0 && match.end == sizeof(unmatched) - 3U
                   ? 0
                   : -1;
}

static int
CaseInsensitive(void)
{
        static const uint8_t multiple[] = "http://one.invalid and HTTPS://two.invalid/x";
        XtpUrlMatch match;

        return XtpUrlMatchAt(multiple, sizeof(multiple) - 1U, 30U, 31U, &match) &&
                       match.start == 23U && match.end == sizeof(multiple) - 1U
                   ? 0
                   : -1;
}

static int
MissingHost(void)
{
        static const uint8_t missing_host[] = "http:///path";
        XtpUrlMatch match;

        return XtpUrlMatchAt(missing_host, sizeof(missing_host) - 1U, 2U, 3U, &match) ? -1 : 0;
}

static int
LengthLimit(void)
{
        uint8_t excessive[XTP_URL_MAX_LENGTH + 2U];
        XtpUrlMatch match;

        memset(excessive, 'a', sizeof(excessive));
        memcpy(excessive, "https://", 8U);
        return XtpUrlMatchAt(excessive, sizeof(excessive), 10U, 11U, &match) ? -1 : 0;
}

static int
NullInput(void)
{
        XtpUrlMatch match;

        return XtpUrlMatchAt(NULL, 0, 0, 0, &match) ? -1 : 0;
}

typedef struct
{
        const char *name;
        int (*run)(void);
} Case;

int
main(int argc, char **argv)
{
        static const Case cases[] = {
            {"sentence", Sentence},
            {"pointer-outside", PointerOutside},
            {"balanced-punctuation", BalancedPunctuation},
            {"unmatched-punctuation", UnmatchedPunctuation},
            {"case-insensitive", CaseInsensitive},
            {"missing-host", MissingHost},
            {"length-limit", LengthLimit},
            {"null-input", NullInput},
        };
        size_t index;

        if (argc != 2) {
                fprintf(stderr, "usage: %s CASE\n", argv[0]);
                return 2;
        }
        for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
                if (strcmp(argv[1], cases[index].name) != 0)
                        continue;
                if (cases[index].run() != 0) {
                        fprintf(stderr, "url-match %s failed\n", cases[index].name);
                        return 1;
                }
                return 0;
        }
        fprintf(stderr, "unknown url-match case: %s\n", argv[1]);
        return 2;
}
