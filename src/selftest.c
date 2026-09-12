#include "selftest.h"

#include "box_glyphs.h"
#include "char_class.h"
#include "cursor_blink.h"
#include "device_attributes.h"
#include "diagnostics.h"
#include "emoji_presentation.h"
#include "font_chain.h"
#include "font_metrics.h"
#include "font_report.h"
#include "font_route_cache.h"
#include "menus.h"
#include "pty_process.h"
#include "terminal.h"
#include "unicode_script.h"
#include "title_stack.h"
#include "working_directory.h"
#include "url_match.h"
#include "window_ops.h"
#include "version.h"
#include "welcome.h"
#include "x11_opacity.h"
#include "vt_widgetP.h"

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
SelfTestLogLevels(void)
{
        static const struct
        {
                const char *text;
                XtpLogLevel level;
                const char *name;
        } valid[] = {
            {"debug", XTP_LOG_DEBUG, "debug"},    {"INFO", XTP_LOG_INFO, "info"},
            {"warn", XTP_LOG_WARNING, "warning"}, {"warning", XTP_LOG_WARNING, "warning"},
            {"error", XTP_LOG_ERROR, "error"},
        };
        XtpLogLevel original = XtpLogLevelCurrent();
        XtpLogLevel parsed = XTP_LOG_DEBUG;
        size_t item;

        if (original != XTP_LOG_WARNING || XtpLogEnabled(XTP_LOG_DEBUG) ||
            !XtpLogEnabled(XTP_LOG_WARNING))
                return -1;
        for (item = 0; item < sizeof(valid) / sizeof(valid[0]); ++item) {
                if (XtpLogLevelParse(valid[item].text, &parsed) != 0 ||
                    parsed != valid[item].level ||
                    strcmp(XtpLogLevelName(parsed), valid[item].name) != 0)
                        return -1;
        }
        if (XtpLogLevelParse(NULL, &parsed) == 0 || XtpLogLevelParse("quiet", &parsed) == 0 ||
            XtpLogLevelParse("", &parsed) == 0 || XtpLogLevelParse("debug", NULL) == 0)
                return -1;
        XtpLogSetLevel(XTP_LOG_ERROR);
        if (XtpLogLevelCurrent() != XTP_LOG_ERROR || XtpLogEnabled(XTP_LOG_WARNING) ||
            !XtpLogEnabled(XTP_LOG_ERROR))
                return -1;
        XtpLogSetLevel(original);
        XtpLogSetQuiet(1);
        if (XtpLogEnabled(XTP_LOG_WARNING) || !XtpLogEnabled(XTP_LOG_ERROR))
                return -1;
        XtpLogSetQuiet(0);
        return 0;
}

static int
SelfTestOsRelease(void)
{
        static const char sample[] = "NAME=ignored\n"
                                     "ID=fedora\n"
                                     "ID_LIKE=\"rhel centos\"\n"
                                     "VERSION_ID='43'\n"
                                     "PRETTY_NAME=\"Fedora Linux 43 (Workstation Edition)\"\n"
                                     "HOME_URL=\"https://example.invalid/$ID\"\n";
        static const char escaped[] =
            "ID=custom\nID_LIKE=\"arch linux\"\nVERSION_ID=1\nPRETTY_NAME=\"Safe\\\" Name\033\"\n";
        static const char debian_like[] = "ID=linuxmint\nID_LIKE=\"ubuntu debian\"\n";
        static const char unknown[] = "ID=haiku\nPRETTY_NAME=Haiku\n";
        static const char single_quoted[] = "ID=test\nPRETTY_NAME='two\\\\slashes'\n";
        XtpOsRelease release;

        if (XtpWelcomeParseOsRelease(sample, &release) != 0 || strcmp(release.id, "fedora") != 0 ||
            strcmp(release.id_like, "rhel centos") != 0 || strcmp(release.version, "43") != 0 ||
            strcmp(release.name, "Fedora Linux 43 (Workstation Edition)") != 0 ||
            strcmp(XtpWelcomePackageFamily(&release), "dnf") != 0)
                return -1;
        if (XtpWelcomeParseOsRelease(escaped, &release) != 0 || strcmp(release.id, "custom") != 0 ||
            strcmp(release.id_like, "arch linux") != 0 ||
            strcmp(release.name, "Safe\" Name") != 0 ||
            strcmp(XtpWelcomePackageFamily(&release), "pacman") != 0 ||
            XtpWelcomeParseOsRelease(debian_like, &release) != 0 ||
            strcmp(XtpWelcomePackageFamily(&release), "apt") != 0 ||
            XtpWelcomeParseOsRelease(unknown, &release) != 0 ||
            XtpWelcomePackageFamily(&release) != NULL ||
            XtpWelcomeParseOsRelease(single_quoted, &release) != 0 ||
            strcmp(release.name, "two\\\\slashes") != 0 ||
            XtpWelcomeParseOsRelease("NAME=missing-id\n", &release) == 0 ||
            XtpWelcomeParseOsRelease(NULL, &release) == 0 ||
            XtpWelcomeParseOsRelease(sample, NULL) == 0 || XtpWelcomePackageFamily(NULL) != NULL)
                return -1;
        return 0;
}

static int
SelfTestWelcomeReadability(void)
{
        return !XtpWelcomeNeedsReadableFont(0, 0, 16U, 96.0) ||
                       !XtpWelcomeNeedsReadableFont(0, 0, 18U, 192.0) ||
                       XtpWelcomeNeedsReadableFont(1, 0, 12U, 192.0) ||
                       XtpWelcomeNeedsReadableFont(0, 1, 12U, 192.0) ||
                       XtpWelcomeNeedsReadableFont(0, 0, 18U, 96.0)
                   ? -1
                   : 0;
}

static int
SelfTestUrlMatch(void)
{
        static const uint8_t sentence[] = "See https://example.com/path.";
        static const uint8_t balanced[] = "(https://example.com/a_(b)).";
        static const uint8_t unmatched[] = "https://example.com/a).";
        static const uint8_t multiple[] = "http://one.invalid and HTTPS://two.invalid/x";
        static const uint8_t missing_host[] = "http:///path";
        uint8_t excessive[XTP_URL_MAX_LENGTH + 2U];
        XtpUrlMatch match;

        memset(excessive, 'a', sizeof(excessive));
        memcpy(excessive, "https://", 8U);
        if (!XtpUrlMatchAt(sentence, sizeof(sentence) - 1U, 12U, 13U, &match) ||
            match.start != 4U || match.end != sizeof(sentence) - 2U ||
            XtpUrlMatchAt(sentence, sizeof(sentence) - 1U, sizeof(sentence) - 2U,
                          sizeof(sentence) - 1U, &match) ||
            !XtpUrlMatchAt(balanced, sizeof(balanced) - 1U, 10U, 11U, &match) ||
            match.start != 1U || match.end != sizeof(balanced) - 3U ||
            !XtpUrlMatchAt(unmatched, sizeof(unmatched) - 1U, 10U, 11U, &match) ||
            match.start != 0 || match.end != sizeof(unmatched) - 3U ||
            !XtpUrlMatchAt(multiple, sizeof(multiple) - 1U, 30U, 31U, &match) ||
            match.start != 23U || match.end != sizeof(multiple) - 1U ||
            XtpUrlMatchAt(missing_host, sizeof(missing_host) - 1U, 2U, 3U, &match) ||
            XtpUrlMatchAt(excessive, sizeof(excessive), 10U, 11U, &match) ||
            XtpUrlMatchAt(NULL, 0, 0, 0, &match))
                return -1;
        return 0;
}

static int
SelfTestEmojiPresentation(void)
{
        static const char keycap[] = "1\xef\xb8\x8f\xe2\x83\xa3";
        static const char invalid_keycap[] = "A\xe2\x83\xa3";
        static const char modifier[] = "\xf0\x9f\x91\x8b\xf0\x9f\x8f\xbd";
        static const char ri_pair[] = "\xf0\x9f\x87\xba\xf0\x9f\x87\xb8";
        static const char tag_flag[] =
            "\xf0\x9f\x8f\xb4\xf3\xa0\x81\xa7\xf3\xa0\x81\xa2\xf3\xa0\x81\xb3"
            "\xf3\xa0\x81\xa3\xf3\xa0\x81\xb4\xf3\xa0\x81\xbf";
        static const char trailing_zwj[] = "\xf0\x9f\x91\xa8\xe2\x80\x8d";
        static const char technologist[] = "\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb";
        XtpEmojiClusterStyle cluster;

        if (strcmp(XtpEmojiUnicodeVersion(), "17.0") != 0 || !XtpEmojiHasProperty(0x1f600U) ||
            !XtpEmojiHasDefaultPresentation(0x1f600U) || !XtpEmojiHasProperty(0x263aU) ||
            XtpEmojiHasDefaultPresentation(0x263aU) || !XtpEmojiHasProperty(0x2764U) ||
            XtpEmojiHasDefaultPresentation(0x2764U) || !XtpEmojiHasProperty(0x2139U) ||
            XtpEmojiHasDefaultPresentation(0x2139U) || !XtpEmojiHasProperty(0x1fae8U) ||
            !XtpEmojiHasDefaultPresentation(0x1fae8U) || XtpEmojiHasProperty(0x65e5U) ||
            XtpEmojiHasDefaultPresentation(0x65e5U))
                return -1;
        if (XtpEmojiResolveStyle(0x1f600U, 0, XTP_EMOJI_POLICY_UNICODE) != XTP_EMOJI_STYLE_EMOJI ||
            XtpEmojiResolveStyle(0x263aU, 0, XTP_EMOJI_POLICY_UNICODE) != XTP_EMOJI_STYLE_TEXT ||
            XtpEmojiResolveStyle(0x263aU, 0xfe0fU, XTP_EMOJI_POLICY_TEXT) !=
                XTP_EMOJI_STYLE_EMOJI ||
            XtpEmojiResolveStyle(0x1f600U, 0xfe0eU, XTP_EMOJI_POLICY_EMOJI) !=
                XTP_EMOJI_STYLE_TEXT ||
            XtpEmojiResolveStyle('1', 0, XTP_EMOJI_POLICY_EMOJI) != XTP_EMOJI_STYLE_TEXT ||
            XtpEmojiResolveStyle('1', 0xfe0fU, XTP_EMOJI_POLICY_UNICODE) != XTP_EMOJI_STYLE_EMOJI ||
            XtpEmojiResolveStyle(0x65e5U, 0xfe0fU, XTP_EMOJI_POLICY_EMOJI) != XTP_EMOJI_STYLE_NONE)
                return -1;
        cluster =
            XtpEmojiResolveClusterStyle(keycap, sizeof(keycap) - 1U, XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != '1' || cluster.style != XTP_EMOJI_STYLE_EMOJI ||
            !cluster.requires_composition)
                return -1;
        cluster = XtpEmojiResolveClusterStyle(invalid_keycap, sizeof(invalid_keycap) - 1U,
                                              XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 'A' || cluster.style != XTP_EMOJI_STYLE_NONE ||
            cluster.requires_composition)
                return -1;
        cluster =
            XtpEmojiResolveClusterStyle(modifier, sizeof(modifier) - 1U, XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 0x1f44bU || !cluster.requires_composition)
                return -1;
        cluster =
            XtpEmojiResolveClusterStyle(ri_pair, sizeof(ri_pair) - 1U, XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 0x1f1faU || !cluster.requires_composition)
                return -1;
        cluster =
            XtpEmojiResolveClusterStyle(tag_flag, sizeof(tag_flag) - 1U, XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 0x1f3f4U || !cluster.requires_composition)
                return -1;
        cluster = XtpEmojiResolveClusterStyle(trailing_zwj, sizeof(trailing_zwj) - 1U,
                                              XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 0x1f468U || cluster.style != XTP_EMOJI_STYLE_EMOJI ||
            cluster.requires_composition)
                return -1;
        cluster = XtpEmojiResolveClusterStyle(technologist, sizeof(technologist) - 1U,
                                              XTP_EMOJI_POLICY_UNICODE);
        if (cluster.base != 0x1f469U || cluster.style != XTP_EMOJI_STYLE_EMOJI ||
            !cluster.requires_composition)
                return -1;
        return 0;
}

static int
SelfTestUnicodeScript(void)
{
        static const char han[] = "\xe6\x97\xa5";
        static const char blank_cluster[] = " \xef\xb8\x8f";
        static const char malformed[] = "\xc0\x80";
        uint32_t codepoint = 0;
        size_t consumed = 0;

        if (strcmp(XtpHanUnicodeVersion(), "17.0") != 0 || !XtpUnicodeScriptHan(0x65e5U) ||
            !XtpUnicodeScriptHan(0x2f00U) || !XtpUnicodeScriptHan(0xf900U) ||
            XtpUnicodeScriptHan(0x3042U) || XtpUnicodeScriptHan(0xac00U) ||
            XtpUnicodeScriptHan(0x3001U) || XtpUnicodeScriptHan(0xff0cU))
                return -1;
        if (!XtpUtf8Decode(han, sizeof(han) - 1U, &codepoint, &consumed) || codepoint != 0x65e5U ||
            consumed != 3U ||
            XtpUtf8Decode(malformed, sizeof(malformed) - 1U, &codepoint, &consumed) ||
            XtpUnicodeClusterRequiresInk(blank_cluster, sizeof(blank_cluster) - 1U) ||
            !XtpUnicodeClusterRequiresInk(han, sizeof(han) - 1U) ||
            !XtpUnicodeClusterRequiresInk(malformed, sizeof(malformed) - 1U) ||
            !XtpUnicodeSequenceControl(0x200dU) || !XtpUnicodeSequenceControl(0xe0100U) ||
            XtpUnicodeSequenceControl('A'))
                return -1;
        return 0;
}

static int
SelfTestFontChain(void)
{
        static const struct
        {
                const char *configured;
                size_t count;
                const char *first;
                const char *second;
                size_t discarded;
        } cases[] = {
            {NULL, 0, NULL, NULL, 0},
            {"", 0, NULL, NULL, 0},
            {", DejaVu Sans Mono:size=11,", 1, "DejaVu Sans Mono:size=11", NULL, 0},
            {" xft:DejaVu Sans Mono , x:fixed ", 1, "DejaVu Sans Mono", NULL, 0},
            {"x:fixed,xft:DejaVu Sans Mono", 1, "DejaVu Sans Mono", NULL, 0},
            {"x11:fixed,xft:Noto Sans Mono CJK JP", 2, "x11:fixed", "Noto Sans Mono CJK JP", 0},
            {"NoSuchFontZZZQQ:size=11,DejaVu Sans Mono:size=11", 2, "NoSuchFontZZZQQ:size=11",
             "DejaVu Sans Mono:size=11", 0},
            {"A,B,C,D", 2, "A", "B", 2},
            {"xft: , x:fixed, A", 1, "A", NULL, 0},
        };
        XtpFontChain chain = {0};
        size_t index;

        for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
                if (XtpFontChainParse(cases[index].configured, &chain) != 0 ||
                    chain.count != cases[index].count ||
                    chain.discarded != cases[index].discarded ||
                    ((cases[index].first == NULL) != (chain.entries[0] == NULL)) ||
                    (cases[index].first != NULL &&
                     strcmp(cases[index].first, chain.entries[0]) != 0) ||
                    ((cases[index].second == NULL) != (chain.entries[1] == NULL)) ||
                    (cases[index].second != NULL &&
                     strcmp(cases[index].second, chain.entries[1]) != 0)) {
                        XtpFontChainClear(&chain);
                        return -1;
                }
                XtpFontChainClear(&chain);
        }
        if (XtpFontChainParseXftEntries("fixed,xft:A,x:B,xlfd:C,xft:D,xft:E", &chain) != 0 ||
            chain.count != 2 || chain.discarded != 1 || strcmp(chain.entries[0], "A") != 0 ||
            strcmp(chain.entries[1], "D") != 0) {
                XtpFontChainClear(&chain);
                return -1;
        }
        XtpFontChainClear(&chain);
        return 0;
}

static int
SelfTestFontMetrics(void)
{
        double scale = XtpFontHeightScale(27U, 30U);

        if (scale < 0.899999999 || scale > 0.900000001 || XtpFontHeightScale(0U, 30U) != 1.0 ||
            XtpFontHeightScale(27U, 0U) != 1.0 ||
            !XtpFontFallbackAdvanceFits(10.999, 10U, 1U, 10) ||
            XtpFontFallbackAdvanceFits(11.0, 10U, 1U, 10) ||
            !XtpFontFallbackAdvanceFits(-10.999, 10U, 1U, 10) ||
            !XtpFontFallbackAdvanceFits(40.0, 10U, 2U, 10) ||
            !XtpFontFallbackAdvanceFits(40.0, 0U, 1U, 10) ||
            XtpFontCenteredOrigin(0.0, 12.0, 0, 13U) != 1 ||
            XtpFontCenteredOrigin(0.0, 11.0, 0, 10U) != -1 ||
            XtpFontCenteredOrigin(-2.0, 8.0, 10, 10U) != 12)
                return -1;
        return 0;
}

static int
SelfTestFontReportBound(void)
{
        XtpFontRoutingReport *report = XtpFontRoutingReportCreate(true);
        XtpFontRoutingReport *build = XtpFontRoutingReportCreate(true);
        XtpFontRouteTrace trace = {0};
        XtpFontRouteValue value = {XTP_FONT_ROUTE_TOFU, XTP_FONT_RUNG_TOFU, 0, NULL};
        XtpFontRouteKey key = {0};
        size_t index;

        if (report == NULL || build == NULL) {
                XtpFontRoutingReportDestroy(report);
                XtpFontRoutingReportDestroy(build);
                return -1;
        }
        key.width = 1;
        for (index = 0; index <= XTP_FONT_REPORT_ROUTE_CAPACITY; ++index) {
                int length = snprintf(key.text, sizeof(key.text), "route-%zu", index);

                if (length <= 0 || (size_t)length >= sizeof(key.text)) {
                        XtpFontRoutingReportDestroy(report);
                        XtpFontRoutingReportDestroy(build);
                        return -1;
                }
                key.text_length = (uint8_t)length;
                XtpFontRoutingReportRoute(report, &key, value, NULL, NULL, "normal", false);
        }
        if (XtpFontRoutingReportRouteCount(report) != XTP_FONT_REPORT_ROUTE_CAPACITY ||
            !XtpFontRoutingReportRouteBounded(report)) {
                XtpFontRoutingReportDestroy(report);
                XtpFontRoutingReportDestroy(build);
                return -1;
        }
        for (index = 0; index <= XTP_FONT_ROUTE_MISS_CAPACITY; ++index)
                XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_NAMED, (uint8_t)(index % 16U + 1U),
                                     XTP_FONT_MISS_CMAP);
        if (trace.count != XTP_FONT_ROUTE_MISS_CAPACITY || !trace.bounded) {
                XtpFontRoutingReportDestroy(report);
                XtpFontRoutingReportDestroy(build);
                return -1;
        }
        for (index = 0; index <= XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY; ++index)
                XtpFontRoutingReportLoad(build, "primary", 0, "normal", 1, "configured", NULL,
                                         "active", 1);
        if (XtpFontRoutingReportLoadCount(build) != XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY + 1U ||
            XtpFontRoutingReportLoadBounded(build)) {
                XtpFontRoutingReportDestroy(report);
                XtpFontRoutingReportDestroy(build);
                return -1;
        }
        XtpFontRoutingReportMergeBuild(report, build);
        if (XtpFontRoutingReportLoadCount(report) != XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY + 1U ||
            XtpFontRoutingReportLoadBounded(report)) {
                XtpFontRoutingReportDestroy(report);
                XtpFontRoutingReportDestroy(build);
                return -1;
        }
        XtpFontRoutingReportDestroy(report);
        XtpFontRoutingReportDestroy(build);
        return 0;
}

static XtpFontRouteKey
RouteCacheKey(const char *text)
{
        XtpFontRouteKey key = {0};
        size_t length = strlen(text);

        memcpy(key.text, text, length);
        key.text_length = (uint8_t)length;
        key.width = 1;
        key.presentation = 1;
        key.presentation_policy = 2;
        key.slot = 3;
        key.capturing_slot = 4;
        key.color_glyphs = true;
        key.system_fallback = true;
        key.generation = 5;
        return key;
}

static int
SelfTestFontRouteCache(void)
{
        XtpFontRouteCache *cache = XtpFontRouteCacheCreate(2);
        XtpFontRouteKey first = RouteCacheKey("a");
        XtpFontRouteKey second = RouteCacheKey("b");
        XtpFontRouteKey third = RouteCacheKey("c");
        XtpFontRouteKey changed;
        XtpFontRouteValue value = {XTP_FONT_ROUTE_PRIMARY, XTP_FONT_RUNG_ENTRY1, 0,
                                   (void *)(uintptr_t)1U};
        XtpFontRouteValue found = {0};

        if (cache == NULL || !XtpFontRouteKeysEqual(&first, &first) ||
            XtpFontRouteKeysEqual(&first, &second) || XtpFontRouteKeysEqual(NULL, &first) ||
            XtpFontRouteKeysEqual(&first, NULL) || !XtpFontRouteCacheStore(cache, &first, value) ||
            !XtpFontRouteCacheStore(cache, &second, value) || XtpFontRouteCacheCount(cache) != 2U ||
            !XtpFontRouteCacheLookup(cache, &first, &found) ||
            found.kind != XTP_FONT_ROUTE_PRIMARY || found.normal_font != value.normal_font) {
                XtpFontRouteCacheDestroy(cache);
                return -1;
        }

#define XTP_CHECK_KEY_FIELD(field, replacement)                                                    \
        do {                                                                                       \
                changed = first;                                                                   \
                changed.field = (replacement);                                                     \
                if (XtpFontRouteKeysEqual(&first, &changed) ||                                     \
                    XtpFontRouteCacheLookup(cache, &changed, NULL)) {                              \
                        XtpFontRouteCacheDestroy(cache);                                           \
                        return -1;                                                                 \
                }                                                                                  \
        } while (0)
        XTP_CHECK_KEY_FIELD(width, 2);
        XTP_CHECK_KEY_FIELD(text_length, 0);
        XTP_CHECK_KEY_FIELD(presentation, 2);
        XTP_CHECK_KEY_FIELD(presentation_policy, 1);
        XTP_CHECK_KEY_FIELD(slot, 2);
        XTP_CHECK_KEY_FIELD(capturing_slot, 3);
        XTP_CHECK_KEY_FIELD(color_glyphs, false);
        XTP_CHECK_KEY_FIELD(system_fallback, false);
        XTP_CHECK_KEY_FIELD(generation, 6);
#undef XTP_CHECK_KEY_FIELD

        changed = first;
        changed.text[0] = 'z';
        if (XtpFontRouteCacheLookup(cache, &changed, NULL) ||
            !XtpFontRouteCacheStore(cache, &third, value) ||
            !XtpFontRouteCacheLookup(cache, &first, NULL) ||
            XtpFontRouteCacheLookup(cache, &second, NULL) ||
            !XtpFontRouteCacheLookup(cache, &third, NULL)) {
                XtpFontRouteCacheDestroy(cache);
                return -1;
        }
        XtpFontRouteCacheDestroy(cache);
        return 0;
}

static int
SelfTestPty(void)
{
        char *command[] = {
            (char *)"/bin/sh",
            (char *)"-c",
            (char *)"printf 'pty-env|%s|%s' \"$TERM_PROGRAM\" "
                    "\"$TERM_PROGRAM_VERSION\"",
            NULL,
        };
        XtpPty *pty = XtpPtySpawn(command, NULL, 80, 24, 8, 16);
        char output[256];
        size_t used = 0;
        int attempts;

        if (pty == NULL)
                return -1;
        for (attempts = 0; attempts < 10 && used + 1U < sizeof(output); ++attempts) {
                struct pollfd descriptor = {
                    XtpPtyFd(pty),
                    POLLIN | POLLHUP,
                    0,
                };
                ssize_t amount;

                if (poll(&descriptor, 1, 200) < 0 && errno != EINTR)
                        break;
                amount = XtpPtyRead(pty, output + used, sizeof(output) - used - 1U);
                if (amount > 0) {
                        used += (size_t)amount;
                } else if (amount < 0 &&
                           (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                        continue;
                } else {
                        break;
                }
        }
        output[used] = '\0';
        XtpPtyFree(pty);
        {
                char expected[256];

                (void)snprintf(expected, sizeof(expected), "pty-env|%s|%s", XTP_PROGRAM_NAME,
                               XTP_VERSION);
                return strstr(output, expected) != NULL ? 0 : -1;
        }
}

static int
SelfTestCharClass(XtpTerminal *terminal)
{
        static const char *const invalid[] = {
            "",
            " ",
            "-1:48",
            "33-",
            "40-33:48",
            "33:",
            "33:48,",
            "33::48",
            "0x110000",
            "junk",
            "33:999999999999999999999999999999999999999999999999999999999999999",
        };
        XtpCharClassTable *table = NULL;
        XtpCharClassTable *candidate;
        size_t index;

        if (XtpCharClassOf(NULL, 0) != 32 || XtpCharClassOf(NULL, '\t') != 32 ||
            XtpCharClassOf(NULL, 1) != 1 || XtpCharClassOf(NULL, '!') != '!' ||
            XtpCharClassOf(NULL, 'A') != 48 || XtpCharClassOf(NULL, 215) != 215 ||
            XtpCharClassOf(NULL, 0x2000) != 32 || XtpCharClassOf(NULL, 0x3042) != 0x3040)
                return -1;
        if (XtpCharClassParse("33:48, 37:48, 65-90:7, 67:9, 0x100-0x102:12", &table) != 0 ||
            table == NULL || XtpCharClassOf(table, '!') != 48 || XtpCharClassOf(table, '%') != 48 ||
            XtpCharClassOf(table, 'A') != 7 || XtpCharClassOf(table, 'C') != 9 ||
            XtpCharClassOf(table, 0x101) != 12)
                goto failure;
        for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
                candidate = table;
                if (XtpCharClassParse(invalid[index], &candidate) == 0 || candidate != table)
                        goto failure;
        }
        if (XtpTerminalSetCharClass(terminal, "33:48,37:48") != 0 ||
            XtpTerminalSetCharClass(terminal, "-1:48") == 0 ||
            XtpTerminalSetCharClass(terminal, NULL) != 0)
                goto failure;
        XtpCharClassFree(table);
        return 0;

failure:
        XtpCharClassFree(table);
        return -1;
}

static int
SelfTestBackgroundOpacity(void)
{
        static const char *const invalid[] = {
            "", "-0.1", "1.1", "nan", "inf", ".", "0.5e0", "0.5 trailing",
        };
        XtpX11AlphaFormat format = {
            .mask = 0xffU,
            .shift = 24,
            .red_mask = 0xffU,
            .red_shift = 16,
            .green_mask = 0xffU,
            .green_shift = 8,
            .blue_mask = 0xffU,
            .blue_shift = 0,
        };
        uint16_t alpha;
        Pixel pixel;
        size_t index;

        if (XtpBackgroundOpacityParse(NULL, &alpha) != 0 || alpha != UINT16_MAX ||
            XtpBackgroundOpacityParse("0", &alpha) != 0 || alpha != 0 ||
            XtpBackgroundOpacityParse(" 0.5 ", &alpha) != 0 || alpha < 32767U || alpha > 32768U ||
            XtpBackgroundOpacityParse(".5", &alpha) != 0 || alpha < 32767U || alpha > 32768U ||
            XtpBackgroundOpacityParse("+1.", &alpha) != 0 || alpha != UINT16_MAX ||
            XtpBackgroundOpacityParse("1.0", &alpha) != 0 || alpha != UINT16_MAX ||
            !XtpBackgroundOpacityDisabled("disabled") ||
            !XtpBackgroundOpacityDisabled(" Disabled ") ||
            XtpBackgroundOpacityDisabled("disable") || XtpBackgroundOpacityDisabled("0.5"))
                return -1;
        for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
                if (XtpBackgroundOpacityParse(invalid[index], &alpha) == 0)
                        return -1;
        }
        pixel = XtpX11PixelWithAlpha(0x00ffffffUL, &format, 41942U);
        if (pixel != 0xa3a3a3a3UL || XtpX11OpaquePixel(pixel, &format) != 0xffffffffUL)
                return -1;
        pixel = XtpX11PixelWithAlpha(0x00ff8000UL, &format, 41942U);
        if (pixel != 0xa3a35200UL || XtpX11OpaquePixel(pixel, &format) != 0xffff8000UL)
                return -1;
        pixel = XtpX11PixelWithAlpha(0x00000000UL, &format, 41942U);
        if (pixel != 0xa3000000UL || XtpX11PixelAlpha(pixel, &format) < 41890U ||
            XtpX11PixelAlpha(pixel, &format) > 41892U)
                return -1;
        return 0;
}

static int
SelfTestPtyQueue(void)
{
        static const size_t first_write = 96U * 1024U;
        static const size_t payload_size = 160U * 1024U;
        char *command[] = {
            (char *)"/bin/sh",
            (char *)"-c",
            (char *)"stty raw -echo; printf R; sleep 0.2; exec cat",
            NULL,
        };
        XtpPty *pty = XtpPtySpawn(command, NULL, 80, 24, 8, 16);
        uint8_t *payload = NULL;
        uint8_t buffer[8192];
        size_t received = 0;
        int attempts;
        int result = -1;

        if (pty == NULL)
                return -1;
        for (attempts = 0; attempts < 20; ++attempts) {
                struct pollfd descriptor = {XtpPtyFd(pty), POLLIN, 0};
                ssize_t amount;

                if (poll(&descriptor, 1, 100) < 0 && errno != EINTR)
                        goto done;
                amount = XtpPtyRead(pty, buffer, sizeof(buffer));
                if (amount == 1 && buffer[0] == 'R')
                        break;
                if (amount < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
                        continue;
                goto done;
        }
        if (attempts == 20)
                goto done;
        payload = malloc(payload_size);
        if (payload == NULL)
                goto done;
        for (received = 0; received < payload_size; ++received)
                payload[received] = (uint8_t)(received * 31U + 7U);
        received = 0;
        if (XtpPtyQueue(pty, payload, first_write) != 0 || XtpPtyFlush(pty) != 1 ||
            XtpPtyPending(pty) == 0 ||
            XtpPtyQueue(pty, payload + first_write, payload_size - first_write) != 0)
                goto done;
        for (attempts = 0; attempts < 200 && (XtpPtyPending(pty) != 0 || received < payload_size);
             ++attempts) {
                struct pollfd descriptor = {XtpPtyFd(pty),
                                            POLLIN | (XtpPtyPending(pty) != 0 ? POLLOUT : 0), 0};
                ssize_t amount;

                if (poll(&descriptor, 1, 100) < 0) {
                        if (errno == EINTR)
                                continue;
                        goto done;
                }
                if ((descriptor.revents & POLLOUT) != 0 && XtpPtyFlush(pty) < 0)
                        goto done;
                if ((descriptor.revents & POLLIN) == 0)
                        continue;
                amount = XtpPtyRead(pty, buffer, sizeof(buffer));
                if (amount > 0) {
                        if ((size_t)amount > payload_size - received ||
                            memcmp(buffer, payload + received, (size_t)amount) != 0)
                                goto done;
                        received += (size_t)amount;
                } else if (amount < 0 && errno != EINTR && errno != EAGAIN &&
                           errno != EWOULDBLOCK) {
                        goto done;
                }
        }
        if (XtpPtyPending(pty) == 0 && received == payload_size)
                result = 0;
done:
        free(payload);
        XtpPtyFree(pty);
        return result;
}

static uint64_t
SelfTestRowsBelow(const XtpTerminalScrollbar *state)
{
        uint64_t end = state->offset + state->length;

        return state->total > end ? state->total - end : 0;
}

static int
SelfTestScrollTtyOutput(void)
{
        XtpTerminal *terminal;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        int line;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL || XtpTerminalSetScrollbackLines(terminal, 64) != 0)
                goto done;
        for (line = 0; line < 12; ++line) {
                char text[32];
                int length = snprintf(text, sizeof(text), "anchor-%02d\r\n", line);

                XtpTerminalFeed(terminal, (const uint8_t *)text, (size_t)length);
        }
        if (XtpTerminalScrollTo(terminal, 3) != 0 ||
            XtpTerminalGetScrollbar(terminal, &before) != 0 ||
            XtpTerminalFeedOutput(terminal, (const uint8_t *)"next\r\n", 6, false) != 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 || after.total <= before.total ||
            after.offset <= before.offset ||
            SelfTestRowsBelow(&after) != SelfTestRowsBelow(&before))
                goto done;
        if (XtpTerminalScrollTo(terminal, 2) != 0 ||
            XtpTerminalFeedOutput(terminal, (const uint8_t *)"bottom\r\n", 8, true) != 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 ||
            after.offset + after.length != after.total)
                goto done;
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        size_t nonempty_cells;
        size_t frame_cells;
        size_t last_frame_cells;
        size_t begin_calls;
        size_t end_calls;
        Boolean saw_styled_cell;
        Boolean saw_inverse_cell;
        Boolean saw_hyperlink_cell;
        Boolean saw_wide_cell;
        Boolean saw_wide_tail;
        Boolean saw_selected_cell;
        size_t selected_cells;
        size_t underline_counts[XTP_UNDERLINE_DASHED + 1U];
        XtpColor underline_colors[4];
        XtpRenderFrame frame;
} SelfTestRender;

static void
SelfTestBegin(const XtpRenderFrame *frame, void *closure)
{
        SelfTestRender *render = closure;

        render->frame = *frame;
        render->frame_cells = 0;
        ++render->begin_calls;
}

static void
SelfTestCell(const XtpRenderCell *cell, void *closure)
{
        SelfTestRender *render = closure;

        ++render->frame_cells;
        if (cell->utf8_length != 0)
                ++render->nonempty_cells;
        if (cell->foreground.kind != XTP_COLOR_DEFAULT)
                render->saw_styled_cell = True;
        if (cell->inverse)
                render->saw_inverse_cell = True;
        if (cell->hyperlink)
                render->saw_hyperlink_cell = True;
        if (cell->width == 2)
                render->saw_wide_cell = True;
        if (cell->width == 0)
                render->saw_wide_tail = True;
        if (cell->selected)
                render->saw_selected_cell = True;
        if (cell->selected)
                ++render->selected_cells;
        if (cell->underline <= XTP_UNDERLINE_DASHED)
                ++render->underline_counts[cell->underline];
        if (cell->row == 0 && cell->column < 4U)
                render->underline_colors[cell->column] = cell->underline_color;
}

static void
SelfTestEnd(const XtpRenderFrame *frame, void *closure)
{
        SelfTestRender *render = closure;

        render->frame = *frame;
        render->last_frame_cells = render->frame_cells;
        ++render->end_calls;
}

static int
SelfTestCursorOnly(XtpTerminal *terminal, const XtpRenderer *renderer, SelfTestRender *render)
{
        static const uint8_t text[] = "abc";
        static const uint8_t cursor_left[] = "\033[D";
        size_t begin_calls;
        size_t end_calls;
        uint16_t column;

        if (XtpTerminalBackendIsStub())
                return 0;
        XtpTerminalFeed(terminal, text, sizeof(text) - 1U);
        if (XtpTerminalRender(terminal, renderer, render, false) != 0 ||
            !render->frame.cursor_visible || render->frame.cursor_column == 0)
                return -1;
        column = render->frame.cursor_column;
        begin_calls = render->begin_calls;
        end_calls = render->end_calls;
        XtpTerminalFeed(terminal, cursor_left, sizeof(cursor_left) - 1U);
        if (XtpTerminalRender(terminal, renderer, render, false) != 0 ||
            render->begin_calls != begin_calls + 1U || render->end_calls != end_calls + 1U ||
            render->last_frame_cells != 0 || !render->frame.cursor_visible ||
            render->frame.cursor_column + 1U != column)
                return -1;
        return 0;
}

static int
SelfTestReverseColors(const XtpRenderer *renderer)
{
        static const uint8_t styled[] = "\033[7mX\033[0m";
        static const uint8_t reverse_on[] = "\033[?5h";
        static const uint8_t reverse_off[] = "\033[?5l";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(8, 3, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, styled, sizeof(styled) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.frame.reverse_colors || !render.saw_inverse_cell)
                goto done;

        render.saw_inverse_cell = False;
        XtpTerminalFeed(terminal, reverse_on, sizeof(reverse_on) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.reverse_colors || !render.frame.full_repaint ||
            render.last_frame_cells != 24U || !render.saw_inverse_cell)
                goto done;

        XtpTerminalFeed(terminal, reverse_off, sizeof(reverse_off) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.reverse_colors || !render.frame.full_repaint ||
            render.last_frame_cells != 24U)
                goto done;
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestCursorBlinkPolicy(void)
{
        static const struct
        {
                XtpCursorBlinkPolicy policy;
                bool xor_policy;
                bool requested;
                bool effective;
        } cases[] = {
            {XTP_CURSOR_BLINK_DEFAULT_FALSE, false, false, false},
            {XTP_CURSOR_BLINK_DEFAULT_FALSE, false, true, true},
            {XTP_CURSOR_BLINK_DEFAULT_FALSE, true, false, false},
            {XTP_CURSOR_BLINK_DEFAULT_FALSE, true, true, true},
            {XTP_CURSOR_BLINK_DEFAULT_TRUE, false, false, true},
            {XTP_CURSOR_BLINK_DEFAULT_TRUE, false, true, true},
            {XTP_CURSOR_BLINK_DEFAULT_TRUE, true, false, true},
            {XTP_CURSOR_BLINK_DEFAULT_TRUE, true, true, false},
            {XTP_CURSOR_BLINK_ALWAYS, false, false, true},
            {XTP_CURSOR_BLINK_ALWAYS, true, true, true},
            {XTP_CURSOR_BLINK_NEVER, false, true, false},
            {XTP_CURSOR_BLINK_NEVER, true, false, false},
        };
        size_t item;

        for (item = 0; item < XtNumber(cases); ++item) {
                if (XtpCursorBlinkEffective(cases[item].policy, cases[item].xor_policy,
                                            cases[item].requested) != cases[item].effective)
                        return -1;
        }
        return 0;
}

typedef struct
{
        XtpTerminal *terminal;
        size_t calls;
        bool failed;
} SelfTestCursorBlinkReset;

static void
SelfTestRestoreCursorBlinkRequests(void *closure)
{
        SelfTestCursorBlinkReset *reset = closure;

        ++reset->calls;
        if (XtpTerminalSetCursorBlinkRequestsEnabled(reset->terminal, true) != 0)
                reset->failed = true;
}

static int
SelfTestCursorStyles(const XtpRenderer *renderer)
{
        static const uint8_t cancelled_strings[][32] = {
            "\033[?12l\033Pignored\030\033[?12h",
            "\033[?12l\033Pignored\032\033[?12h",
            "\033[?12l\033Pignored\033[?12h",
        };
        static const uint8_t excessive_modes[] = "\033[?12"
                                                 ";0;0;0;0;0;0;0;0"
                                                 ";0;0;0;0;0;0;0;0"
                                                 ";0;0;0;0;0;0;0;0h";
        static const uint8_t raw_dcs_c1[] = "\033[?12l\033Pq\234\233?12h\033\\";
        static const uint8_t raw_ignored_dcs_c1[] = "\033[?12l\033P1<\233?12h\033\\";
        static const uint8_t raw_osc_c1[] = "\033[?12l\033]0;\234\233?12h\007";
        static const struct
        {
                const char *sequence;
                XtpCursorShape shape;
                bool blinking;
        } cases[] = {
            {"\033[0 q", XTP_CURSOR_SHAPE_BLOCK, true},
            {"\033[0000000000000000000000000000000000000000000000000000000000000000000000000000 q",
             XTP_CURSOR_SHAPE_BLOCK, true},
            {"\033[1 q", XTP_CURSOR_SHAPE_BLOCK, true},
            {"\033[2 q", XTP_CURSOR_SHAPE_BLOCK, false},
            {"\033[4294967296 q", XTP_CURSOR_SHAPE_BLOCK, false},
            {"\033[9999999999999999999999999999999999999999 q", XTP_CURSOR_SHAPE_BLOCK, false},
            {"\033[3 q", XTP_CURSOR_SHAPE_UNDERLINE, true},
            {"\033[4 q", XTP_CURSOR_SHAPE_UNDERLINE, false},
            {"\033[5 q", XTP_CURSOR_SHAPE_BAR, true},
            {"\033[6 q", XTP_CURSOR_SHAPE_BAR, false},
        };
        XtpTerminal *terminal;
        SelfTestCursorBlinkReset reset = {0};
        XtpTerminalEffects effects = {
            .cursor_blink_reset = SelfTestRestoreCursorBlinkRequests,
            .closure = &reset,
        };
        SelfTestRender render = {0};
        size_t item;
        size_t reset_calls;
        const char *stage = "initial";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(8, 3, 8, 16, false);
        if (terminal == NULL)
                return -1;
        reset.terminal = terminal;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BLOCK ||
            render.frame.cursor_blink_requested)
                goto done;
        if (XtpTerminalSetCursorBlinkDefault(terminal, true) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested ||
            XtpTerminalSetCursorBlinkDefault(terminal, false) != 0)
                goto done;
        for (item = 0; item < XtNumber(cases); ++item) {
                size_t begin_calls = render.begin_calls;
                size_t end_calls = render.end_calls;

                XtpTerminalFeed(terminal, (const uint8_t *)cases[item].sequence,
                                strlen(cases[item].sequence));
                if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
                    render.begin_calls != begin_calls + 1U || render.end_calls != end_calls + 1U ||
                    render.frame.cursor_shape != cases[item].shape ||
                    render.frame.cursor_blink_requested != cases[item].blinking) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "cursor style case=%zu shape=%d blink=%s", item,
                               render.frame.cursor_shape,
                               render.frame.cursor_blink_requested ? "true" : "false");
                        goto done;
                }
        }
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12h", 6);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12s\033[?12l\033[?12r", 18);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12l", 6);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "empty cursor-style parameters";
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[;1 q", 6);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "excessive private-mode parameters";
        XtpTerminalFeed(terminal, excessive_modes, sizeof(excessive_modes) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?4294967296h", 14);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?4294967296;12h", 17);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?12l", 6);
        if (XtpTerminalSetCursorBlinkDefault(terminal, true) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        if (XtpTerminalSetCursorBlinkDefault(terminal, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[!p", 4);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "control-like OSC payload";
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]0;ignored [1 q\007",
                        strlen("\033]0;ignored [1 q\007"));
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]0;\xe2\x9c\x93 [1 q\033\\",
                        strlen("\033]0;\xe2\x9c\x93 [1 q\033\\"));
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "cancelled control strings";
        for (item = 0; item < XtNumber(cancelled_strings); ++item) {
                XtpTerminalFeed(terminal, cancelled_strings[item],
                                strlen((const char *)cancelled_strings[item]));
                if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
                    !render.frame.cursor_blink_requested)
                        goto done;
        }
        stage = "raw DCS C1 payload";
        XtpTerminalFeed(terminal, raw_dcs_c1, sizeof(raw_dcs_c1) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "raw ignored-DCS C1 payload";
        XtpTerminalFeed(terminal, raw_ignored_dcs_c1, sizeof(raw_ignored_dcs_c1) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "raw OSC C1 payload";
        XtpTerminalFeed(terminal, raw_osc_c1, sizeof(raw_osc_c1) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[", 2);
        XtpTerminalFeed(terminal, (const uint8_t *)"5 q", 3);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        reset_calls = reset.calls;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033c", 2);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested || reset.calls != reset_calls + 1U || reset.failed)
                goto done;
        if (XtpTerminalSetCursorBlinkRequestsEnabled(terminal, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[1 q\033[?12h", 11);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        if (XtpTerminalSetCursorBlinkRequestsEnabled(terminal, true) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[1 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested ||
            XtpTerminalSetCursorBlinkRequestsEnabled(terminal, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[2 q", 5);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033c", 2);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            render.frame.cursor_blink_requested)
                goto done;
        if (XtpTerminalSetCursorBlinkRequestsEnabled(terminal, false) != 0)
                goto done;
        stage = "reset followed by style";
        reset_calls = reset.calls;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[!p\033[1 q", 9);
        if (XtpTerminalRender(terminal, renderer, &render, false) != 0 ||
            !render.frame.cursor_blink_requested || reset.calls != reset_calls + 1U || reset.failed)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "cursor-style stage=%s reset-calls=%zu reset-failed=%s requested=%s", stage,
                       reset.calls, reset.failed ? "true" : "false",
                       render.frame.cursor_blink_requested ? "true" : "false");
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestHyperlinks(const XtpRenderer *renderer)
{
        static const uint8_t content[] =
            "\033]8;;http://example.com\033\\This is a link\033]8;;\033\\ plain";
        static const uint8_t expected[] = "http://example.com";
        static const uint8_t underline_styles[] =
            "plain "
            "\033[4:1msingle\033[4:2mdouble\033[4:3mcurly\033[4:4mdotted\033[4:5mdashed\033[24m";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        uint8_t *uri = NULL;
        size_t length = 0;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(30, 2, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, content, sizeof(content) - 1U);
        if (XtpTerminalHyperlinkAt(terminal, 0, 0, &uri, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(uri, expected, length) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            !render.saw_hyperlink_cell) {
                XtpLog(XTP_LOG_ERROR, "self-test", "OSC 8 URI length=%zu rendered=%s", length,
                       render.saw_hyperlink_cell ? "true" : "false");
                goto done;
        }
        free(uri);
        uri = NULL;
        length = 0;
        if (XtpTerminalHyperlinkAt(terminal, 14, 0, &uri, &length) != 0 || uri != NULL ||
            length != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "OSC 8 terminator left URI length=%zu", length);
                goto done;
        }
        XtpTerminalFree(terminal);
        terminal = XtpTerminalNewWithGraphemeWidth(80, 2, 8, 16, false);
        render = (SelfTestRender){0};
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, underline_styles, sizeof(underline_styles) - 1U);
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.underline_counts[XTP_UNDERLINE_NONE] == 0 ||
            render.underline_counts[XTP_UNDERLINE_SINGLE] == 0 ||
            render.underline_counts[XTP_UNDERLINE_DOUBLE] == 0 ||
            render.underline_counts[XTP_UNDERLINE_CURLY] == 0 ||
            render.underline_counts[XTP_UNDERLINE_DOTTED] == 0 ||
            render.underline_counts[XTP_UNDERLINE_DASHED] == 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "SGR underline styles did not render");
                goto done;
        }
        result = 0;
done:
        free(uri);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestHyperlinkHover(void)
{
        static const uint8_t same_uri[] = "https://same.example";
        static const uint8_t duplicate_labels[] =
            "\033]8;;https://same.example\033\\ONE\033]8;;\033\\  "
            "\033]8;;https://same.example\033\\TWO\033]8;;\033\\";
        VtHyperlinkTarget explicit_target = {
            .uri = (uint8_t *)same_uri,
            .length = sizeof(same_uri) - 1U,
            .first_cell = 10,
            .last_cell = 13,
        };
        VtHyperlinkTarget inferred_target = {
            .uri = (uint8_t *)same_uri,
            .length = sizeof(same_uri) - 1U,
            .inferred = True,
            .first_cell = 20,
            .last_cell = 23,
        };
        VtHyperlinkTarget derived_target = {0};
        Vt100Rec range_vt = {0};
        XtpTerminal *range_terminal = NULL;
        int top;
        int bottom;
        int result = -1;

        if (VtHyperlinkHoverUnderline(XTP_UNDERLINE_NONE) != XTP_UNDERLINE_SINGLE ||
            VtHyperlinkHoverUnderline(XTP_UNDERLINE_SINGLE) != XTP_UNDERLINE_DOUBLE ||
            VtHyperlinkHoverUnderline(XTP_UNDERLINE_DOUBLE) != XTP_UNDERLINE_SINGLE ||
            VtHyperlinkHoverUnderline(XTP_UNDERLINE_CURLY) != XTP_UNDERLINE_SINGLE ||
            VtHyperlinkHoverUnderline(XTP_UNDERLINE_DOTTED) != XTP_UNDERLINE_SINGLE ||
            VtHyperlinkHoverUnderline(XTP_UNDERLINE_DASHED) != XTP_UNDERLINE_SINGLE)
                goto done;
        VtDoubleUnderlineRows(10, 0, 20, &top, &bottom);
        if (top != 8 || bottom != 12)
                goto done;
        VtDoubleUnderlineRows(1, 0, 2, &top, &bottom);
        if (top != 0 || bottom != 2)
                goto done;
        VtDoubleUnderlineRows(0, 0, 0, &top, &bottom);
        if (top != 0 || bottom != 0)
                goto done;
        if (!VtHyperlinkTargetContainsCell(&explicit_target, 11, True, same_uri,
                                           sizeof(same_uri) - 1U) ||
            VtHyperlinkTargetContainsCell(&explicit_target, 9, True, same_uri,
                                          sizeof(same_uri) - 1U) ||
            VtHyperlinkTargetContainsCell(&explicit_target, 11, False, same_uri,
                                          sizeof(same_uri) - 1U) ||
            VtHyperlinkTargetContainsCell(&explicit_target, 11, True,
                                          (const uint8_t *)"https://other.example", 21) ||
            !VtHyperlinkTargetContainsCell(&inferred_target, 21, False, NULL, 0) ||
            VtHyperlinkTargetContainsCell(&inferred_target, 24, False, NULL, 0))
                goto done;
        if (XtpTerminalBackendIsStub()) {
                result = 0;
                goto done;
        }
        range_terminal = XtpTerminalNewWithGraphemeWidth(16, 1, 8, 16, false);
        if (range_terminal == NULL)
                goto done;
        range_vt.vt.terminal = range_terminal;
        range_vt.vt.frame_columns = 16;
        range_vt.vt.frame_rows = 1;
        range_vt.vt.frame_valid = True;
        range_vt.vt.frame_cells = calloc(16, sizeof(*range_vt.vt.frame_cells));
        if (range_vt.vt.frame_cells == NULL)
                goto done;
        XtpTerminalFeed(range_terminal, duplicate_labels, sizeof(duplicate_labels) - 1U);
        if (XtpTerminalHyperlinkAt(range_terminal, 1, 0, &derived_target.uri,
                                   &derived_target.length) != 0 ||
            derived_target.length != sizeof(same_uri) - 1U)
                goto done;
        VtExpandExplicitHyperlinkRange(&range_vt, 1, 0, &derived_target);
        if (derived_target.first_cell != 0 || derived_target.last_cell != 2 ||
            VtHyperlinkTargetContainsCell(&derived_target, 5, True, same_uri,
                                          sizeof(same_uri) - 1U))
                goto done;
        result = 0;
done:
        free(derived_target.uri);
        free(range_vt.vt.frame_cells);
        XtpTerminalFree(range_terminal);
        return result;
}

static int
SelfTestModes(XtpTerminal *terminal)
{
        XtpTerminalMode mode;

        for (mode = 0; mode < XTP_TERMINAL_MODE_COUNT; ++mode) {
                bool initial;
                bool changed;

                if (XtpTerminalGetMode(terminal, mode, &initial) != 0 ||
                    XtpTerminalSetMode(terminal, mode, !initial) != 0 ||
                    XtpTerminalGetMode(terminal, mode, &changed) != 0 || changed == initial ||
                    XtpTerminalSetMode(terminal, mode, initial) != 0)
                        return -1;
        }
        return 0;
}

static int
SelfTestSelection(const XtpRenderer *renderer)
{
        static const uint8_t content[] = "hello   world\r\nsecond line\r\n~/workspace/xterm-plus";
        static const char url_char_class[] =
            "33:48,35:48,37-38:48,43-47:48,58:48,61:48,63-64:48,95:48,126:48";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        uint8_t *text = NULL;
        uint8_t *paste = NULL;
        size_t length = 0;
        size_t paste_length = 0;
        XtpSelectionResult start_result;
        XtpSelectionResult extend_result;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(30, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalSelectionExtendStart(terminal, 0, 0, XTP_SELECTION_CELL) !=
            XTP_SELECTION_ERROR) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "selection extension without an existing selection did not fail");
                goto done;
        }
        XtpTerminalFeed(terminal, content, sizeof(content) - 1U);
        start_result = XtpTerminalSelectionStart(terminal, 0, 0, 1.0, 1.0, 1000000000U,
                                                 XTP_SELECTION_CELL, false);
        extend_result = XtpTerminalSelectionExtend(terminal, 4, 0, 38.0, 1.0, 30, 8, 0, 64, false);
        if (start_result != XTP_SELECTION_UNCHANGED || extend_result != XTP_SELECTION_CHANGED) {
                XtpLog(XTP_LOG_ERROR, "self-test", "selection gesture start=%d extend=%d",
                       start_result, extend_result);
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 4, 0, true);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 5 ||
            memcmp(text, "hello", 5) != 0 ||
            XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            !render.saw_selected_cell) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "selection output length=%zu text=%.*s selected=%s", length, (int)length,
                       text != NULL ? (const char *)text : "",
                       render.saw_selected_cell ? "yes" : "no");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendStart(terminal, 11, 0, XTP_SELECTION_CELL) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cell-granular right-click extension failed");
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 12 ||
            memcmp(text, "hello   worl", 12) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cell extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 6, 0, 51.0, 1.0, 2000000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 6, 0, true);
        if (XtpTerminalSelectionStart(terminal, 6, 0, 51.0, 1.0, 2100000000U, XTP_SELECTION_WORD,
                                      true) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "double-click whitespace selection failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 3) {
                XtpLog(XTP_LOG_ERROR, "self-test", "whitespace selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        if (XtpTerminalSelectionExtendStart(terminal, 11, 0, XTP_SELECTION_WORD) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word-granular right-click extension failed");
                goto done;
        }
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 8 ||
            memcmp(text, "   world", 8) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendActive(terminal, 0, 0, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "crossing word extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 8) {
                XtpLog(XTP_LOG_ERROR, "self-test", "crossed word selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3000000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line sequence initial click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 1, 0, true);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3100000000U, XTP_SELECTION_WORD,
                                      true) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line sequence double click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 1, 0, true);
        if (XtpTerminalSelectionStart(terminal, 1, 0, 9.0, 1.0, 3200000000U, XTP_SELECTION_LINE,
                                      true) != 1 ||
            XtpTerminalSelectionExtendStart(terminal, 2, 1, XTP_SELECTION_LINE) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line-granular right-click extension failed");
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 25 ||
            memcmp(text, "hello   world\nsecond line", 25) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "line extension length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 3, 2, 25.0, 33.0, 4000000000U, XTP_SELECTION_WORD,
                                      false) != 1 ||
            XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 9 ||
            memcmp(text, "workspace", 9) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "default charClass path length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        if (XtpTerminalSelectionExtendStart(terminal, 25, 2, XTP_SELECTION_WORD) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word extension into undrawn suffix failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 28) {
                XtpLog(XTP_LOG_ERROR, "self-test", "word-to-undrawn selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionExtendEnd(terminal);
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 20, 0, 161.0, 1.0, 4200000000U, XTP_SELECTION_CELL,
                                      false) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn initial click failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 20, 0, true);
        if (XtpTerminalSelectionStart(terminal, 20, 0, 161.0, 1.0, 4300000000U, XTP_SELECTION_WORD,
                                      true) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn double click selected text");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn double-click cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 0, 0, 1.0, 1.0, 4400000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 20, 0, 165.0, 1.0, 30, 8, 0, 64, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn suffix cell extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 30) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn suffix selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 0, 2, 1.0, 33.0, 4500000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 10, 3, 85.0, 49.0, 30, 8, 0, 64, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn row cell extension failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 60) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn row selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 10, 3, 85.0, 49.0, 4600000000U, XTP_SELECTION_LINE,
                                      false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn triple-click line failed");
                goto done;
        }
        render.selected_cells = 0;
        if (XtpTerminalRender(terminal, renderer, &render, true) != 0 ||
            render.selected_cells != 30) {
                XtpLog(XTP_LOG_ERROR, "self-test", "undrawn line selected cells=%zu",
                       render.selected_cells);
                goto done;
        }
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSetCharClass(terminal, url_char_class) != 0)
                goto done;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalSelectionStart(terminal, 3, 2, 25.0, 33.0, 4100000000U, XTP_SELECTION_WORD,
                                      false) != 1 ||
            XtpTerminalSelectionText(terminal, &text, &length) != 0 || length != 22 ||
            memcmp(text, "~/workspace/xterm-plus", 22) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "custom charClass path length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[?2004h", 8);
        if (XtpTerminalEncodePaste(terminal, (const uint8_t *)"hello\n", 6, &paste,
                                   &paste_length) != 0 ||
            paste_length != 18 || memcmp(paste, "\033[200~hello\n\033[201~", 18) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "paste output length=%zu", paste_length);
                goto done;
        }
        result = 0;
done:
        free(paste);
        free(text);
        XtpTerminalSelectionClear(terminal);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestScrollbackLimit(void)
{
        enum
        {
                configured_lines = 16500,
                emitted_lines = 20000,
                bytes_per_line = 8,
        };
        XtpTerminal *terminal;
        XtpTerminalScrollbar scrollbar = {0};
        char *content;
        int line;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        content = malloc((size_t)emitted_lines * bytes_per_line + 1U);
        if (terminal == NULL || content == NULL)
                goto done;
        for (line = 0; line < emitted_lines; ++line) {
                int length = snprintf(content + (size_t)line * bytes_per_line, bytes_per_line + 1U,
                                      "L%05d\r\n", line);

                if (length != bytes_per_line)
                        goto done;
        }
        if (XtpTerminalSetScrollbackLines(terminal, configured_lines) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)content, (size_t)emitted_lines * bytes_per_line);
        /*
         * libghostty prunes whole pages, so its documented line limit is an
         * estimate rather than an exact retained-row count. This lower bound
         * is deliberately loose enough for one page of granularity while
         * still catching the independent default byte cap.
         */
        if (XtpTerminalGetScrollbar(terminal, &scrollbar) != 0 ||
            scrollbar.total < configured_lines * 3U / 4U || scrollbar.total >= emitted_lines) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "saveLines not retained configured=%d emitted=%d total=%" PRIu64,
                       configured_lines, emitted_lines, scrollbar.total);
                goto done;
        }
        if (XtpTerminalSetScrollbackLines(terminal, 0) != 0 ||
            XtpTerminalGetScrollbar(terminal, &scrollbar) != 0 ||
            scrollbar.total != scrollbar.length) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "saveLines zero retained history total=%" PRIu64 " length=%" PRIu64,
                       scrollbar.total, scrollbar.length);
                goto done;
        }
        result = 0;
done:
        free(content);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestSelectionScrollback(void)
{
        static const char expected[] = "L00\nL01\nL02\nL03\nL04\nL05\nL06\nL07\nL08\nL09\nL10\nL11";
        XtpTerminal *terminal;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        XtpSelectionAutoscroll direction = XTP_SELECTION_AUTOSCROLL_NONE;
        uint8_t *text = NULL;
        size_t length = 0;
        int line;
        int ticks = 0;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(8, 3, 8, 16, false);
        if (terminal == NULL || XtpTerminalSetScrollbackLines(terminal, 64) != 0)
                goto done;
        for (line = 0; line < 12; ++line) {
                char row[8];
                int row_length =
                    snprintf(row, sizeof(row), line == 0 ? "L%02d" : "\r\nL%02d", line);

                XtpTerminalFeed(terminal, (const uint8_t *)row, (size_t)row_length);
        }
        if (XtpTerminalGetScrollbar(terminal, &before) != 0 || before.offset == 0 ||
            XtpTerminalSelectionStart(terminal, 2, 2, 23.0, 40.0, 5000000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48, false) != 1 ||
            XtpTerminalSelectionGetAutoscroll(terminal, &direction) != 0 ||
            direction != XTP_SELECTION_AUTOSCROLL_UP) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection did not begin at bottom offset=%" PRIu64
                       " direction=%d",
                       before.offset, direction);
                goto done;
        }
        do {
                before = after = (XtpTerminalScrollbar){0};
                if (XtpTerminalGetScrollbar(terminal, &before) != 0 ||
                    XtpTerminalSelectionAutoscrollTick(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48,
                                                       false) < 0 ||
                    XtpTerminalGetScrollbar(terminal, &after) != 0 ||
                    after.offset > before.offset) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "scrollback selection moved non-monotonically before=%" PRIu64
                               " after=%" PRIu64,
                               before.offset, after.offset);
                        goto done;
                }
                ++ticks;
        } while (after.offset != 0 && ticks < 64);
        if (after.offset != 0 ||
            XtpTerminalSelectionAutoscrollTick(terminal, 0, 0, 1.0, -1.0, 8, 8, 0, 48, false) < 0 ||
            XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection did not stop at oldest row offset=%" PRIu64,
                       after.offset);
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 0, 0, true);
        if (XtpTerminalSelectionText(terminal, &text, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(text, expected, sizeof(expected) - 1U) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback selection lost or duplicated text length=%zu text=%.*s", length,
                       (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        free(text);
        text = NULL;
        XtpTerminalSelectionClear(terminal);
        if (XtpTerminalScrollToBottom(terminal) != 0 ||
            XtpTerminalSelectionStart(terminal, 0, 2, 1.0, 40.0, 6000000000U, XTP_SELECTION_CELL,
                                      false) != 0 ||
            XtpTerminalSelectionExtend(terminal, 2, 2, 23.0, 40.0, 8, 8, 0, 48, false) != 1) {
                XtpLog(XTP_LOG_ERROR, "self-test", "scrollback Button-3 setup failed");
                goto done;
        }
        XtpTerminalSelectionEnd(terminal, 2, 2, true);
        if (XtpTerminalSelectionExtendStart(terminal, 0, 0, XTP_SELECTION_CELL) != 1)
                goto done;
        ticks = 0;
        do {
                before = after = (XtpTerminalScrollbar){0};
                if (XtpTerminalGetScrollbar(terminal, &before) != 0 ||
                    XtpTerminalScrollBy(terminal, -1) != 0 ||
                    XtpTerminalSelectionExtendActive(terminal, 0, 0, false) != 1 ||
                    XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset > before.offset)
                        goto done;
                ++ticks;
        } while (after.offset != 0 && ticks < 64);
        XtpTerminalSelectionExtendEnd(terminal);
        free(text);
        text = NULL;
        if (after.offset != 0 || XtpTerminalSelectionText(terminal, &text, &length) != 0 ||
            length != sizeof(expected) - 1U || memcmp(text, expected, sizeof(expected) - 1U) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "scrollback Button-3 extension lost or duplicated text length=%zu text=%.*s",
                       length, (int)length, text != NULL ? (const char *)text : "");
                goto done;
        }
        result = 0;
done:
        free(text);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestFocus(void)
{
        static const uint8_t enable[] = "\033[?1004h";
        static const uint8_t disable[] = "\033[?1004l";
        XtpTerminal *terminal;
        char encoded[8];
        size_t written = 0;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        XtpTerminalFeed(terminal, enable, sizeof(enable) - 1U);
        if (XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written != 3 || memcmp(encoded, "\033[I", 3) != 0)
                goto mismatch;
        if (XtpTerminalEncodeFocus(terminal, false, encoded, sizeof(encoded), &written) != 0 ||
            written != 3 || memcmp(encoded, "\033[O", 3) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, disable, sizeof(disable) - 1U);
        if (XtpTerminalEncodeFocus(terminal, false, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        result = 0;
        goto done;
mismatch:
        XtpLog(XTP_LOG_ERROR, "self-test", "focus encoding mismatch length=%zu bytes=%.*s", written,
               (int)written, encoded);
done:
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        uint8_t bytes[1024];
        size_t used;
        bool overflow;
} SelfTestPtyCapture;

static void
SelfTestCapturePty(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestPtyCapture *capture = closure;

        if (length > sizeof(capture->bytes) - capture->used) {
                capture->overflow = true;
                return;
        }
        memcpy(capture->bytes + capture->used, bytes, length);
        capture->used += length;
}

static bool
SelfTestPtyEquals(const SelfTestPtyCapture *capture, const uint8_t *expected, size_t length)
{
        return !capture->overflow && capture->used == length &&
               memcmp(capture->bytes, expected, length) == 0;
}

static int
SelfTestTerminalReports(void)
{
        static const uint8_t queries[] = "\033[14t\033[16t\033[18t\033[>q";
        static const uint8_t initial_expected[] =
            "\033[4;384;640t"
            "\033[6;16;8t"
            "\033[8;24;80t"
            "\033P>|" XTP_PROGRAM_NAME "(" XTP_VERSION ")\033\\";
        static const uint8_t resized_expected[] =
            "\033[4;540;900t"
            "\033[6;18;9t"
            "\033[8;30;100t"
            "\033P>|" XTP_PROGRAM_NAME "(" XTP_VERSION ")\033\\";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        XtpTerminalFeed(terminal, queries, sizeof(queries) - 1U);
        if (!SelfTestPtyEquals(&capture, initial_expected, sizeof(initial_expected) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalResize(terminal, 100, 30, 9, 18) != 0)
                goto done;
        XtpTerminalFeed(terminal, queries, sizeof(queries) - 1U);
        if (!SelfTestPtyEquals(&capture, resized_expected, sizeof(resized_expected) - 1U))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "terminal report mismatch length=%zu",
                       capture.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestSynchronizedOutput(void)
{
        static const uint8_t enable[] = "\033[?2026h\033[?2026$p";
        static const uint8_t disable[] = "\033[?2026l\033[?2026$p";
        static const uint8_t query[] = "\033[?2026$p";
        static const uint8_t set_report[] = "\033[?2026;1$y";
        static const uint8_t reset_report[] = "\033[?2026;2$y";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        bool enabled = true;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, &enabled) != 0 ||
            enabled)
                goto done;
        XtpTerminalFeed(terminal, enable, sizeof(enable) - 1U);
        if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, &enabled) != 0 ||
            !enabled || !SelfTestPtyEquals(&capture, set_report, sizeof(set_report) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, disable, sizeof(disable) - 1U);
        if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, &enabled) != 0 ||
            enabled || !SelfTestPtyEquals(&capture, reset_report, sizeof(reset_report) - 1U))
                goto done;
        /* The widget's timeout resets the mode; DECRQM must report that. */
        XtpTerminalFeed(terminal, enable, sizeof(enable) - 1U);
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalSetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
        if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_SYNCHRONIZED_OUTPUT, &enabled) != 0 ||
            enabled || !SelfTestPtyEquals(&capture, reset_report, sizeof(reset_report) - 1U))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "synchronized output mismatch enabled=%s report length=%zu",
                       enabled ? "true" : "false", capture.used);
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        SelfTestPtyCapture pty;
        XtpClipboardTarget target;
        uint8_t bytes[64];
        size_t length;
        bool clear;
        unsigned int writes;
        unsigned int reads;
        XtpClipboardResult read_result;
        const char *read_text;
} SelfTestClipboard;

static void
SelfTestClipboardPty(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestClipboard *harness = closure;

        SelfTestCapturePty(bytes, length, &harness->pty);
}

static XtpClipboardResult
SelfTestClipboardWrite(XtpClipboardTarget target, const uint8_t *bytes, size_t length, bool clear,
                       void *closure)
{
        SelfTestClipboard *harness = closure;

        ++harness->writes;
        harness->target = target;
        harness->clear = clear;
        harness->length = length < sizeof(harness->bytes) ? length : sizeof(harness->bytes);
        if (harness->length != 0)
                memcpy(harness->bytes, bytes, harness->length);
        return XTP_CLIPBOARD_SUCCESS;
}

static XtpClipboardResult
SelfTestClipboardRead(XtpClipboardTarget target, uint8_t **bytes, size_t *length, void *closure)
{
        SelfTestClipboard *harness = closure;

        ++harness->reads;
        harness->target = target;
        *bytes = NULL;
        *length = 0;
        if (harness->read_result != XTP_CLIPBOARD_SUCCESS)
                return harness->read_result;
        *length = strlen(harness->read_text);
        *bytes = malloc(*length + 1U);
        if (*bytes == NULL)
                return XTP_CLIPBOARD_UNAVAILABLE;
        memcpy(*bytes, harness->read_text, *length + 1U);
        return XTP_CLIPBOARD_SUCCESS;
}

static bool
SelfTestClipboardWritten(const SelfTestClipboard *harness, unsigned int writes,
                         XtpClipboardTarget target, const char *text, bool clear)
{
        size_t length = strlen(text);

        return harness->writes == writes && harness->target == target && harness->clear == clear &&
               harness->length == length && memcmp(harness->bytes, text, length) == 0;
}

static int
SelfTestOsc52(void)
{
        static const struct
        {
                const char *sequence;
                XtpClipboardTarget target;
                const char *text;
                bool clear;
        } writes[] = {
            {"\033]52;c;aGVsbG8=\a", XTP_CLIPBOARD_TARGET_CLIPBOARD, "hello", false},
            {"\033]52;p;d29ybGQ=\033\\", XTP_CLIPBOARD_TARGET_PRIMARY, "world", false},
            {"\033]52;s;eA==\a", XTP_CLIPBOARD_TARGET_SELECT, "x", false},
            {"\033]52;;eQ\a", XTP_CLIPBOARD_TARGET_CLIPBOARD, "y", false},
            {"\033]52;c;\a", XTP_CLIPBOARD_TARGET_CLIPBOARD, "", true},
        };
        /* libghostty drops these before any callback: bad base64 and multi-target lists. */
        static const char *const ignored[] = {"\033]52;c;!!!!\a", "\033]52;cp;eA==\a"};
        XtpTerminal *terminal;
        SelfTestClipboard harness = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestClipboardPty,
            .clipboard_write = SelfTestClipboardWrite,
            .clipboard_read = SelfTestClipboardRead,
            .closure = &harness,
        };
        size_t index;
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        for (index = 0; index < XtNumber(writes); ++index) {
                stage = writes[index].sequence;
                XtpTerminalFeed(terminal, (const uint8_t *)writes[index].sequence,
                                strlen(writes[index].sequence));
                if (!SelfTestClipboardWritten(&harness, (unsigned int)index + 1U,
                                              writes[index].target, writes[index].text,
                                              writes[index].clear))
                        goto done;
        }
        for (index = 0; index < XtNumber(ignored); ++index) {
                stage = ignored[index];
                XtpTerminalFeed(terminal, (const uint8_t *)ignored[index], strlen(ignored[index]));
                if (harness.writes != XtNumber(writes))
                        goto done;
        }
        stage = "query BEL";
        harness.read_result = XTP_CLIPBOARD_SUCCESS;
        harness.read_text = "hi";
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]52;c;?\a", 9);
        if (harness.reads != 1 || harness.target != XTP_CLIPBOARD_TARGET_CLIPBOARD ||
            !SelfTestPtyEquals(&harness.pty, (const uint8_t *)"\033]52;c;aGk=\a", 12))
                goto done;
        stage = "query ST";
        harness.pty = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]52;p;?\033\\", 10);
        if (harness.reads != 2 || harness.target != XTP_CLIPBOARD_TARGET_PRIMARY ||
            !SelfTestPtyEquals(&harness.pty, (const uint8_t *)"\033]52;p;aGk=\033\\", 13))
                goto done;
        stage = "query denied";
        harness.pty = (SelfTestPtyCapture){0};
        harness.read_result = XTP_CLIPBOARD_DENIED;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]52;s;?\a", 9);
        if (harness.reads != 3 || harness.target != XTP_CLIPBOARD_TARGET_SELECT ||
            !SelfTestPtyEquals(&harness.pty, (const uint8_t *)"\033]52;s;\a", 8))
                goto done;
        stage = "query without read effect";
        effects.clipboard_read = NULL;
        XtpTerminalSetEffects(terminal, &effects);
        harness.pty = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]52;c;?\a", 9);
        if (harness.reads != 3 || harness.pty.used != 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "OSC 52 mismatch stage=%s writes=%u reads=%u target=%d reply-bytes=%zu",
                       stage, harness.writes, harness.reads, (int)harness.target, harness.pty.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestTitleStack(void)
{
        XtpTitleStack stack = {0};
        XtpTitleEntry entry = {NULL, NULL};
        XtpTitleEntry both = {"icon-a", "title-a"};
        XtpTitleEntry window_only = {NULL, "title-b"};
        XtpTitleEntry empty = {NULL, NULL};
        unsigned int index;
        int result = -1;
        const char *stage = "empty pop";

        if (XtpTitleStackPop(&stack, 0, &entry))
                goto done;
        stage = "push and pop";
        if (!XtpTitleStackPush(&stack, &both, 0) || !XtpTitleStackPush(&stack, &window_only, 0) ||
            stack.used != 2 || !XtpTitleStackPop(&stack, 0, &entry) || stack.used != 1)
                goto done;
        /* The icon name of the window-only entry comes from the older slot. */
        if (entry.icon_name == NULL || strcmp(entry.icon_name, "icon-a") != 0 ||
            entry.window_name == NULL || strcmp(entry.window_name, "title-b") != 0)
                goto done;
        XtpTitleEntryFree(&entry);
        stage = "direct slot";
        if (!XtpTitleStackPush(&stack, &window_only, 5) || stack.used != 1 ||
            !XtpTitleStackPop(&stack, 5, &entry) || stack.used != 1 || entry.window_name == NULL ||
            strcmp(entry.window_name, "title-b") != 0)
                goto done;
        XtpTitleEntryFree(&entry);
        stage = "empty entry then pop";
        if (!XtpTitleStackPush(&stack, &empty, 0) || !XtpTitleStackPop(&stack, 0, &entry) ||
            entry.window_name == NULL || strcmp(entry.window_name, "title-a") != 0)
                goto done;
        XtpTitleEntryFree(&entry);
        stage = "overflow";
        XtpTitleStackClear(&stack);
        for (index = 0; index < XTP_TITLE_STACK_DEPTH + 2U; ++index) {
                char name[16];
                XtpTitleEntry numbered = {NULL, name};

                (void)snprintf(name, sizeof(name), "t%u", index);
                if (!XtpTitleStackPush(&stack, &numbered, 0))
                        goto done;
        }
        if (stack.used != XTP_TITLE_STACK_DEPTH + 2U || !XtpTitleStackPop(&stack, 0, &entry) ||
            strcmp(entry.window_name, "t11") != 0)
                goto done;
        XtpTitleEntryFree(&entry);
        for (index = 0; index < XTP_TITLE_STACK_DEPTH - 2U; ++index) {
                if (!XtpTitleStackPop(&stack, 0, &entry))
                        goto done;
                XtpTitleEntryFree(&entry);
        }
        /* The two oldest entries were overwritten by the wrap; xterm still
         * counts them, so two empty pops remain before the stack is empty. */
        if (!XtpTitleStackPop(&stack, 0, &entry) || strcmp(entry.window_name, "t2") != 0 ||
            stack.used != 2)
                goto done;
        XtpTitleEntryFree(&entry);
        if (!XtpTitleStackPop(&stack, 0, &entry) || entry.window_name != NULL ||
            !XtpTitleStackPop(&stack, 0, &entry) || entry.window_name != NULL || stack.used != 0 ||
            XtpTitleStackPop(&stack, 0, &entry))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "title stack mismatch stage=%s", stage);
        XtpTitleEntryFree(&entry);
        XtpTitleStackClear(&stack);
        return result;
}

typedef struct
{
        SelfTestPtyCapture pty;
        XtpTitleOp op;
        unsigned int target;
        unsigned int slot;
        unsigned int calls;
} SelfTestTitleOpHarness;

static void
SelfTestTitleOpsPty(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestTitleOpHarness *harness = closure;

        SelfTestCapturePty(bytes, length, &harness->pty);
}

static void
SelfTestTitleOpEffect(XtpTitleOp op, unsigned int target, unsigned int slot, void *closure)
{
        SelfTestTitleOpHarness *harness = closure;

        ++harness->calls;
        harness->op = op;
        harness->target = target;
        harness->slot = slot;
}

static int
SelfTestTitleOps(void)
{
        static const struct
        {
                const char *sequence;
                unsigned int op;
                unsigned int target;
                unsigned int slot;
        } cases[] = {
            {"\033[20t", 20, 0, 0},   {"\033[21t", 21, 0, 0},     {"\033[22t", 22, 0, 0},
            {"\033[22;2t", 22, 2, 0}, {"\033[23;1;4t", 23, 1, 4}, {"\033[22;0;10t", 22, 0, 10},
        };
        /* Not XTWINOPS title operations: private, intermediate, other finals. */
        static const char *const ignored[] = {"\033[?22;2t", "\033[22 t", "\033[22;2s", "\033[1t",
                                              "\033[24t"};
        XtpTerminal *terminal;
        SelfTestTitleOpHarness harness = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestTitleOpsPty,
            .title_op = SelfTestTitleOpEffect,
            .closure = &harness,
        };
        size_t index;
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        for (index = 0; index < XtNumber(cases); ++index) {
                stage = cases[index].sequence;
                XtpTerminalFeed(terminal, (const uint8_t *)cases[index].sequence,
                                strlen(cases[index].sequence));
                if (harness.calls != index + 1U || (unsigned int)harness.op != cases[index].op ||
                    harness.target != cases[index].target || harness.slot != cases[index].slot)
                        goto done;
        }
        for (index = 0; index < XtNumber(ignored); ++index) {
                stage = ignored[index];
                XtpTerminalFeed(terminal, (const uint8_t *)ignored[index], strlen(ignored[index]));
                if (harness.calls != XtNumber(cases))
                        goto done;
        }
        stage = "split across feeds";
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[2", 3);
        XtpTerminalFeed(terminal, (const uint8_t *)"3;", 2);
        XtpTerminalFeed(terminal, (const uint8_t *)"2t", 2);
        if (harness.calls != XtNumber(cases) + 1U || harness.op != XTP_TITLE_OP_POP ||
            harness.target != 2)
                goto done;
        stage = "libghostty keeps CSI 21 t silent";
        if (harness.pty.used != 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "title op mismatch stage=%s calls=%u op=%u",
                       stage, harness.calls, (unsigned int)harness.op);
        XtpTerminalFree(terminal);
        return result;
}

static void
SelfTestFeedText(XtpTerminal *terminal, const char *text)
{
        XtpTerminalFeed(terminal, (const uint8_t *)text, strlen(text));
}

static bool
SelfTestPtyEqualsText(const SelfTestPtyCapture *capture, const char *expected)
{
        return SelfTestPtyEquals(capture, (const uint8_t *)expected, strlen(expected));
}

static bool
SelfTestRgbEquals(XtpRgbColor color, uint8_t red, uint8_t green, uint8_t blue)
{
        return color.red == red && color.green == green && color.blue == blue;
}

static int
SelfTestDynamicColors(void)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        XtpRgbColor foreground = {0x10, 0x20, 0x30};
        XtpRgbColor background = {0x30, 0x40, 0x50};
        XtpRgbColor cursor = {0x50, 0x60, 0x70};
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, cursor) != 0)
                goto done;
        stage = "configured frame colors";
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            !render.frame.colors_valid ||
            !SelfTestRgbEquals(render.frame.foreground, 0x10, 0x20, 0x30) ||
            !SelfTestRgbEquals(render.frame.background, 0x30, 0x40, 0x50) ||
            !SelfTestRgbEquals(render.frame.cursor, 0x50, 0x60, 0x70))
                goto done;
        stage = "OSC 11 set repaints";
        SelfTestFeedText(terminal, "\033]11;#aabbcc\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !render.frame.full_repaint ||
            !SelfTestRgbEquals(render.frame.background, 0xaa, 0xbb, 0xcc) ||
            !SelfTestRgbEquals(render.frame.foreground, 0x10, 0x20, 0x30))
                goto done;
        stage = "unchanged colors keep partial frames";
        SelfTestFeedText(terminal, "x");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            render.frame.full_repaint)
                goto done;
        stage = "OSC 10 and 12 set";
        SelfTestFeedText(terminal, "\033]10;#010203\033\\\033]12;#040506\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !render.frame.full_repaint ||
            !SelfTestRgbEquals(render.frame.foreground, 0x01, 0x02, 0x03) ||
            !SelfTestRgbEquals(render.frame.cursor, 0x04, 0x05, 0x06))
                goto done;
        stage = "queries report displayed colors";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]10;?\a\033]11;?\033\\\033]12;?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033]10;rgb:0101/0202/0303\a"
                                             "\033]11;rgb:aaaa/bbbb/cccc\033\\"
                                             "\033]12;rgb:0404/0505/0606\a"))
                goto done;
        stage = "resets restore configured colors";
        SelfTestFeedText(terminal, "\033]110\a\033]111\033\\\033]112\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !render.frame.full_repaint ||
            !SelfTestRgbEquals(render.frame.foreground, 0x10, 0x20, 0x30) ||
            !SelfTestRgbEquals(render.frame.background, 0x30, 0x40, 0x50) ||
            !SelfTestRgbEquals(render.frame.cursor, 0x50, 0x60, 0x70))
                goto done;
        stage = "resource change while overridden";
        SelfTestFeedText(terminal, "\033]11;#aabbcc\a");
        background = (XtpRgbColor){0x70, 0x80, 0x90};
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, cursor) != 0 ||
            XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !SelfTestRgbEquals(render.frame.background, 0xaa, 0xbb, 0xcc))
                goto done;
        SelfTestFeedText(terminal, "\033]111\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !SelfTestRgbEquals(render.frame.background, 0x70, 0x80, 0x90))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "dynamic colors mismatch stage=%s", stage);
        XtpTerminalFree(terminal);
        return result;
}

static bool
SelfTestForegroundIs(XtpTerminal *terminal, uint8_t red, uint8_t green, uint8_t blue)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        SelfTestRender render = {0};

        return XtpTerminalRender(terminal, &renderer, &render, true) == 0 &&
               SelfTestRgbEquals(render.frame.foreground, red, green, blue);
}

static bool
SelfTestBackgroundIs(XtpTerminal *terminal, uint8_t red, uint8_t green, uint8_t blue)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        SelfTestRender render = {0};

        return XtpTerminalRender(terminal, &renderer, &render, true) == 0 &&
               SelfTestRgbEquals(render.frame.background, red, green, blue);
}

static int
SelfTestRequestOps(void)
{
        XtpFontOps font;
        XtpTcapOps tcap;
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        XtpMouseEvent event = {.action = XTP_MOUSE_ACTION_PRESS,
                               .button = XTP_MOUSE_BUTTON_LEFT,
                               .x = 8,
                               .y = 8,
                               .screen_width = 160,
                               .screen_height = 96,
                               .cell_width = 8,
                               .cell_height = 16};
        char encoded[128];
        size_t written = 0;
        const char *stage = "policy";
        int result = -1;

        XtpFontOpsParse("*,~Get*", &font);
        if (XtpFontOpAllowed(false, &font, XTP_FONT_OP_SET) ||
            !XtpFontOpAllowed(false, &font, XTP_FONT_OP_GET) ||
            !XtpFontOpAllowed(true, &font, XTP_FONT_OP_SET))
                return -1;
        XtpTcapOpsParse("gEtTcAp,unknown", &tcap);
        if (tcap.ignored_entries != 1 || XtpTcapOpAllowed(false, &tcap, XTP_TCAP_OP_GET) ||
            !XtpTcapOpAllowed(false, &tcap, XTP_TCAP_OP_SET))
                return -1;
        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "denied Tcap with unrelated replies";
        XtpTerminalSetTcapOpsPolicy(terminal, false, &tcap);
        SelfTestFeedText(terminal, "\033[6n\033P+q436f\033\\\033[6n");
        if (!SelfTestPtyEqualsText(&capture, "\033[1;1R\033[1;1R"))
                goto done;
        stage = "live Tcap enable";
        XtpTerminalSetTcapOpsPolicy(terminal, true, &tcap);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033P+q436f\033\\");
        if (capture.used < 8 || memcmp(capture.bytes, "\033P1+r", 5) != 0)
                goto done;
        stage = "Tcap deny list exceptions";
        XtpTcapOpsParse("*,~GetTcap", &tcap);
        XtpTerminalSetTcapOpsPolicy(terminal, false, &tcap);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033P+q436f\033\\");
        if (capture.used == 0)
                goto done;
        stage = "mouse reports";
        SelfTestFeedText(terminal, "\033[?1000h\033[?1006h\033[?1004h");
        if (!XtpTerminalMouseTracking(terminal) ||
            XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written == 0)
                goto done;
        XtpTerminalSetAllowMouseOps(terminal, false);
        if (XtpTerminalMouseTracking(terminal) ||
            XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 0 ||
            XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        XtpTerminalSetAllowMouseOps(terminal, true);
        if (!XtpTerminalMouseTracking(terminal) ||
            XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written == 0 ||
            XtpTerminalEncodeFocus(terminal, true, encoded, sizeof(encoded), &written) != 0 ||
            written == 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "request Ops mismatch stage=%s", stage);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestColorOpsPolicy(void)
{
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        XtpRgbColor foreground = {0x10, 0x20, 0x30};
        XtpRgbColor background = {0x30, 0x40, 0x50};
        XtpRgbColor cursor = {0x50, 0x60, 0x70};
        XtpColorOps ops;
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, cursor) != 0)
                goto done;

        stage = "GetColor denied: sets apply, queries stay silent";
        XtpColorOpsParse("GetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#aabbcc\a\033]11;?\a\033]4;1;?\a");
        if (!SelfTestBackgroundIs(terminal, 0xaa, 0xbb, 0xcc) || capture.used != 25U ||
            memcmp(capture.bytes, "\033]4;1;rgb:", 10) != 0)
                goto done;

        stage = "SetColor denied: sets and resets ignored, queries answered";
        XtpColorOpsParse("SetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#ddeeff\a\033]111\a\033]11;?\a");
        if (!SelfTestBackgroundIs(terminal, 0xaa, 0xbb, 0xcc) ||
            !SelfTestPtyEqualsText(&capture, "\033]11;rgb:aaaa/bbbb/cccc\a"))
                goto done;

        stage = "GetAnsiColor denied: palette writes apply, palette queries stay silent";
        XtpColorOpsParse("GetAnsiColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]4;1;#ff0000;2;?\a\033]4;3;?\a\033]11;?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033]11;rgb:aaaa/bbbb/cccc\a"))
                goto done;
        XtpColorOpsParse("", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]4;1;?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033]4;1;rgb:ffff/0000/0000\a"))
                goto done;

        stage = "SetColor denied: permitted queries in a mixed list still answer";
        XtpColorOpsParse("SetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]111\a", 6);
        XtpColorOpsParse("SetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]10;#aabbcc;?\a");
        if (!SelfTestForegroundIs(terminal, 0x10, 0x20, 0x30) ||
            !SelfTestPtyEqualsText(&capture, "\033]11;rgb:aaaa/bbbb/cccc\a"))
                goto done;
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]10;?;#112233\033\\");
        if (!SelfTestBackgroundIs(terminal, 0xaa, 0xbb, 0xcc) ||
            !SelfTestPtyEqualsText(&capture, "\033]10;rgb:1010/2020/3030\033\\"))
                goto done;

        stage = "GetColor denied: permitted sets in a mixed list still apply";
        XtpColorOpsParse("GetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]10;#445566;?\a");
        if (!SelfTestForegroundIs(terminal, 0x44, 0x55, 0x66) || capture.used != 0)
                goto done;
        SelfTestFeedText(terminal, "\033]10;?;#778899\033\\");
        if (!SelfTestBackgroundIs(terminal, 0x77, 0x88, 0x99) || capture.used != 0)
                goto done;

        stage = "denied set split across feeds is still undone";
        XtpColorOpsParse("SetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        {
                static const char split_case[] = "\033]11;#000000;?\033\\";
                size_t split;

                for (split = 0; split <= strlen(split_case); ++split) {
                        capture = (SelfTestPtyCapture){0};
                        XtpTerminalFeed(terminal, (const uint8_t *)split_case, split);
                        XtpTerminalFeed(terminal, (const uint8_t *)split_case + split,
                                        strlen(split_case) - split);
                        if (!SelfTestBackgroundIs(terminal, 0x77, 0x88, 0x99) ||
                            !SelfTestPtyEqualsText(&capture, "\033]12;rgb:5050/6060/7070\033\\"))
                                goto done;
                }
        }
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        SelfTestFeedText(terminal, "\033]110\a\033]111\a");

        stage = "split terminator never exposes a denied color";
        XtpColorOpsParse("SetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#ffffff\033");
        if (!SelfTestBackgroundIs(terminal, 0x30, 0x40, 0x50))
                goto done;
        SelfTestFeedText(terminal, "\\");
        if (!SelfTestBackgroundIs(terminal, 0x30, 0x40, 0x50) || capture.used != 0)
                goto done;

        stage = "a denied list leaves the parser ready for the next control";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#ffffff\033\\\033[6n");
        if (!SelfTestBackgroundIs(terminal, 0x30, 0x40, 0x50) ||
            !SelfTestPtyEqualsText(&capture, "\033[1;1R"))
                goto done;
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#ffffff\033[6n");
        if (!SelfTestBackgroundIs(terminal, 0x30, 0x40, 0x50) ||
            !SelfTestPtyEqualsText(&capture, "\033[1;1R"))
                goto done;

        stage = "a denied set equal to the default installs no override";
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        SelfTestFeedText(terminal, "\033]111\a");
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        SelfTestFeedText(terminal, "\033]11;#304050\a");
        background = (XtpRgbColor){0x80, 0x90, 0xa0};
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, cursor) != 0 ||
            !SelfTestBackgroundIs(terminal, 0x80, 0x90, 0xa0))
                goto done;
        background = (XtpRgbColor){0x30, 0x40, 0x50};
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, cursor) != 0)
                goto done;

        stage = "decisions are per item across a policy change";
        SelfTestFeedText(terminal, "\033]10;#111111;");
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "#222222\a");
        if (!SelfTestForegroundIs(terminal, 0x10, 0x20, 0x30) ||
            !SelfTestBackgroundIs(terminal, 0x22, 0x22, 0x22) || capture.used != 0)
                goto done;
        XtpColorOpsParse("GetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        SelfTestFeedText(terminal, "\033]10;?;");
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033]11;rgb:2222/2222/2222\a"))
                goto done;
        SelfTestFeedText(terminal, "\033]110\a\033]111\a");

        stage = "palette denial covers every query in a long list";
        XtpColorOpsParse("GetAnsiColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        {
                unsigned int pair;

                SelfTestFeedText(terminal, "\033]4");
                for (pair = 0; pair < 70U; ++pair)
                        SelfTestFeedText(terminal, ";1;?");
                SelfTestFeedText(terminal, "\a");
        }
        if (capture.used != 0)
                goto done;

        stage = "palette decisions follow the reply index, not the query order";
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        SelfTestFeedText(terminal, "\033]4;256;?;");
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "1;?\a");
        if (capture.used != 0)
                goto done;
        SelfTestFeedText(terminal, "\033]4;1;?;");
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "2;?\a");
        if (capture.used != 25U || memcmp(capture.bytes, "\033]4;2;rgb:", 10) != 0)
                goto done;
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]4;3;#ff0000;3;?;4;?\a");
        if (capture.used != 0)
                goto done;

        stage = "index spellings libghostty accepts stay denied";
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]4;+1;?\a\033]4;000000000001;?\a");
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]4;\0001;?\a", 10);
        XtpTerminalFeed(terminal, (const uint8_t *)"\033]4;1\000;?\a", 10);
        if (capture.used != 0)
                goto done;

        stage = "repeated indices keep each occurrence's decision";
        SelfTestFeedText(terminal, "\033]4;1;?;");
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "1;?\a");
        if (capture.used != 25U || memcmp(capture.bytes, "\033]4;1;rgb:", 10) != 0)
                goto done;
        SelfTestFeedText(terminal, "\033]4;1;?;");
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "1;?\a");
        if (capture.used != 25U || memcmp(capture.bytes, "\033]4;1;rgb:", 10) != 0)
                goto done;

        stage = "earlier replies in the same feed are not filtered";
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[6n\033]4;1;?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033[1;1R"))
                goto done;
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[6n");
        SelfTestFeedText(terminal, "\033]4;1;?\a");
        if (!SelfTestPtyEqualsText(&capture, "\033[1;1R"))
                goto done;
        XtpColorOpsParse("GetColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, false, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[6n\033]11;?\a\033[6n");
        if (!SelfTestPtyEqualsText(&capture, "\033[1;1R\033[1;1R"))
                goto done;
        XtpColorOpsParse("GetAnsiColor", &ops);

        stage = "allowColorOps overrides the list";
        XtpColorOpsParse("SetColor,GetColor,GetAnsiColor", &ops);
        XtpTerminalSetColorOpsPolicy(terminal, true, &ops);
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]111\a\033]11;?\a");
        if (!SelfTestBackgroundIs(terminal, 0x30, 0x40, 0x50) ||
            !SelfTestPtyEqualsText(&capture, "\033]11;rgb:3030/4040/5050\a"))
                goto done;

        stage = "wildcards and negation";
        XtpColorOpsParse("*,~get*", &ops);
        if (!ops.disallowed[XTP_COLOR_OP_SET_COLOR] || ops.disallowed[XTP_COLOR_OP_GET_COLOR] ||
            ops.disallowed[XTP_COLOR_OP_GET_ANSI_COLOR] || ops.ignored_entries != 0)
                goto done;
        XtpColorOpsParse("Get?olor,SetXprop", &ops);
        if (!ops.disallowed[XTP_COLOR_OP_GET_COLOR] || ops.disallowed[XTP_COLOR_OP_SET_COLOR] ||
            ops.ignored_entries != 1)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "color-ops policy mismatch stage=%s bytes=%zu",
                       stage, capture.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestColorScheme(void)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        XtpRgbColor foreground = {0x00, 0x00, 0x00};
        XtpRgbColor background = {0xff, 0xff, 0xff};
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalSetDefaultColors(terminal, foreground, background, foreground) != 0)
                goto done;
        stage = "light query";
        SelfTestFeedText(terminal, "\033[?996n");
        if (!XtpTerminalBackgroundIsLight(terminal) ||
            !SelfTestPtyEqualsText(&capture, "\033[?997;2n"))
                goto done;
        stage = "dark after OSC 11";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033]11;#101010\a\033[?996n");
        if (XtpTerminalBackgroundIsLight(terminal) ||
            !SelfTestPtyEqualsText(&capture, "\033[?997;1n"))
                goto done;
        stage = "DECSCNM swaps the displayed background";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[?5h\033[?996n\033[?5l");
        if (!SelfTestPtyEqualsText(&capture, "\033[?997;1n"))
                goto done;
        stage = "no unsolicited report without mode 2031";
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0)
                goto done;
        SelfTestFeedText(terminal, "\033]111\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 || capture.used != 0)
                goto done;
        stage = "mode 2031 reports scheme changes at render";
        SelfTestFeedText(terminal, "\033[?2031h\033]11;#101010\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !SelfTestPtyEqualsText(&capture, "\033[?997;1n"))
                goto done;
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "x");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 || capture.used != 0)
                goto done;
        SelfTestFeedText(terminal, "\033]111\a");
        if (XtpTerminalRender(terminal, &renderer, &render, false) != 0 ||
            !SelfTestPtyEqualsText(&capture, "\033[?997;2n"))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "color scheme mismatch stage=%s bytes=%zu",
                       stage, capture.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestWorkingDirectoryDecode(void)
{
        static const struct
        {
                const char *report;
                const char *hostname;
                XtpWorkingDirectoryStatus status;
                const char *path;
        } cases[] = {
            {"", "xtp-host.example.org", XTP_WORKING_DIRECTORY_CLEARED, NULL},
            {"file:///tmp", "xtp-host.example.org", XTP_WORKING_DIRECTORY_SET, "/tmp"},
            {"file:///tmp", NULL, XTP_WORKING_DIRECTORY_SET, "/tmp"},
            {"file://localhost/tmp/a%20b", "xtp-host.example.org", XTP_WORKING_DIRECTORY_SET,
             "/tmp/a b"},
            {"FILE://LOCALHOST/x", "xtp-host.example.org", XTP_WORKING_DIRECTORY_SET, "/x"},
            {"file://xtp-host.example.org/home/u/%C3%A9t%C3%A9", "xtp-host.example.org",
             XTP_WORKING_DIRECTORY_SET, "/home/u/\xc3\xa9t\xc3\xa9"},
            {"file://XTP-HOST/x", "xtp-host.example.org", XTP_WORKING_DIRECTORY_SET, "/x"},
            {"file://xtp-host.example.org/x", "xtp-host", XTP_WORKING_DIRECTORY_SET, "/x"},
            {"file:///tmp/x?query#fragment", "xtp-host", XTP_WORKING_DIRECTORY_SET, "/tmp/x"},
            {"/bare/path with space/\xc3\xa9", "xtp-host", XTP_WORKING_DIRECTORY_SET,
             "/bare/path with space/\xc3\xa9"},
            {"file://other.example.org/x", "xtp-host.example.org", XTP_WORKING_DIRECTORY_REMOTE,
             NULL},
            {"file://xtp-host/x", NULL, XTP_WORKING_DIRECTORY_REMOTE, NULL},
            {"file://xtp-hostile/x", "xtp-host", XTP_WORKING_DIRECTORY_REMOTE, NULL},
            {"file://xtp-host.example@elsewhere.invalid/tmp", "xtp-host",
             XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://user@localhost/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp-host:22/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp-host?x/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp-host#/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp-host\n/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp%2Dhost/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://[::1]/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://localhost /tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://xtp-host.example.org", "xtp-host.example.org", XTP_WORKING_DIRECTORY_INVALID,
             NULL},
            {"file:///tmp/%2", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file:///tmp/%zz", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file:///tmp/a%00b", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file:///tmp/a%0Ab", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file:///tmp/a\x01b", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"/bare/\x7f", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file:/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"file://", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"ssh://xtp-host/tmp", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"relative/path", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
            {"C:\\Users", "xtp-host", XTP_WORKING_DIRECTORY_INVALID, NULL},
        };
        size_t index;

        for (index = 0; index < XtNumber(cases); ++index) {
                char *path = "untouched";
                XtpWorkingDirectoryStatus status = XtpWorkingDirectoryDecode(
                    (const uint8_t *)cases[index].report, strlen(cases[index].report),
                    cases[index].hostname, &path);
                bool ok = status == cases[index].status &&
                          (cases[index].path == NULL
                               ? path == NULL
                               : path != NULL && strcmp(path, cases[index].path) == 0);

                if (!ok) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "working directory decode mismatch report=%s status=%d path=%s",
                               cases[index].report, (int)status, path != NULL ? path : "(null)");
                        free(path);
                        return -1;
                }
                free(path);
        }
        return 0;
}

typedef struct
{
        unsigned int calls;
        char last[64];
        size_t last_length;
        unsigned int dropped_calls;
        size_t dropped_length;
} SelfTestWorkingDirectoryHarness;

static void
SelfTestWorkingDirectoryDropped(size_t length, void *closure)
{
        SelfTestWorkingDirectoryHarness *harness = closure;

        ++harness->dropped_calls;
        harness->dropped_length = length;
}

/* An OSC 7 whose payload is `length` bytes, split for feeding in two chunks. */
static char *
SelfTestLongDirectoryReport(size_t length)
{
        char *report = malloc(length + 8U);

        if (report == NULL)
                return NULL;
        memcpy(report, "\033]7;/", 5);
        memset(report + 5, 'd', length - 1U);
        memcpy(report + 4 + length, "\033\\", 3);
        return report;
}

static void
SelfTestWorkingDirectoryEffect(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestWorkingDirectoryHarness *harness = closure;

        ++harness->calls;
        harness->last_length = length;
        memset(harness->last, 0, sizeof(harness->last));
        if (length < sizeof(harness->last))
                memcpy(harness->last, bytes, length);
}

static int
SelfTestWorkingDirectoryEffectDelivery(void)
{
        XtpTerminal *terminal;
        SelfTestWorkingDirectoryHarness harness = {0};
        XtpTerminalEffects effects = {
            .working_directory_changed = SelfTestWorkingDirectoryEffect,
            .working_directory_dropped = SelfTestWorkingDirectoryDropped,
            .closure = &harness,
        };
        const char *stage = "setup";
        char *report = NULL;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "OSC 7 delivers the raw URI";
        SelfTestFeedText(terminal, "\033]7;file://localhost/tmp/a%20b\033\\");
        if (harness.calls != 1 || strcmp(harness.last, "file://localhost/tmp/a%20b") != 0)
                goto done;
        stage = "OSC 7 with BEL terminator";
        SelfTestFeedText(terminal, "\033]7;file:///tmp\a");
        if (harness.calls != 2 || strcmp(harness.last, "file:///tmp") != 0)
                goto done;
        stage = "an empty OSC 7 reports a clear";
        SelfTestFeedText(terminal, "\033]7;\033\\");
        if (harness.calls != 3 || harness.last_length != 0)
                goto done;
        stage = "unrelated OSCs do not fire the effect";
        SelfTestFeedText(terminal, "\033]2;title\033\\\033]133;A\033\\");
        if (harness.calls != 3 || harness.dropped_calls != 0)
                goto done;
        stage = "a report inside the core's capture limit is delivered";
        report = SelfTestLongDirectoryReport(2000);
        if (report == NULL)
                goto done;
        SelfTestFeedText(terminal, report);
        free(report);
        if (harness.calls != 4 || harness.last_length != 2000 || harness.dropped_calls != 0)
                goto done;
        stage = "a report beyond the capture limit is reported as dropped";
        report = SelfTestLongDirectoryReport(2300);
        if (report == NULL)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)report, 1000);
        if (harness.dropped_calls != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)report + 1000, strlen(report) - 1000U);
        free(report);
        if (harness.calls != 4 || harness.dropped_calls != 1 || harness.dropped_length != 2300)
                goto done;
        stage = "a BEL-terminated oversized report is dropped too";
        report = SelfTestLongDirectoryReport(2300);
        if (report == NULL)
                goto done;
        report[strlen(report) - 2U] = '\a';
        report[strlen(report) - 1U] = '\0';
        SelfTestFeedText(terminal, report);
        free(report);
        if (harness.calls != 4 || harness.dropped_calls != 2 || harness.dropped_length != 2300)
                goto done;
        stage = "delivery resumes after a dropped report";
        SelfTestFeedText(terminal, "\033]7;file:///tmp\033\\");
        if (harness.calls != 5 || harness.dropped_calls != 2 ||
            strcmp(harness.last, "file:///tmp") != 0)
                goto done;
        stage = "an earlier report in the same feed does not mask a dropped one";
        report = SelfTestLongDirectoryReport(2300);
        if (report == NULL)
                goto done;
        {
                static const char earlier[] = "\033]1337;CurrentDir=/tmp\033\\";
                size_t earlier_length = strlen(earlier);
                size_t report_length = strlen(report);
                char *combined = malloc(earlier_length + report_length + 1U);

                if (combined == NULL) {
                        free(report);
                        goto done;
                }
                memcpy(combined, earlier, earlier_length);
                memcpy(combined + earlier_length, report, report_length + 1U);
                SelfTestFeedText(terminal, combined);
                free(combined);
                if (harness.calls != 6 || harness.dropped_calls != 3 ||
                    harness.dropped_length != 2300 || strcmp(harness.last, "/tmp") != 0) {
                        free(report);
                        goto done;
                }
                stage = "the same reports in separate feeds give the same result";
                SelfTestFeedText(terminal, earlier);
                SelfTestFeedText(terminal, report);
                free(report);
                if (harness.calls != 7 || harness.dropped_calls != 4 ||
                    harness.dropped_length != 2300 || strcmp(harness.last, "/tmp") != 0)
                        goto done;
        }
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "working directory effect mismatch stage=%s calls=%u dropped=%u last=%s",
                       stage, harness.calls, harness.dropped_calls, harness.last);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestTerminfoName(void)
{
        static const char query[] = "\033P+q544e\033\\";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "unset name answers nothing";
        SelfTestFeedText(terminal, query);
        if (capture.used != 0)
                goto done;
        stage = "configured name is reported hex-encoded";
        if (XtpTerminalSetTerminfoName(terminal, "xtp-test") != 0)
                goto done;
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, query);
        if (!SelfTestPtyEqualsText(&capture, "\033P1+r544E=7874702D74657374\033\\"))
                goto done;
        stage = "Tcap Ops denial keeps TN silent";
        {
                XtpTcapOps tcap;

                XtpTcapOpsParse(XTP_TCAP_OPS_DEFAULT_DISALLOWED, &tcap);
                XtpTerminalSetTcapOpsPolicy(terminal, false, &tcap);
        }
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, query);
        XtpTerminalSetTcapOpsPolicy(terminal, true, NULL);
        if (capture.used != 0)
                goto done;
        stage = "over-long names are rejected";
        {
                char *text = malloc(130);

                if (text == NULL)
                        goto done;
                memset(text, 'x', 129);
                text[129] = '\0';
                if (XtpTerminalSetTerminfoName(terminal, text) == 0) {
                        free(text);
                        goto done;
                }
                free(text);
        }
        stage = "a rejected name leaves TN unanswered rather than stale";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, query);
        if (capture.used != 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "terminfo name mismatch stage=%s bytes=%zu reply=%.*s", stage, capture.used,
                       (int)capture.used, (const char *)capture.bytes);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestAnswerback(void)
{
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "default is silent";
        SelfTestFeedText(terminal, "\005");
        if (capture.used != 0)
                goto done;
        stage = "configured string is sent verbatim";
        if (XtpTerminalSetAnswerback(terminal, "xterm+ ok\033") != 0)
                goto done;
        SelfTestFeedText(terminal, "\005");
        if (!SelfTestPtyEqualsText(&capture, "xterm+ ok\033"))
                goto done;
        stage = "every request is answered";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\005a\005");
        if (!SelfTestPtyEqualsText(&capture, "xterm+ ok\033xterm+ ok\033"))
                goto done;
        stage = "reply filters do not rewrite or block the answerback";
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalSetAnswerback(terminal, "\033[?12;1$y") != 0)
                goto done;
        SelfTestFeedText(terminal, "\005");
        if (!SelfTestPtyEqualsText(&capture, "\033[?12;1$y"))
                goto done;
        {
                XtpTcapOps tcap;

                XtpTcapOpsParse(XTP_TCAP_OPS_DEFAULT_DISALLOWED, &tcap);
                XtpTerminalSetTcapOpsPolicy(terminal, false, &tcap);
        }
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalSetAnswerback(terminal, "\033P1+rHELLO\033\\") != 0)
                goto done;
        SelfTestFeedText(terminal, "\005");
        if (!SelfTestPtyEqualsText(&capture, "\033P1+rHELLO\033\\"))
                goto done;
        XtpTerminalSetTcapOpsPolicy(terminal, true, NULL);
        stage = "long strings are delivered whole";
        {
                char *text = malloc(301);
                size_t index;

                if (text == NULL)
                        goto done;
                for (index = 0; index < 300; ++index)
                        text[index] = (char)('a' + (index % 26));
                text[300] = '\0';
                capture = (SelfTestPtyCapture){0};
                if (XtpTerminalSetAnswerback(terminal, text) != 0) {
                        free(text);
                        goto done;
                }
                SelfTestFeedText(terminal, "\005");
                if (!SelfTestPtyEqualsText(&capture, text)) {
                        free(text);
                        goto done;
                }
                free(text);
        }
        stage = "empty and NULL return to silence";
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalSetAnswerback(terminal, "") != 0)
                goto done;
        SelfTestFeedText(terminal, "\005");
        if (XtpTerminalSetAnswerback(terminal, NULL) != 0)
                goto done;
        SelfTestFeedText(terminal, "\005");
        if (capture.used != 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "answerback mismatch stage=%s bytes=%zu", stage,
                       capture.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestDeviceAttributesFirmware(void)
{
        static const struct
        {
                const char *version;
                unsigned int firmware;
        } cases[] = {
            {"0.7.0-dev", 700}, {"0.6.1", 601},    {"1.2.3", 10203},
            {"0.7", 700},       {"3", 30000},      {"7.0.0", 65535},
            {"1.250.3", 19903}, {"0.100.0", 9900}, {"6.55.35", 65535},
            {"6.55.36", 65535}, {"", 0},           {"dev", 0},
            {".7.0", 0},        {"0.7.0.9", 700},  {NULL, 0},
        };
        size_t index;

        for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
                unsigned int firmware = XtpDeviceAttributesFirmware(cases[index].version);

                if (firmware != cases[index].firmware) {
                        XtpLog(XTP_LOG_ERROR, "self-test",
                               "firmware mismatch version=%s expected=%u actual=%u",
                               cases[index].version != NULL ? cases[index].version : "(null)",
                               cases[index].firmware, firmware);
                        return -1;
                }
        }
        return 0;
}

static int
SelfTestDeviceAttributes(void)
{
        static const char *const queries[] = {"\033[c",   "\033[0c", "\033[>c",
                                              "\033[>0c", "\033[=c", "\033[=0c"};
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        char da2[32];
        char expected[256];
        const char *stage = "exact replies";
        size_t index;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        snprintf(da2, sizeof(da2), "\033[>%u;%u;0c", XTP_DA2_DEVICE_TYPE,
                 XtpDeviceAttributesFirmware(XTP_VERSION));
        for (index = 0; index < sizeof(queries) / sizeof(queries[0]); ++index) {
                const char *reply = index < 2 ? XTP_DA1_REPLY : index < 4 ? da2 : XTP_DA3_REPLY;

                capture = (SelfTestPtyCapture){0};
                SelfTestFeedText(terminal, queries[index]);
                if (!SelfTestPtyEqualsText(&capture, reply))
                        goto done;
        }
        stage = "neighboring queries survive in one feed";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[6n\033[c\033[>c\033[=c\033[>q\033[?2026$p");
        snprintf(expected, sizeof(expected),
                 "\033[1;1R" XTP_DA1_REPLY "%s" XTP_DA3_REPLY "\033P>|" XTP_PROGRAM_NAME
                 "(" XTP_VERSION ")\033\\\033[?2026;2$y",
                 da2);
        if (!SelfTestPtyEqualsText(&capture, expected))
                goto done;
        stage = "split feeds produce the same bytes";
        capture = (SelfTestPtyCapture){0};
        for (index = 0; index < sizeof(queries) / sizeof(queries[0]); ++index) {
                const char *query = queries[index];
                size_t position;

                for (position = 0; query[position] != '\0'; ++position)
                        XtpTerminalFeed(terminal, (const uint8_t *)query + position, 1);
        }
        snprintf(expected, sizeof(expected),
                 XTP_DA1_REPLY XTP_DA1_REPLY "%s%s" XTP_DA3_REPLY XTP_DA3_REPLY, da2, da2);
        if (!SelfTestPtyEqualsText(&capture, expected))
                goto done;
        /* xterm 411 ignores a nonzero parameter; the core answers, as the drift ledger records. */
        stage = "nonzero parameter is answered by the core";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[1c");
        if (!SelfTestPtyEqualsText(&capture, XTP_DA1_REPLY))
                goto done;
        stage = "other intermediates stay silent";
        capture = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[?c\033[!c\033[>=c");
        if (capture.used != 0)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "device attributes mismatch stage=%s bytes=%zu",
                       stage, capture.used);
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        char rows[3][81];
        XtpColor foreground[3];
} SelfTestRowCapture;

static void
SelfTestRowCaptureFrame(const XtpRenderFrame *frame, void *closure)
{
        (void)frame;
        (void)closure;
}

static void
SelfTestRowCaptureCell(const XtpRenderCell *cell, void *closure)
{
        SelfTestRowCapture *capture = closure;

        if (cell->row >= 3U || cell->column >= 80U)
                return;
        capture->rows[cell->row][cell->column] = cell->utf8_length == 0   ? ' '
                                                 : cell->utf8_length == 1 ? cell->utf8[0]
                                                                          : '?';
        if (cell->row == 0U && cell->column < 3U)
                capture->foreground[cell->column] = cell->foreground;
}

static int
SelfTestRowCaptureRender(XtpTerminal *terminal, SelfTestRowCapture *capture)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestRowCaptureFrame,
            .cell = SelfTestRowCaptureCell,
            .end = SelfTestRowCaptureFrame,
            .abort = NULL,
        };
        size_t row;

        memset(capture, 0, sizeof(*capture));
        for (row = 0; row < 3U; ++row)
                memset(capture->rows[row], ' ', 80);
        return XtpTerminalRender(terminal, &renderer, capture, true);
}

static bool
SelfTestRowIs(const SelfTestRowCapture *capture, size_t row, const char *prefix)
{
        size_t length = strlen(prefix);
        size_t column;

        if (memcmp(capture->rows[row], prefix, length) != 0)
                return false;
        for (column = length; column < 80U; ++column) {
                if (capture->rows[row][column] != ' ')
                        return false;
        }
        return true;
}

/* Each claimed DA1 code is proven through rendered cells, not parser acceptance. */
static int
SelfTestDeviceAttributesEvidence(void)
{
        XtpTerminal *terminal;
        SelfTestRowCapture capture;
        SelfTestPtyCapture pty = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &pty,
        };
        const char *stage = "selective erase keeps DECSCA cells";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        SelfTestFeedText(terminal, "\033[H\033[1\"qAB\033[0\"qCD\033[?2J");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 || !SelfTestRowIs(&capture, 0, "AB"))
                goto done;
        SelfTestFeedText(terminal, "\033[2;1H\033[1\"qPQ\033[0\"qRS\033[?2K");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 || !SelfTestRowIs(&capture, 1, "PQ"))
                goto done;
        stage = "plain erase ignores DEC protection";
        SelfTestFeedText(terminal, "\033[2J");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 || !SelfTestRowIs(&capture, 0, "") ||
            !SelfTestRowIs(&capture, 1, ""))
                goto done;
        stage = "left/right margins bound line insertion";
        SelfTestFeedText(
            terminal, "\033[H0123456789ABCDEFGHIJKLMNOPQRST\033[?69h\033[10;20s\033[1;12H\033[L");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 ||
            !SelfTestRowIs(&capture, 0, "012345678           KLMNOPQRST") ||
            !SelfTestRowIs(&capture, 1, "         9ABCDEFGHIJ"))
                goto done;
        stage = "DECRQM reports mode 69";
        pty = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033[?69$p");
        if (!SelfTestPtyEqualsText(&pty, "\033[?69;1$y"))
                goto done;
        stage = "resetting mode 69 restores full-width insertion";
        SelfTestFeedText(terminal, "\033[?69l\033[H\033[L");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 || !SelfTestRowIs(&capture, 0, "") ||
            !SelfTestRowIs(&capture, 1, "012345678           KLMNOPQRST"))
                goto done;
        stage = "palette and RGB foregrounds reach cells";
        SelfTestFeedText(terminal, "\033[2J\033[H\033[31mR\033[38;5;200mG\033[38;2;1;2;3mB\033[0m");
        if (SelfTestRowCaptureRender(terminal, &capture) != 0 ||
            !SelfTestRowIs(&capture, 0, "RGB") || capture.foreground[0].kind != XTP_COLOR_PALETTE ||
            capture.foreground[0].palette != 1 || capture.foreground[1].kind != XTP_COLOR_PALETTE ||
            capture.foreground[1].palette != 200 || capture.foreground[2].kind != XTP_COLOR_RGB ||
            capture.foreground[2].red != 1 || capture.foreground[2].green != 2 ||
            capture.foreground[2].blue != 3)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "device attribute evidence mismatch stage=%s row0=\"%.32s\" row1=\"%.32s\"",
                       stage, capture.rows[0], capture.rows[1]);
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        unsigned int calls;
        size_t last_length;
        bool last_truncated;
        uint8_t last[XTP_UNKNOWN_APC_CAPTURE_LIMIT + 8U];
        SelfTestPtyCapture pty;
} SelfTestUnknownApcHarness;

static void
SelfTestUnknownApcPty(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestUnknownApcHarness *harness = closure;

        SelfTestCapturePty(bytes, length, &harness->pty);
}

static void
SelfTestUnknownApcEffect(const uint8_t *bytes, size_t length, bool truncated, void *closure)
{
        SelfTestUnknownApcHarness *harness = closure;

        ++harness->calls;
        harness->last_length = length;
        harness->last_truncated = truncated;
        memset(harness->last, 0, sizeof(harness->last));
        if (length < sizeof(harness->last))
                memcpy(harness->last, bytes, length);
}

static bool
SelfTestUnknownApcIs(const SelfTestUnknownApcHarness *harness, unsigned int calls,
                     const char *payload, bool truncated)
{
        return harness->calls == calls && harness->last_length == strlen(payload) &&
               memcmp(harness->last, payload, strlen(payload)) == 0 &&
               harness->last_truncated == truncated;
}

static int
SelfTestUnknownApc(void)
{
        XtpTerminal *terminal;
        SelfTestUnknownApcHarness harness = {0};
        SelfTestRowCapture rows;
        XtpTerminalEffects effects = {
            .write_pty = SelfTestUnknownApcPty,
            .unknown_apc = SelfTestUnknownApcEffect,
            .closure = &harness,
        };
        const char *stage = "setup";
        char *big = NULL;
        size_t index;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "a whole unknown APC is delivered once";
        SelfTestFeedText(terminal, "\033_unknown-probe-dispatch\033\\");
        if (!SelfTestUnknownApcIs(&harness, 1, "unknown-probe-dispatch", false))
                goto done;
        stage = "the 8-bit ST also terminates it";
        SelfTestFeedText(terminal, "\033_abc;xy\xc2\x9c");
        if (!SelfTestUnknownApcIs(&harness, 2, "abc;xy", false))
                goto done;
        stage = "byte-by-byte feeding delivers the same payload";
        {
                static const char split[] = "\033_frag;ment\033\\";

                for (index = 0; split[index] != '\0'; ++index)
                        XtpTerminalFeed(terminal, (const uint8_t *)split + index, 1);
        }
        if (!SelfTestUnknownApcIs(&harness, 3, "frag;ment", false))
                goto done;
        stage = "ESC ends the APC before the terminator's backslash arrives";
        SelfTestFeedText(terminal, "\033_tail\033");
        if (!SelfTestUnknownApcIs(&harness, 4, "tail", false))
                goto done;
        SelfTestFeedText(terminal, "\\");
        if (harness.calls != 4 || harness.pty.used != 0)
                goto done;
        stage = "surrounding text and controls survive";
        SelfTestFeedText(terminal, "\033[H\033[2Jab\033_x;1\033\\cd\033[6n");
        if (!SelfTestUnknownApcIs(&harness, 5, "x;1", false) ||
            SelfTestRowCaptureRender(terminal, &rows) != 0 || !SelfTestRowIs(&rows, 0, "abcd") ||
            !SelfTestPtyEqualsText(&harness.pty, "\033[1;5R"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "embedded non-aborting C0 bytes, including NUL, are payload";
        {
                static const uint8_t c0[] = "\033_a\a\r\n\x01\0z\033\\";

                XtpTerminalFeed(terminal, c0, sizeof(c0) - 1U);
        }
        if (harness.calls != 6 || harness.last_length != 7 ||
            memcmp(harness.last, "a\a\r\n\x01\0z", 7) != 0 || harness.last_truncated)
                goto done;
        stage = "ESC followed by another control ends the APC and keeps the control";
        SelfTestFeedText(terminal, "\033_ended\033[6n");
        if (!SelfTestUnknownApcIs(&harness, 7, "ended", false) ||
            !SelfTestPtyEqualsText(&harness.pty, "\033[1;5R"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "CAN aborts without a report";
        SelfTestFeedText(terminal, "\033_dropped\030\033[6n");
        if (harness.calls != 7 || !SelfTestPtyEqualsText(&harness.pty, "\033[1;5R"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "SUB aborts without a report";
        SelfTestFeedText(terminal, "\033_dropped\032\033[6n");
        if (harness.calls != 7 || !SelfTestPtyEqualsText(&harness.pty, "\033[1;5R"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "empty and identifier-prefix payloads are dropped by the core";
        SelfTestFeedText(terminal, "\033_\033\\\033_25a\033\\");
        if (harness.calls != 7)
                goto done;
        stage = "an oversized payload is cut at the capture limit";
        big = malloc(XTP_UNKNOWN_APC_CAPTURE_LIMIT * 4U + 4U);
        if (big == NULL)
                goto done;
        big[0] = '\033';
        big[1] = '_';
        for (index = 0; index < XTP_UNKNOWN_APC_CAPTURE_LIMIT * 4U; ++index)
                big[2 + index] = (char)('a' + index % 26);
        big[2 + index] = '\033';
        big[3 + index] = '\\';
        XtpTerminalFeed(terminal, (const uint8_t *)big, XTP_UNKNOWN_APC_CAPTURE_LIMIT * 4U + 4U);
        if (harness.calls != 8 || harness.last_length != XTP_UNKNOWN_APC_CAPTURE_LIMIT ||
            !harness.last_truncated || memcmp(harness.last, big + 2, harness.last_length) != 0)
                goto done;
        stage = "a limit-sized payload is complete";
        XtpTerminalFeed(terminal, (const uint8_t *)big, XTP_UNKNOWN_APC_CAPTURE_LIMIT + 2U);
        SelfTestFeedText(terminal, "\033\\");
        if (harness.calls != 9 || harness.last_length != XTP_UNKNOWN_APC_CAPTURE_LIMIT ||
            harness.last_truncated)
                goto done;
        stage = "Kitty graphics keeps its own path and reply";
        SelfTestFeedText(terminal, "\033_Ga=q,i=31,f=24,s=1,v=1;AAAA\033\\");
        if (harness.calls != 9 || !SelfTestPtyEqualsText(&harness.pty, "\033_Gi=31;OK\033\\"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "the glyph protocol identifier is not reported";
        SelfTestFeedText(terminal, "\033_25a1;anything\033\\");
        if (harness.calls != 9)
                goto done;
        stage = "the parser is intact afterwards";
        SelfTestFeedText(terminal, "\033_after\033\\\033[c");
        if (!SelfTestUnknownApcIs(&harness, 10, "after", false) ||
            !SelfTestPtyEqualsText(&harness.pty, XTP_DA1_REPLY))
                goto done;
        stage = "without an effect the backend stays quiet";
        effects.unknown_apc = NULL;
        XtpTerminalSetEffects(terminal, &effects);
        harness.pty = (SelfTestPtyCapture){0};
        SelfTestFeedText(terminal, "\033_ignored\033\\\033[5n");
        if (harness.calls != 10 || !SelfTestPtyEqualsText(&harness.pty, "\033[0n"))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "unknown APC mismatch stage=%s calls=%u length=%zu pty=%zu", stage,
                       harness.calls, harness.last_length, harness.pty.used);
        free(big);
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        unsigned int calls;
        char title[64];
        char body[64];
        size_t title_length;
        size_t body_length;
        SelfTestPtyCapture pty;
} SelfTestNotificationHarness;

static void
SelfTestNotificationPty(const uint8_t *bytes, size_t length, void *closure)
{
        SelfTestNotificationHarness *harness = closure;

        SelfTestCapturePty(bytes, length, &harness->pty);
}

static void
SelfTestNotificationEffect(const uint8_t *title, size_t title_length, const uint8_t *body,
                           size_t body_length, void *closure)
{
        SelfTestNotificationHarness *harness = closure;

        ++harness->calls;
        harness->title_length = title_length;
        harness->body_length = body_length;
        memset(harness->title, 0, sizeof(harness->title));
        memset(harness->body, 0, sizeof(harness->body));
        if (title_length < sizeof(harness->title))
                memcpy(harness->title, title, title_length);
        if (body_length < sizeof(harness->body))
                memcpy(harness->body, body, body_length);
}

static bool
SelfTestNotificationIs(const SelfTestNotificationHarness *harness, unsigned int calls,
                       const char *title, const char *body)
{
        return harness->calls == calls && harness->title_length == strlen(title) &&
               strcmp(harness->title, title) == 0 && harness->body_length == strlen(body) &&
               strcmp(harness->body, body) == 0;
}

static int
SelfTestNotificationEffectDelivery(void)
{
        XtpTerminal *terminal;
        SelfTestNotificationHarness harness = {0};
        SelfTestRowCapture rows;
        XtpTerminalEffects effects = {
            .write_pty = SelfTestNotificationPty,
            .notification = SelfTestNotificationEffect,
            .closure = &harness,
        };
        const char *stage = "setup";
        size_t index;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        stage = "OSC 9 delivers the body with an empty title";
        SelfTestFeedText(terminal, "\033]9;hello\033\\");
        if (!SelfTestNotificationIs(&harness, 1, "", "hello"))
                goto done;
        stage = "OSC 777 delivers title and body with a BEL terminator";
        SelfTestFeedText(terminal, "\033]777;notify;Title;Body\a");
        if (!SelfTestNotificationIs(&harness, 2, "Title", "Body"))
                goto done;
        stage = "byte-by-byte feeding delivers the same notification";
        {
                static const char split[] = "\033]777;notify;Sp;lit\033\\";

                for (index = 0; split[index] != '\0'; ++index)
                        XtpTerminalFeed(terminal, (const uint8_t *)split + index, 1);
        }
        if (!SelfTestNotificationIs(&harness, 3, "Sp", "lit"))
                goto done;
        stage = "surrounding text and a CPR query survive";
        SelfTestFeedText(terminal, "\033[H\033[2Jab\033]9;mid\033\\cd\033[6n");
        if (!SelfTestNotificationIs(&harness, 4, "", "mid") ||
            SelfTestRowCaptureRender(terminal, &rows) != 0 || !SelfTestRowIs(&rows, 0, "abcd") ||
            !SelfTestPtyEqualsText(&harness.pty, "\033[1;5R"))
                goto done;
        harness.pty = (SelfTestPtyCapture){0};
        stage = "UTF-8 passes through unchanged";
        SelfTestFeedText(terminal, "\033]777;notify;caf\xc3\xa9;\xe2\x9c\x93 ok\033\\");
        if (!SelfTestNotificationIs(&harness, 5, "caf\xc3\xa9", "\xe2\x9c\x93 ok"))
                goto done;
        stage = "an empty body is still delivered";
        SelfTestFeedText(terminal, "\033]9;\033\\");
        if (!SelfTestNotificationIs(&harness, 6, "", ""))
                goto done;
        stage = "valid ConEmu OSC 9 forms and progress are not notifications";
        SelfTestFeedText(terminal, "\033]9;4;1;50\033\\\033]9;1;10\033\\\033]9;4;0\033\\");
        if (harness.calls != 6)
                goto done;
        /* The core falls incomplete ConEmu shapes back to iTerm2 text; they must not be suppressed.
         */
        stage = "incomplete ConEmu shapes are iTerm2 notifications";
        SelfTestFeedText(terminal, "\033]9;4\033\\");
        if (!SelfTestNotificationIs(&harness, 7, "", "4"))
                goto done;
        SelfTestFeedText(terminal, "\033]9;4;\033\\\033]9;4;5\033\\");
        if (!SelfTestNotificationIs(&harness, 9, "", "4;5"))
                goto done;
        stage = "malformed OSC 777 is dropped";
        SelfTestFeedText(terminal, "\033]777;notify;NoBody\033\\\033]777;other;a;b\033\\");
        if (harness.calls != 9)
                goto done;
        stage = "nothing is written to the PTY";
        if (harness.pty.used != 0)
                goto done;
        stage = "without an effect the backend stays quiet";
        effects.notification = NULL;
        XtpTerminalSetEffects(terminal, &effects);
        SelfTestFeedText(terminal, "\033]9;ignored\033\\\033[5n");
        if (harness.calls != 9 || !SelfTestPtyEqualsText(&harness.pty, "\033[0n"))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "notification mismatch stage=%s calls=%u title=\"%s\" body=\"%s\" pty=%zu",
                       stage, harness.calls, harness.title, harness.body, harness.pty.used);
        XtpTerminalFree(terminal);
        return result;
}

static bool
SelfTestRowStateIs(XtpTerminal *terminal, uint64_t row, XtpSemanticRow expected)
{
        XtpSemanticRow state;

        return XtpTerminalSemanticRow(terminal, row, &state) == 0 && state == expected;
}

static bool
SelfTestPromptIs(XtpTerminal *terminal, uint64_t from, bool forward, int64_t expected)
{
        uint64_t row;
        int result = XtpTerminalFindPrompt(terminal, from, forward, &row);

        if (expected < 0)
                return result != 0;
        return result == 0 && row == (uint64_t)expected;
}

/* Checks the core's output span for a prompt and the plain text it formats to. */
static bool
SelfTestOutputIs(XtpTerminal *terminal, uint64_t prompt, uint64_t start_row, uint16_t start_column,
                 uint64_t end_row, uint16_t end_column, const char *expected)
{
        XtpSemanticSpan span;
        char *text = NULL;
        size_t length = 0;
        bool matches;

        if (XtpTerminalCommandOutput(terminal, prompt, &span) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "no output span for prompt row %llu",
                       (unsigned long long)prompt);
                return false;
        }
        if (span.start_row != start_row || span.start_column != start_column ||
            span.end_row != end_row || span.end_column != end_column) {
                XtpLog(XTP_LOG_ERROR, "self-test", "output span for row %llu is %llu,%u-%llu,%u",
                       (unsigned long long)prompt, (unsigned long long)span.start_row,
                       span.start_column, (unsigned long long)span.end_row, span.end_column);
                return false;
        }
        if (XtpTerminalSpanText(terminal, &span, &text, &length) != 0)
                return false;
        matches = length == strlen(expected) && strcmp(text, expected) == 0;
        if (!matches)
                XtpLog(XTP_LOG_ERROR, "self-test", "output text for row %llu is \"%s\" (%zu bytes)",
                       (unsigned long long)prompt, text, length);
        free(text);
        return matches;
}

static bool
SelfTestNoOutput(XtpTerminal *terminal, uint64_t prompt)
{
        XtpSemanticSpan span;

        return XtpTerminalCommandOutput(terminal, prompt, &span) != 0;
}

/* Walks backward from the bottom of the screen counting prompt starts. */
static unsigned int
SelfTestCountPrompts(XtpTerminal *terminal)
{
        XtpTerminalScrollbar state;
        uint64_t from;
        uint64_t row;
        unsigned int count = 0;

        if (XtpTerminalGetScrollbar(terminal, &state) != 0)
                return 0;
        from = state.total;
        while (XtpTerminalFindPrompt(terminal, from, false, &row) == 0) {
                if (SelfTestRowStateIs(terminal, row, XTP_SEMANTIC_ROW_NONE))
                        return 0;
                ++count;
                from = row;
        }
        return count;
}

/*
 * Screen rows after the fixture on a 20x6 grid: 0 p1 prompt, 1-2 output, 3 p2 prompt,
 * 4 its k=s continuation, 5 output, 6 wrapped prompt, 7 its wrap tail (the core marks it a
 * continuation), 8 output,
 * 9 an orphan k=s continuation, 10 output, 11 the live prompt; rows 6-11 are active.
 */
static int
SelfTestPromptNavigation(void)
{
        static const char fixture[] =
            "\033]133;A\033\\p1$ \033]133;B\033\\cmd1\r\n"
            "\033]133;C\033\\out1a\r\nout1b\r\n\033]133;D;0\033\\"
            "\033]133;A\033\\p2$ \033]133;B\033\\cmd2 \\\r\n"
            "\033]133;A;k=s\033\\> \033]133;B\033\\more\r\n"
            "\033]133;C\033\\out2\r\n\033]133;D;0\033\\"
            "\033]133;A\033\\pppppppppppppppppppppppp$ \033]133;B\033\\cmd3\r\n"
            "\033]133;C\033\\out3\r\n\033]133;D;0\033\\"
            "\033]133;A;k=s\033\\> \033]133;B\033\\orphan\r\n"
            "\033]133;C\033\\out5\r\n\033]133;D;0\033\\"
            "\033]133;A\033\\p4$ \033]133;B\033\\";
        XtpTerminal *terminal;
        XtpTerminal *plain = NULL;
        XtpTerminal *layout = NULL;
        XtpTerminalScrollbar state = {0};
        XtpSemanticRow live_kind;
        uint64_t found;
        const char *stage = "setup";
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
        plain = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
        layout = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
        if (terminal == NULL || plain == NULL || layout == NULL ||
            XtpTerminalSetScrollbackLines(terminal, 64) != 0 ||
            XtpTerminalSetScrollbackLines(plain, 64) != 0)
                goto done;
        stage = "thousands of prompts stay indexed and reachable";
        {
                XtpTerminal *deep = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
                unsigned int count = 0;
                uint64_t walk;
                uint64_t found_row;
                int line;

                if (deep == NULL || XtpTerminalSetScrollbackLines(deep, 20000) != 0)
                        goto deep_failed;
                for (line = 0; line < 6000; ++line)
                        SelfTestFeedText(
                            deep, "\033]133;A\033\\$ \033]133;B\033\\c\r\n\033]133;C\033\\o\r\n");
                if (XtpTerminalGetScrollbar(deep, &state) != 0)
                        goto deep_failed;
                walk = state.total;
                while (XtpTerminalFindPrompt(deep, walk, false, &found_row) == 0) {
                        ++count;
                        walk = found_row;
                }
                if (count != 6000 || walk != 0 || !SelfTestPromptIs(deep, 0, true, 2) ||
                    !SelfTestPromptIs(deep, 11998, true, -1))
                        goto deep_failed;
                XtpTerminalFree(deep);
                goto deep_done;
        deep_failed:
                XtpTerminalFree(deep);
                goto done;
        deep_done:;
        }
        stage = "pruned prompts leave the index bounded by retained rows";
        {
                XtpTerminal *small = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
                int line;

                if (small == NULL || XtpTerminalSetScrollbackLines(small, 40) != 0) {
                        XtpTerminalFree(small);
                        goto done;
                }
                /* No search runs between writes and the count accessor never compacts, so
                 * only insert-time compaction can bound this; the fixture ends on a mark so the
                 * last operation is an insert rather than output that might prune a page. */
                for (line = 0; line < 3000; ++line)
                        SelfTestFeedText(
                            small, "\033]133;A\033\\$ \033]133;B\033\\c\r\n\033]133;C\033\\o\r\n");
                SelfTestFeedText(small, "\033]133;A\033\\");
                if (XtpTerminalGetScrollbar(small, &state) != 0 || state.total >= 6000 ||
                    XtpTerminalPromptMarks(small) > state.total ||
                    XtpTerminalPromptMarks(small) == 0 ||
                    !SelfTestPromptIs(small, state.total, false, state.total - 1U)) {
                        XtpLog(XTP_LOG_ERROR, "self-test", "prompt marks=%zu retained rows=%llu",
                               XtpTerminalPromptMarks(small), (unsigned long long)state.total);
                        XtpTerminalFree(small);
                        goto done;
                }
                XtpTerminalFree(small);
        }
        stage = "OSC 133 D names the completed command before any next prompt";
        {
                XtpTerminal *done = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
                uint64_t completed = 99;

                stage = "D: nothing seen yet";
                if (done == NULL || XtpTerminalSetScrollbackLines(done, 64) != 0 ||
                    XtpTerminalLastCompletedPrompt(done, &completed) == 0 ||
                    XtpTerminalCommandEndSeen(done)) {
                        XtpTerminalFree(done);
                        goto done;
                }
                SelfTestFeedText(done,
                                 "\033]133;A\033\\$ \033]133;B\033\\one\r\n\033]133;C\033\\o1\r\n");
                stage = "D: output without D";
                if (XtpTerminalLastCompletedPrompt(done, &completed) == 0) {
                        XtpTerminalFree(done);
                        goto done;
                }
                SelfTestFeedText(done, "\033]133;D;0\033\\");
                stage = "D: first D";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 0) {
                        XtpTerminalFree(done);
                        goto done;
                }
                /* A later prompt with a running command leaves the answer at the finished one. */
                SelfTestFeedText(done,
                                 "\033]133;A\033\\$ \033]133;B\033\\two\r\n\033]133;C\033\\o2\r\n");
                stage = "D: later running command";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 0) {
                        XtpTerminalFree(done);
                        goto done;
                }
                SelfTestFeedText(done, "\033]133;D;1\033\\");
                stage = "D: second D";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 2) {
                        XtpTerminalFree(done);
                        goto done;
                }
                /* Rejected forms leave the answer at the last accepted D: aborts, a first item
                 * other than a bare D, and a report past the core's capture limit. */
                SelfTestFeedText(
                    done, "\033]133;A\033\\$ \033]133;B\033\\three\r\n\033]133;C\033\\o3\r\n");
                SelfTestFeedText(done, "\033]133;D\030\033]133;D;0\032\033]133;Dgarbage\033\\");
                stage = "D: aborted and malformed forms";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 2) {
                        XtpTerminalFree(done);
                        goto done;
                }
                {
                        char *huge = malloc(XTP_UNKNOWN_APC_CAPTURE_LIMIT * 12U + 16U);
                        size_t index;

                        if (huge == NULL) {
                                XtpTerminalFree(done);
                                goto done;
                        }
                        memcpy(huge, "\033]133;D;", 9);
                        for (index = 0; index < XTP_UNKNOWN_APC_CAPTURE_LIMIT * 12U; ++index)
                                huge[9 + index] = 'x';
                        memcpy(huge + 9 + index, "\033\\", 2);
                        XtpTerminalFeed(done, (const uint8_t *)huge,
                                        XTP_UNKNOWN_APC_CAPTURE_LIMIT * 12U + 11U);
                        free(huge);
                }
                stage = "D: oversized report";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 2) {
                        XtpTerminalFree(done);
                        goto done;
                }
                /* The capture limit counts the payload after "133;": exactly 2048 bytes is
                 * accepted, one more is dropped by the core. Options after the exit code are
                 * ignored by the core, so padding is a valid report. */
                {
                        static const char lead[] = "\033]133;D;0;aid=";
                        /* The payload after "133;" is the lead minus its 6-byte introducer and NUL.
                         */
                        size_t pad = 2048U - (sizeof(lead) - 7U);
                        char *report = malloc(sizeof(lead) + pad + 4U);
                        size_t index;

                        if (report == NULL) {
                                XtpTerminalFree(done);
                                goto done;
                        }
                        memcpy(report, lead, sizeof(lead) - 1U);
                        for (index = 0; index < pad + 1U; ++index)
                                report[sizeof(lead) - 1U + index] = 'p';
                        memcpy(report + sizeof(lead) - 1U + pad + 1U, "\033\\", 2);
                        /* 2049 payload bytes: rejected. */
                        XtpTerminalFeed(done, (const uint8_t *)report,
                                        sizeof(lead) - 1U + pad + 3U);
                        stage = "D: 2049-byte payload rejected";
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 2) {
                                free(report);
                                XtpTerminalFree(done);
                                goto done;
                        }
                        /* 2048 payload bytes: accepted, and it names prompt three. */
                        memcpy(report + sizeof(lead) - 1U + pad, "\033\\", 2);
                        XtpTerminalFeed(done, (const uint8_t *)report,
                                        sizeof(lead) - 1U + pad + 2U);
                        free(report);
                        stage = "D: 2048-byte payload accepted";
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 4) {
                                XtpTerminalFree(done);
                                goto done;
                        }
                }
                /* Nonterminating C0 bytes are dropped by the core, so they neither lengthen the
                 * D item nor count toward the capture, whole or split across feeds. */
                stage = "D: ignored C0 bytes inside the item";
                {
                        static const uint8_t nul_whole[] = "\033]133;D\0;0\033\\";
                        static const uint8_t nul_head[] = "\033]133;D";
                        static const uint8_t nul_mid[] = "\0\0";
                        static const uint8_t nul_tail[] = ";0\033\\";
                        static const char lead[] = "\033]133;D;0;aid=";
                        size_t pad = 2048U - (sizeof(lead) - 7U);
                        char *report = malloc(sizeof(lead) + pad * 2U + 4U);
                        size_t index;
                        size_t used;

                        SelfTestFeedText(
                            done,
                            "\033]133;A\033\\$ \033]133;B\033\\five\r\n\033]133;C\033\\o5\r\n");
                        XtpTerminalFeed(done, nul_whole, sizeof(nul_whole) - 1U);
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 6) {
                                free(report);
                                XtpTerminalFree(done);
                                goto done;
                        }
                        SelfTestFeedText(
                            done,
                            "\033]133;A\033\\$ \033]133;B\033\\six\r\n\033]133;C\033\\o6\r\n");
                        XtpTerminalFeed(done, nul_head, sizeof(nul_head) - 1U);
                        XtpTerminalFeed(done, nul_mid, sizeof(nul_mid) - 1U);
                        XtpTerminalFeed(done, nul_tail, sizeof(nul_tail) - 1U);
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 8) {
                                free(report);
                                XtpTerminalFree(done);
                                goto done;
                        }
                        /* 2048 printable bytes interleaved with NULs are still accepted; one
                         * more printable byte is not. */
                        stage = "D: ignored C0 bytes near the capture boundary";
                        if (report == NULL) {
                                XtpTerminalFree(done);
                                goto done;
                        }
                        SelfTestFeedText(
                            done,
                            "\033]133;A\033\\$ \033]133;B\033\\seven\r\n\033]133;C\033\\o7\r\n");
                        memcpy(report, lead, sizeof(lead) - 1U);
                        used = sizeof(lead) - 1U;
                        for (index = 0; index < pad; ++index) {
                                report[used++] = 'p';
                                report[used++] = '\0';
                        }
                        report[used++] = 'p';
                        memcpy(report + used, "\033\\", 2);
                        XtpTerminalFeed(done, (const uint8_t *)report, used + 2U);
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 8) {
                                free(report);
                                XtpTerminalFree(done);
                                goto done;
                        }
                        --used;
                        memcpy(report + used, "\033\\", 2);
                        XtpTerminalFeed(done, (const uint8_t *)report, used + 2U);
                        free(report);
                        if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 ||
                            completed != 10) {
                                XtpTerminalFree(done);
                                goto done;
                        }
                }
                /* A D split across feeds, including inside its terminator, still counts. */
                SelfTestFeedText(
                    done, "\033]133;A\033\\$ \033]133;B\033\\eight\r\n\033]133;C\033\\o8\r\n");
                SelfTestFeedText(done, "\033]133");
                SelfTestFeedText(done, ";D");
                SelfTestFeedText(done, ";0\033");
                SelfTestFeedText(done, "\\");
                stage = "D: split across feeds";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 12) {
                        XtpTerminalFree(done);
                        goto done;
                }
                SelfTestFeedText(
                    done,
                    "\033]133;A\033\\$ \033]133;B\033\\four\r\n\033]133;C\033\\o4\r\n\033]133;D\a");
                stage = "D: BEL terminator";
                if (XtpTerminalLastCompletedPrompt(done, &completed) != 0 || completed != 14 ||
                    !XtpTerminalCommandEndSeen(done)) {
                        XtpTerminalFree(done);
                        goto done;
                }
                SelfTestFeedText(done, "\033[?1049h");
                stage = "D: alternate screen";
                if (XtpTerminalLastCompletedPrompt(done, &completed) == 0 ||
                    !XtpTerminalCommandEndSeen(done)) {
                        XtpTerminalFree(done);
                        goto done;
                }
                XtpTerminalFree(done);
                /* A D accepted while the alternate screen is active cannot retain a prompt but
                 * still proves the shell reports completion. */
                done = XtpTerminalNewWithGraphemeWidth(20, 6, 8, 16, false);
                stage = "D: accepted on the alternate screen";
                if (done == NULL) {
                        goto done;
                }
                SelfTestFeedText(done, "\033[?1049h\033]133;D;0\033\\");
                if (!XtpTerminalCommandEndSeen(done) ||
                    XtpTerminalLastCompletedPrompt(done, &completed) == 0) {
                        XtpTerminalFree(done);
                        goto done;
                }
                XtpTerminalFree(done);
        }
        stage = "no markers means no prompts";
        SelfTestFeedText(plain, "a\r\nb\r\nc\r\nd\r\ne\r\nf\r\ng\r\nh\r\n");
        if (XtpTerminalGetScrollbar(plain, &state) != 0 || state.total != 9 ||
            !SelfTestPromptIs(plain, state.offset, false, -1) ||
            !SelfTestPromptIs(plain, 0, true, -1) ||
            !SelfTestRowStateIs(plain, 0, XTP_SEMANTIC_ROW_NONE) ||
            XtpTerminalSemanticRow(plain, 9, NULL) == 0)
                goto done;
        stage = "row states follow OSC 133";
        SelfTestFeedText(terminal, fixture);
        if (XtpTerminalGetScrollbar(terminal, &state) != 0 || state.total != 12 ||
            state.offset != 6 || !SelfTestRowStateIs(terminal, 0, XTP_SEMANTIC_ROW_PROMPT) ||
            !SelfTestRowStateIs(terminal, 1, XTP_SEMANTIC_ROW_NONE) ||
            !SelfTestRowStateIs(terminal, 3, XTP_SEMANTIC_ROW_PROMPT) ||
            !SelfTestRowStateIs(terminal, 4, XTP_SEMANTIC_ROW_PROMPT_CONTINUATION) ||
            !SelfTestRowStateIs(terminal, 6, XTP_SEMANTIC_ROW_PROMPT) ||
            !SelfTestRowStateIs(terminal, 7, XTP_SEMANTIC_ROW_PROMPT_CONTINUATION) ||
            !SelfTestRowStateIs(terminal, 9, XTP_SEMANTIC_ROW_PROMPT_CONTINUATION) ||
            !SelfTestRowStateIs(terminal, 11, XTP_SEMANTIC_ROW_PROMPT) ||
            XtpTerminalSemanticRow(terminal, 12, NULL) == 0)
                goto done;
        stage = "previous prompt from the active viewport";
        if (!SelfTestPromptIs(terminal, 6, false, 3) || !SelfTestPromptIs(terminal, 3, false, 0) ||
            !SelfTestPromptIs(terminal, 0, false, -1))
                goto done;
        stage = "a continuation resolves to its primary row";
        if (!SelfTestPromptIs(terminal, 5, false, 3) || !SelfTestPromptIs(terminal, 4, false, 3))
                goto done;
        stage = "an orphan continuation is its own prompt";
        if (!SelfTestPromptIs(terminal, 11, false, 9) || !SelfTestPromptIs(terminal, 10, false, 9))
                goto done;
        stage = "next prompt skips the current prompt's continuations";
        if (!SelfTestPromptIs(terminal, 0, true, 3) || !SelfTestPromptIs(terminal, 3, true, 6) ||
            !SelfTestPromptIs(terminal, 4, true, 6) || !SelfTestPromptIs(terminal, 6, true, 9) ||
            !SelfTestPromptIs(terminal, 9, true, 11) || !SelfTestPromptIs(terminal, 11, true, -1))
                goto done;
        stage = "command output spans are cell-exact and formatted by the core";
        if (!SelfTestOutputIs(terminal, 0, 1, 0, 2, 4, "out1a\nout1b") ||
            !SelfTestOutputIs(terminal, 3, 5, 0, 5, 3, "out2") ||
            !SelfTestOutputIs(terminal, 6, 8, 0, 8, 3, "out3") ||
            !SelfTestOutputIs(terminal, 9, 10, 0, 10, 3, "out5") ||
            !SelfTestNoOutput(terminal, 11) || !SelfTestNoOutput(terminal, 1))
                goto done;
        /* Output may start on the prompt row right after the input; a prompt that begins
         * mid-row is moved to a fresh line by the core, so the earlier output keeps its row.
         * Both follow libghostty's own selectOutput rather than row arithmetic. */
        stage = "same-row layouts follow the core";
        SelfTestFeedText(layout, "\033]133;A\033\\p$ \033]133;B\033\\cmd\033]133;C\033\\tail\r\n"
                                 "more\r\nout\033]133;A\033\\q$ \033]133;B\033\\");
        if (!SelfTestPromptIs(layout, 0, true, 3) || !SelfTestPromptIs(layout, 3, false, 0) ||
            !SelfTestOutputIs(layout, 0, 0, 6, 2, 2, "tail\nmore\nout") ||
            !SelfTestNoOutput(layout, 3))
                goto done;
        /* Written spaces are output cells: leading blanks stay, and blank-only output is a
         * span whose formatted text is empty rather than "no output". */
        stage = "blank-leading and blank-only output follow the core";
        SelfTestFeedText(layout,
                         "\r\n\033]133;A\033\\r$ \033]133;B\033\\x\r\n\033]133;C\033\\  lead\r\n"
                         "\033]133;D;0\033\\\033]133;A\033\\s$ \033]133;B\033\\y\r\n"
                         "\033]133;C\033\\   \r\n\033]133;D;0\033\\"
                         "\033]133;A\033\\t$ \033]133;B\033\\");
        if (!SelfTestPromptIs(layout, 3, true, 4) || !SelfTestPromptIs(layout, 4, true, 6) ||
            !SelfTestPromptIs(layout, 6, true, 8) ||
            !SelfTestOutputIs(layout, 4, 5, 0, 5, 5, "  lead") ||
            !SelfTestOutputIs(layout, 6, 7, 0, 7, 2, "") || !SelfTestNoOutput(layout, 8))
                goto done;
        stage = "scrolling to a prompt row lands there, or on the active area";
        if (XtpTerminalScrollTo(terminal, 3) != 0 ||
            XtpTerminalGetScrollbar(terminal, &state) != 0 || state.offset != 3 ||
            XtpTerminalScrollTo(terminal, 9) != 0 ||
            XtpTerminalGetScrollbar(terminal, &state) != 0 || state.offset != 6)
                goto done;
        stage = "the alternate screen has no history and the API declines";
        SelfTestFeedText(terminal, "\033[?1049h\033[Halt\r\n");
        if (XtpTerminalGetScrollbar(terminal, &state) != 0 || state.total != state.length ||
            state.total != 6 || XtpTerminalSemanticRow(terminal, 0, &live_kind) == 0 ||
            XtpTerminalFindPrompt(terminal, 6, false, &found) == 0 ||
            XtpTerminalFindPrompt(terminal, 0, true, &found) == 0 || !SelfTestNoOutput(terminal, 0))
                goto done;
        SelfTestFeedText(terminal, "\033[?1049l");
        if (SelfTestCountPrompts(terminal) != 5)
                goto done;
        /* Reflow rejoins the wrapped prompt into one row that the core leaves marked as a
         * continuation; the orphan rule still makes it a prompt start. */
        stage = "reflow keeps every prompt start";
        if (XtpTerminalResize(terminal, 40, 6, 8, 16) != 0 || SelfTestCountPrompts(terminal) != 5 ||
            XtpTerminalGetScrollbar(terminal, &state) != 0 || state.total != 11 ||
            !SelfTestPromptIs(terminal, 11, false, 10) ||
            !SelfTestPromptIs(terminal, 10, false, 8) || !SelfTestPromptIs(terminal, 8, false, 6) ||
            !SelfTestPromptIs(terminal, 6, true, 8) ||
            !SelfTestOutputIs(terminal, 6, 7, 0, 7, 3, "out3"))
                goto done;
        /* A prompt redrawn on an earlier active row (screen row 8, active row 4) must not add a
         * second mark: the walk still counts five prompts and reaches the same rows. */
        stage = "redrawing an older prompt row adds no mark";
        SelfTestFeedText(terminal, "\033[4;1H\033]133;A\033\\\033[6;1H");
        if (SelfTestCountPrompts(terminal) != 5 || !SelfTestPromptIs(terminal, 10, false, 8) ||
            !SelfTestPromptIs(terminal, 8, false, 6))
                goto done;
        stage = "eviction drops early prompts without breaking the walk";
        if (XtpTerminalSetScrollbackLines(terminal, 0) != 0 ||
            XtpTerminalGetScrollbar(terminal, &state) != 0 || state.total != state.length ||
            SelfTestCountPrompts(terminal) > 3 || !SelfTestPromptIs(terminal, 0, false, -1))
                goto done;
        result = 0;
done:
        if (result != 0) {
                char states[16] = {0};
                size_t index;

                for (index = 0; index < 12; ++index) {
                        XtpSemanticRow kind;

                        states[index] = XtpTerminalSemanticRow(terminal, index, &kind) != 0 ? '?'
                                        : kind == XTP_SEMANTIC_ROW_PROMPT                   ? 'P'
                                        : kind == XTP_SEMANTIC_ROW_PROMPT_CONTINUATION      ? 'C'
                                                                                            : '.';
                }
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "prompt navigation mismatch stage=%s total=%llu offset=%llu rows=%s", stage,
                       (unsigned long long)state.total, (unsigned long long)state.offset, states);
        }
        XtpTerminalFree(layout);
        XtpTerminalFree(plain);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestStartupCursorShape(void)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        const char *stage = "precedence";
        int result = -1;

        if (XtpTerminalStartupCursorShape(false, false) != XTP_CURSOR_SHAPE_BLOCK ||
            XtpTerminalStartupCursorShape(true, false) != XTP_CURSOR_SHAPE_UNDERLINE ||
            XtpTerminalStartupCursorShape(false, true) != XTP_CURSOR_SHAPE_BAR ||
            XtpTerminalStartupCursorShape(true, true) != XTP_CURSOR_SHAPE_UNDERLINE)
                return -1;
        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        stage = "configured underline";
        if (XtpTerminalSetDefaultCursorShape(terminal, XTP_CURSOR_SHAPE_UNDERLINE) != 0 ||
            XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_UNDERLINE ||
            render.frame.cursor_blink_requested)
                goto done;
        stage = "application override";
        SelfTestFeedText(terminal, "\033[5 q");
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BAR ||
            !render.frame.cursor_blink_requested)
                goto done;
        stage = "DECSCUSR 0 restores the configured shape";
        SelfTestFeedText(terminal, "\033[0 q");
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_UNDERLINE)
                goto done;
        stage = "full reset restores the configured shape";
        SelfTestFeedText(terminal, "\033[6 q\033c");
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_UNDERLINE)
                goto done;
        stage = "changing the default shape keeps the application blink mode";
        {
                bool blinking = false;

                SelfTestFeedText(terminal, "\033[?12h");
                if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_CURSOR_BLINKING, &blinking) !=
                        0 ||
                    !blinking ||
                    XtpTerminalSetDefaultCursorShape(terminal, XTP_CURSOR_SHAPE_BAR) != 0 ||
                    XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_CURSOR_BLINKING, &blinking) !=
                        0 ||
                    !blinking)
                        goto done;
                SelfTestFeedText(terminal, "\033[?12l");
                if (XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_CURSOR_BLINKING, &blinking) !=
                        0 ||
                    blinking ||
                    XtpTerminalSetDefaultCursorShape(terminal, XTP_CURSOR_SHAPE_UNDERLINE) != 0 ||
                    XtpTerminalGetMode(terminal, XTP_TERMINAL_MODE_CURSOR_BLINKING, &blinking) !=
                        0 ||
                    blinking)
                        goto done;
        }
        stage = "xterm's CSI 7 SP q is not a libghostty style";
        SelfTestFeedText(terminal, "\033[6 q\033[7 q");
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BAR)
                goto done;
        stage = "configured bar replaces the default under an application shape";
        SelfTestFeedText(terminal, "\033[2 q");
        if (XtpTerminalSetDefaultCursorShape(terminal, XTP_CURSOR_SHAPE_BAR) != 0 ||
            XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BLOCK)
                goto done;
        SelfTestFeedText(terminal, "\033[ q");
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0 ||
            render.frame.cursor_shape != XTP_CURSOR_SHAPE_BAR)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "startup cursor shape mismatch stage=%s shape=%d", stage,
                       (int)render.frame.cursor_shape);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestUnderlineColor(void)
{
        static const XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
            .abort = NULL,
        };
        static const char text[] =
            "\033[4;58:2::255:0:0mA\033[59mB\033[58;5;1mC\033[0m\033[58:2:0:1:2:3mD";
        XtpTerminal *terminal;
        SelfTestRender render = {0};
        const XtpColor *colors = render.underline_colors;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(20, 4, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalFeed(terminal, (const uint8_t *)text, sizeof(text) - 1U);
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0)
                goto done;
        if (colors[0].kind != XTP_COLOR_RGB || colors[0].red != 255 || colors[0].green != 0 ||
            colors[0].blue != 0 || colors[1].kind != XTP_COLOR_DEFAULT ||
            colors[2].kind != XTP_COLOR_PALETTE || colors[2].palette != 1 ||
            colors[3].kind != XTP_COLOR_RGB || colors[3].red != 1 || colors[3].green != 2 ||
            colors[3].blue != 3)
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "underline color mismatch kinds=%d,%d,%d,%d",
                       (int)colors[0].kind, (int)colors[1].kind, (int)colors[2].kind,
                       (int)colors[3].kind);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestWindowOps(void)
{
        static const struct
        {
                const char *list;
                bool allow_all;
                bool get_allowed;
                bool set_allowed;
                unsigned int ignored;
        } cases[] = {
            {"GetIconTitle,GetWinTitle,GetChecksum,SetSelection,GetSelection,SetXprop", false,
             false, false, 2},
            {"GetIconTitle,GetWinTitle,GetChecksum,SetSelection,GetSelection,SetXprop", true, true,
             true, 2},
            {"GetIconTitle,GetWinTitle,GetSelection", false, false, true, 0},
            {"getselection setselection, ~GetSelection", false, true, false, 0},
            {"*", false, false, false, 0},
            {"Get*", false, false, true, 0},
            {"*Selection", false, false, false, 0},
            {"?etSelection", false, false, false, 0},
            {"*,~SetSelection", false, false, true, 0},
            {"*,~*Selection,GetSel*", false, false, true, 0},
            {"Get?election?", false, true, true, 1},
            {"20,21,22,23", false, true, true, 0},
            {"", false, true, true, 0},
            {NULL, false, true, true, 0},
        };
        size_t index;

        static const struct
        {
                const char *list;
                bool get_win_title;
                bool push_title;
        } title_cases[] = {
            {"GetIconTitle,GetWinTitle,GetChecksum,SetSelection,GetSelection,SetXprop", false,
             true},
            {"20,21,22,23", false, false},
            {"22", true, false},
            {"Get*Title,~21", true, true},
            {"PushTitle,PopTitle", true, false},
        };

        for (index = 0; index < XtNumber(title_cases); ++index) {
                XtpWindowOps ops;

                XtpWindowOpsParse(title_cases[index].list, &ops);
                if (XtpWindowOpAllowed(false, &ops, XTP_WINDOW_OP_GET_WIN_TITLE) !=
                        title_cases[index].get_win_title ||
                    XtpWindowOpAllowed(false, &ops, XTP_WINDOW_OP_PUSH_TITLE) !=
                        title_cases[index].push_title) {
                        XtpLog(XTP_LOG_ERROR, "self-test", "window-ops title mismatch list=%s",
                               title_cases[index].list);
                        return -1;
                }
        }
        for (index = 0; index < XtNumber(cases); ++index) {
                XtpWindowOps ops;

                XtpWindowOpsParse(cases[index].list, &ops);
                if (XtpWindowOpAllowed(cases[index].allow_all, &ops, XTP_WINDOW_OP_GET_SELECTION) !=
                        cases[index].get_allowed ||
                    XtpWindowOpAllowed(cases[index].allow_all, &ops, XTP_WINDOW_OP_SET_SELECTION) !=
                        cases[index].set_allowed ||
                    ops.ignored_entries != cases[index].ignored) {
                        XtpLog(XTP_LOG_ERROR, "self-test", "window-ops mismatch list=%s",
                               cases[index].list != NULL ? cases[index].list : "(null)");
                        return -1;
                }
        }
        return 0;
}

static int
SelfTestCursorBlinkReports(void)
{
        static const uint8_t coalesced_query_reset[] = "\033[?12$p\033[?12l";
        static const uint8_t coalesced_query_set[] = "\033[?12$p\033[?12h";
        static const uint8_t decrqss_before_style[] = "\033P$q q\033\\\033[4 q";
        static const uint8_t decrqss_blinking_block[] = "\033P1$r1 q\033\\";
        static const uint8_t query[] = "\033[?12$p";
        static const uint8_t report_set[] = "\033[?12;1$y";
        static const uint8_t report_reset[] = "\033[?12;2$y";
        static const uint8_t forced_style_query[] = "\033[4 q\033P$q q\033\\";
        static const uint8_t forced_style_report[] = "\033P1$r3 q\033\\";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalSetCursorBlinkDefault(terminal, true) != 0)
                goto done;
        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
        if (!SelfTestPtyEquals(&capture, report_reset, sizeof(report_reset) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, coalesced_query_set, sizeof(coalesced_query_set) - 1U);
        if (!SelfTestPtyEquals(&capture, report_reset, sizeof(report_reset) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, coalesced_query_reset, sizeof(coalesced_query_reset) - 1U);
        if (!SelfTestPtyEquals(&capture, report_set, sizeof(report_set) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q\033[?12$p", 12);
        if (!SelfTestPtyEquals(&capture, report_set, sizeof(report_set) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, decrqss_before_style, sizeof(decrqss_before_style) - 1U);
        if (!SelfTestPtyEquals(&capture, decrqss_blinking_block,
                               sizeof(decrqss_blinking_block) - 1U))
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[0 q", 5);
        capture = (SelfTestPtyCapture){0};
        if (XtpTerminalSetCursorBlinkRequestsEnabled(terminal, false) != 0)
                goto done;
        XtpTerminalFeed(terminal, forced_style_query, sizeof(forced_style_query) - 1U);
        if (!SelfTestPtyEquals(&capture, forced_style_report, sizeof(forced_style_report) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
        if (!SelfTestPtyEquals(&capture, report_set, sizeof(report_set) - 1U))
                goto done;
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, (const uint8_t *)"\033[!p", 4);
        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
        if (!SelfTestPtyEquals(&capture, report_reset, sizeof(report_reset) - 1U))
                goto done;
        result = 0;
done:
        if (result != 0)
                XtpLog(XTP_LOG_ERROR, "self-test", "cursor-blink mode report mismatch length=%zu",
                       capture.used);
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestDefaultColors(void)
{
        static const uint8_t query[] = "\033]10;?\033\\\033]11;?\033\\\033]12;?\033\\";
        static const uint8_t expected[] = "\033]10;rgb:1212/3434/5656\033\\"
                                          "\033]11;rgb:7878/9a9a/bcbc\033\\"
                                          "\033]12;rgb:dede/f0f0/1212\033\\";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        if (XtpTerminalSetDefaultColors(terminal, (XtpRgbColor){0x12, 0x34, 0x56},
                                        (XtpRgbColor){0x78, 0x9a, 0xbc},
                                        (XtpRgbColor){0xde, 0xf0, 0x12}) != 0)
                goto done;
        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
        if (capture.overflow || capture.used != sizeof(expected) - 1U ||
            memcmp(capture.bytes, expected, sizeof(expected) - 1U) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "default-color query mismatch length=%zu",
                       capture.used);
                goto done;
        }
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestColorOps(void)
{
        static const char *const denied[] = {
            "\033]10;#123456\033\\", "\033]11;#123456\a", "\033]12;#123456\033\\",
            "\033]110\033\\",        "\033]111\a",        "\033]112\033\\",
            "\033]110;\a",           "\033]110\030",      "\033]111\032",
            "\033]1\0010;#123456\a", "\033]11\0112\a",    "\033]10;?\033\\",
            "\033]11;?\a",           "\033]12;?\033\\",   "\033]10;#123456;#234567;#345678\a",
        };
        static const uint8_t baseline[] = "\033]10;#aabbcc\a\033]11;#aabbcc\a\033]12;#aabbcc\a";
        static const uint8_t query[] = "\033]10;?\033\\\033]11;?\033\\\033]12;?\033\\";
        static const uint8_t expected[] = "\033]10;rgb:aaaa/bbbb/cccc\033\\"
                                          "\033]11;rgb:aaaa/bbbb/cccc\033\\"
                                          "\033]12;rgb:aaaa/bbbb/cccc\033\\";
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {.write_pty = SelfTestCapturePty, .closure = &capture};
        XtpTerminal *terminal;
        size_t item, split;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        XtpTerminalFeed(terminal, baseline, sizeof(baseline) - 1U);
        for (item = 0; item < sizeof(denied) / sizeof(denied[0]); ++item) {
                size_t length = strlen(denied[item]);
                for (split = 0; split <= length; ++split) {
                        capture = (SelfTestPtyCapture){0};
                        XtpTerminalSetAllowColorOps(terminal, false);
                        XtpTerminalFeed(terminal, (const uint8_t *)denied[item], split);
                        XtpTerminalFeed(terminal, (const uint8_t *)denied[item] + split,
                                        length - split);
                        if (capture.used != 0U || capture.overflow)
                                goto failed;
                        XtpTerminalSetAllowColorOps(terminal, true);
                        XtpTerminalFeed(terminal, query, sizeof(query) - 1U);
                        if (capture.overflow || capture.used != sizeof(expected) - 1U ||
                            memcmp(capture.bytes, expected, sizeof(expected) - 1U) != 0)
                                goto failed;
                }
        }
        result = 0;
        goto done;
failed:
        XtpLog(XTP_LOG_ERROR, "self-test", "color policy failed case=%zu split=%zu length=%zu",
               item, split, capture.used);
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestAnsiPalette(void)
{
        static const uint8_t high_query[] = "\033]4;200;?\033\\";
        static const uint8_t override_reset[] = "\033]4;0;#abcdef\033\\"
                                                "\033]4;0;?\033\\"
                                                "\033]104;0\033\\"
                                                "\033]4;0;?\033\\";
        static const uint8_t override_reset_expected[] = "\033]4;0;rgb:abab/cdcd/efef\033\\"
                                                         "\033]4;0;rgb:0000/2020/4040\033\\";
        XtpRgbColor palette[XTP_ANSI_PALETTE_SIZE];
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        uint8_t high_reply[sizeof(capture.bytes)];
        char query[256];
        char expected[768];
        size_t high_reply_length;
        size_t query_length = 0;
        size_t expected_length = 0;
        unsigned int index;
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        XtpTerminalFeed(terminal, high_query, sizeof(high_query) - 1U);
        if (capture.overflow || capture.used == 0)
                goto done;
        high_reply_length = capture.used;
        memcpy(high_reply, capture.bytes, high_reply_length);
        capture = (SelfTestPtyCapture){0};
        for (index = 0; index < XTP_ANSI_PALETTE_SIZE; ++index) {
                int written;

                palette[index] = (XtpRgbColor){(uint8_t)index, (uint8_t)(0x20U + index),
                                               (uint8_t)(0x40U + index)};
                written = snprintf(query + query_length, sizeof(query) - query_length,
                                   "\033]4;%u;?\033\\", index);
                if (written < 0 || (size_t)written >= sizeof(query) - query_length)
                        goto done;
                query_length += (size_t)written;
                written =
                    snprintf(expected + expected_length, sizeof(expected) - expected_length,
                             "\033]4;%u;rgb:%02x%02x/%02x%02x/%02x%02x\033\\", index, index, index,
                             0x20U + index, 0x20U + index, 0x40U + index, 0x40U + index);
                if (written < 0 || (size_t)written >= sizeof(expected) - expected_length)
                        goto done;
                expected_length += (size_t)written;
        }
        if (XtpTerminalSetAnsiPalette(terminal, palette) != 0)
                goto done;
        XtpTerminalFeed(terminal, (const uint8_t *)query, query_length);
        if (capture.overflow || capture.used != expected_length ||
            memcmp(capture.bytes, expected, expected_length) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "ANSI-palette query mismatch length=%zu",
                       capture.used);
                goto done;
        }
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, high_query, sizeof(high_query) - 1U);
        if (!SelfTestPtyEquals(&capture, high_reply, high_reply_length)) {
                XtpLog(XTP_LOG_ERROR, "self-test", "ANSI-palette high-index changed length=%zu",
                       capture.used);
                goto done;
        }
        capture = (SelfTestPtyCapture){0};
        XtpTerminalFeed(terminal, override_reset, sizeof(override_reset) - 1U);
        if (capture.overflow || capture.used != sizeof(override_reset_expected) - 1U ||
            memcmp(capture.bytes, override_reset_expected, sizeof(override_reset_expected) - 1U) !=
                0) {
                XtpLog(XTP_LOG_ERROR, "self-test",
                       "ANSI-palette override/reset mismatch length=%zu", capture.used);
                goto done;
        }
        result = 0;
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestKittyKeyboardState(void)
{
        static const uint8_t query_order[] = "\033[?u\033[c";
        static const uint8_t query_order_expected[] = "\033[?0u" XTP_DA1_REPLY;
        static const uint8_t state_transitions[] = "\033[=1;1u\033[?u" /* set: 1 */
                                                   "\033[=2;2u\033[?u" /* augment: 1 | 2 = 3 */
                                                   "\033[=1;3u\033[?u" /* clear: 3 & ~1 = 2 */
                                                   "\033[>4u\033[?u"   /* outer push: 4 */
                                                   "\033[>8u\033[?u"   /* inner push: 8 */
                                                   "\033[<u\033[?u"    /* restore outer: 4 */
                                                   "\033[<u\033[?u";   /* restore original: 2 */
        static const uint8_t state_expected[] =
            "\033[?1u\033[?3u\033[?2u\033[?4u\033[?8u\033[?4u\033[?2u";
        XtpTerminal *terminal;
        SelfTestPtyCapture capture = {0};
        XtpTerminalEffects effects = {
            .write_pty = SelfTestCapturePty,
            .closure = &capture,
        };
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        XtpTerminalSetEffects(terminal, &effects);
        XtpTerminalFeed(terminal, query_order, sizeof(query_order) - 1U);
        if (capture.overflow || capture.used != sizeof(query_order_expected) - 1U ||
            memcmp(capture.bytes, query_order_expected, sizeof(query_order_expected) - 1U) != 0)
                goto mismatch;
        capture.used = 0;
        capture.overflow = false;
        XtpTerminalFeed(terminal, state_transitions, sizeof(state_transitions) - 1U);
        if (capture.overflow || capture.used != sizeof(state_expected) - 1U ||
            memcmp(capture.bytes, state_expected, sizeof(state_expected) - 1U) != 0)
                goto mismatch;
        result = 0;
        goto done;
mismatch:
        XtpLog(XTP_LOG_ERROR, "self-test", "Kitty keyboard state mismatch length=%zu bytes=%.*s",
               capture.used, (int)capture.used, capture.bytes);
done:
        XtpTerminalFree(terminal);
        return result;
}

static int
SelfTestMouse(void)
{
        static const uint8_t sgr_normal[] = "\033[?1000h\033[?1006h";
        static const uint8_t sgr_button[] = "\033[?1002h";
        static const uint8_t sgr_any[] = "\033[?1003h";
        static const uint8_t x10_mode[] = "\033[?1003l\033[?1006l\033[?9h";
        static const uint8_t urxvt_mode[] = "\033[?9l\033[?1000h\033[?1015h";
        static const uint8_t pixel_mode[] = "\033[?1015l\033[?1016h";
        static const uint8_t utf8_mode[] = "\033[?1016l\033[?1005h";
        static const char x10_left[] = {'\033', '[', 'M', 32, 34, 34};
        static const char utf8_right[] = {'\033', '[', 'M', 34, (char)0xc3, (char)0xa9, 34};
        XtpMouseEvent event = {
            .action = XTP_MOUSE_ACTION_PRESS,
            .button = XTP_MOUSE_BUTTON_LEFT,
            .x = 10.0f,
            .y = 18.0f,
            .screen_width = 644,
            .screen_height = 388,
            .cell_width = 8,
            .cell_height = 16,
            .padding_top = 2,
            .padding_bottom = 2,
            .padding_left = 2,
            .padding_right = 2,
            .any_button_pressed = true,
        };
        XtpTerminal *terminal;
        char encoded[128];
        size_t written = 0;
        int result = -1;

        if (XtpTerminalBackendIsStub())
                return 0;
        terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        if (terminal == NULL)
                return -1;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto done;
        XtpTerminalFeed(terminal, sgr_normal, sizeof(sgr_normal) - 1U);
        if (!XtpTerminalMouseTracking(terminal) ||
            XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[<0;2;2M", 9) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_RELEASE;
        event.any_button_pressed = false;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[<0;2;2m", 9) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, sgr_button, sizeof(sgr_button) - 1U);
        event.action = XTP_MOUSE_ACTION_MOTION;
        event.any_button_pressed = true;
        event.x = 18.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<32;3;2M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, sgr_any, sizeof(sgr_any) - 1U);
        event.button = XTP_MOUSE_BUTTON_NONE;
        event.any_button_pressed = false;
        event.x = 26.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<35;4;2M", 10) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_FOUR;
        event.x = 10.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<64;2;2M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, x10_mode, sizeof(x10_mode) - 1U);
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_LEFT;
        event.modifiers = XTP_MOD_CONTROL | XTP_MOD_ALT;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != sizeof(x10_left) || memcmp(encoded, x10_left, sizeof(x10_left)) != 0)
                goto mismatch;
        event.action = XTP_MOUSE_ACTION_RELEASE;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, urxvt_mode, sizeof(urxvt_mode) - 1U);
        event.action = XTP_MOUSE_ACTION_PRESS;
        event.button = XTP_MOUSE_BUTTON_RIGHT;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 9 || memcmp(encoded, "\033[58;2;2M", 9) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, pixel_mode, sizeof(pixel_mode) - 1U);
        event.modifiers = 0;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != 10 || memcmp(encoded, "\033[<2;8;16M", 10) != 0)
                goto mismatch;
        XtpTerminalFeed(terminal, utf8_mode, sizeof(utf8_mode) - 1U);
        event.screen_width = 2404;
        event.x = 1602.0f;
        if (XtpTerminalEncodeMouse(terminal, &event, encoded, sizeof(encoded), &written) != 0 ||
            written != sizeof(utf8_right) || memcmp(encoded, utf8_right, sizeof(utf8_right)) != 0)
                goto mismatch;
        result = 0;
        goto done;
mismatch:
        XtpLog(XTP_LOG_ERROR, "self-test", "mouse encoding mismatch length=%zu bytes=%.*s", written,
               (int)written, encoded);
done:
        XtpTerminalFree(terminal);
        return result;
}

typedef struct
{
        unsigned int width;
        unsigned int height;
} BoxSize;

static Boolean
BoxMask(uint32_t codepoint, BoxSize size, Boolean bold, uint8_t *mask, XtpBoxGlyph *glyph_out)
{
        XtpBoxGlyph glyph = {0};

        if (!XtpBoxGlyphPlan(codepoint, size.width, size.height, bold, &glyph))
                return False;
        XtpBoxGlyphRasterize(&glyph, size.width, size.height, mask);
        if (glyph_out != NULL)
                *glyph_out = glyph;
        else
                XtpBoxGlyphFree(&glyph);
        return True;
}

static unsigned int
BoxInk(const uint8_t *mask, BoxSize size)
{
        unsigned int total = 0;
        unsigned int index;

        for (index = 0; index < size.width * size.height; ++index)
                total += mask[index];
        return total;
}

static Boolean
BoxSameColumns(const uint8_t *mask, BoxSize size, unsigned int left, unsigned int right)
{
        unsigned int y;

        for (y = 0; y < size.height; ++y)
                if (mask[y * size.width + left] != mask[y * size.width + right])
                        return False;
        return True;
}

static Boolean
BoxSameRows(const uint8_t *mask, BoxSize size, unsigned int top, unsigned int bottom)
{
        return memcmp(mask + top * size.width, mask + bottom * size.width, size.width) == 0;
}

static Boolean
BoxPartition(const uint8_t *left, const uint8_t *right, BoxSize size)
{
        unsigned int index;

        for (index = 0; index < size.width * size.height; ++index)
                if (left[index] + right[index] != 1)
                        return False;
        return True;
}

static int
SelfTestBoxGlyphs(void)
{
        static const BoxSize sizes[] = {{6, 13},  {9, 19}, {10, 20},  {7, 14},
                                        {24, 48}, {5, 8},  {24, 170}, {1030, 1100}};
        static const uint32_t partitions[][2] = {
            {0x2580U, 0x2584U}, {0x258CU, 0x2590U}, {0x2598U, 0x259FU}, {0x259DU, 0x2599U},
            {0x2596U, 0x259CU}, {0x2597U, 0x259BU}, {0x259AU, 0x259EU}};
        uint8_t *a = malloc(1030U * 1100U);
        uint8_t *b = malloc(1030U * 1100U);
        uint8_t *c = malloc(1030U * 1100U);
        uint32_t codepoint = 0;
        size_t size_index;
        unsigned int x;
        unsigned int y;
        XtpBoxGlyph glyph = {0};
        int result = -1;

        if (a == NULL || b == NULL || c == NULL)
                goto done;
        if (XtpBoxGlyphCodepoint(0x24FFU) || !XtpBoxGlyphCodepoint(0x2500U) ||
            !XtpBoxGlyphCodepoint(0x259FU) || XtpBoxGlyphCodepoint(0x25A0U) ||
            !XtpBoxGlyphText("\xe2\x94\x80", 3, &codepoint) || codepoint != 0x2500U ||
            XtpBoxGlyphText("\xe2\x94\x80\xcc\x81", 5, NULL) || XtpBoxGlyphText("a", 1, NULL) ||
            XtpBoxGlyphText("\xe2\x94", 2, NULL) || XtpBoxGlyphText("", 0, NULL) ||
            XtpBoxGlyphPlan(0x2500U, 0, 13, false, &glyph) ||
            XtpBoxGlyphPlan(0x25A0U, 9, 19, false, &glyph))
                goto done;
        if (XtpBoxGlyphThickness(6, 13, false) != 1 || XtpBoxGlyphThickness(6, 13, true) != 2 ||
            XtpBoxGlyphThickness(24, 48, false) != 3 || XtpBoxGlyphThickness(24, 48, true) != 4 ||
            XtpBoxGlyphThickness(24, 170, false) != 3 || XtpBoxGlyphThickness(9, 170, false) != 1)
                goto done;
        for (size_index = 0; size_index < XtNumber(sizes); ++size_index) {
                BoxSize size = sizes[size_index];
                unsigned int thickness = XtpBoxGlyphThickness(size.width, size.height, false);
                unsigned int band0 = (size.height - thickness) / 2U;
                unsigned int index;
                size_t rect;

                /* Every glyph plans, stays inside the cell and is not blank. */
                for (codepoint = 0x2500U; codepoint <= 0x259FU; ++codepoint) {
                        if (!BoxMask(codepoint, size, False, a, &glyph) || BoxInk(a, size) == 0)
                                goto done;
                        for (rect = 0; rect < glyph.count; ++rect) {
                                if (glyph.rects[rect].x + glyph.rects[rect].width > size.width ||
                                    glyph.rects[rect].y + glyph.rects[rect].height > size.height)
                                        goto done;
                        }
                        if ((glyph.shade != XTP_BOX_SHADE_NONE) !=
                            (codepoint >= 0x2591U && codepoint <= 0x2593U))
                                goto done;
                        XtpBoxGlyphFree(&glyph);
                }
                /* Light horizontal: one full-width band of the light thickness. */
                if (!BoxMask(0x2500U, size, False, a, NULL))
                        goto done;
                for (y = 0; y < size.height; ++y)
                        for (x = 0; x < size.width; ++x)
                                if (a[y * size.width + x] !=
                                    (y >= band0 && y < band0 + thickness ? 1U : 0U))
                                        goto done;
                if (!BoxSameColumns(a, size, 0, size.width - 1U))
                        goto done;
                /* Light vertical: the same rule turned, joining top to bottom. */
                if (!BoxMask(0x2502U, size, False, b, NULL) ||
                    !BoxSameRows(b, size, 0, size.height - 1U) ||
                    BoxInk(b, size) != thickness * size.height)
                        goto done;
                /* Cross is exactly the union; corners and tees are subsets that reach the edges. */
                if (!BoxMask(0x253CU, size, False, c, NULL))
                        goto done;
                for (index = 0; index < size.width * size.height; ++index)
                        if (c[index] != (a[index] | b[index]))
                                goto done;
                {
                        static const uint32_t joins[] = {0x250CU, 0x2510U, 0x2514U, 0x2518U,
                                                         0x251CU, 0x2524U, 0x252CU, 0x2534U};
                        size_t join;

                        for (join = 0; join < XtNumber(joins); ++join) {
                                Boolean right = joins[join] == 0x250CU || joins[join] == 0x2514U ||
                                                joins[join] == 0x251CU || joins[join] == 0x252CU ||
                                                joins[join] == 0x2534U;
                                Boolean left = joins[join] == 0x2510U || joins[join] == 0x2518U ||
                                               joins[join] == 0x2524U || joins[join] == 0x252CU ||
                                               joins[join] == 0x2534U;
                                Boolean down = joins[join] == 0x250CU || joins[join] == 0x2510U ||
                                               joins[join] == 0x251CU || joins[join] == 0x2524U ||
                                               joins[join] == 0x252CU;
                                Boolean up = joins[join] == 0x2514U || joins[join] == 0x2518U ||
                                             joins[join] == 0x251CU || joins[join] == 0x2524U ||
                                             joins[join] == 0x2534U;

                                if (!BoxMask(joins[join], size, False, c, NULL))
                                        goto done;
                                for (index = 0; index < size.width * size.height; ++index)
                                        if (c[index] && !(a[index] | b[index]))
                                                goto done;
                                if (BoxSameColumns(c, size, size.width - 1U, size.width - 1U) &&
                                    (right != (c[band0 * size.width + size.width - 1U] != 0) ||
                                     left != (c[band0 * size.width] != 0)))
                                        goto done;
                                if (down != (c[(size.height - 1U) * size.width +
                                               (size.width - thickness) / 2U] != 0) ||
                                    up != (c[(size.width - thickness) / 2U] != 0))
                                        goto done;
                        }
                }
                /* Heavy is a thicker centered band containing the light one. */
                if (!BoxMask(0x2501U, size, False, c, NULL) ||
                    BoxInk(c, size) != 2U * thickness * size.width ||
                    !BoxSameColumns(c, size, 0, size.width - 1U))
                        goto done;
                for (index = 0; index < size.width * size.height; ++index)
                        if (a[index] && !c[index])
                                goto done;
                /* Bold adds one pixel of thickness. */
                if (!BoxMask(0x2500U, size, True, c, NULL) ||
                    BoxInk(c, size) != (thickness + 1U) * size.width)
                        goto done;
                /* Double lines: two bands with a gap, joining a double corner exactly. */
                if (size.height >= 13) {
                        unsigned int rows = 0;

                        if (!BoxMask(0x2550U, size, False, a, NULL) ||
                            !BoxSameColumns(a, size, 0, size.width - 1U))
                                goto done;
                        for (y = 0; y < size.height; ++y)
                                rows += a[y * size.width];
                        if (rows != 2U * thickness ||
                            a[((size.height - 3U * thickness) / 2U + thickness) * size.width] != 0)
                                goto done;
                        if (!BoxMask(0x2554U, size, False, b, NULL) ||
                            !BoxMask(0x2551U, size, False, c, NULL))
                                goto done;
                        for (y = 0; y < size.height; ++y)
                                if (b[y * size.width + size.width - 1U] != a[y * size.width])
                                        goto done;
                        if (memcmp(b + (size.height - 1U) * size.width, c, size.width) != 0)
                                goto done;
                        /* A single line crosses a double pair without a gap. */
                        if (!BoxMask(0x256AU, size, False, b, NULL) ||
                            !BoxMask(0x2502U, size, False, c, NULL))
                                goto done;
                        for (index = 0; index < size.width * size.height; ++index)
                                if (c[index] && !b[index])
                                        goto done;
                }
                /* Blocks partition the cell, eighths grow monotonically, full block fills. */
                if (!BoxMask(0x2588U, size, False, a, NULL) ||
                    BoxInk(a, size) != size.width * size.height)
                        goto done;
                for (index = 0; index < XtNumber(partitions); ++index) {
                        if (!BoxMask(partitions[index][0], size, False, b, NULL) ||
                            !BoxMask(partitions[index][1], size, False, c, NULL) ||
                            !BoxPartition(b, c, size))
                                goto done;
                }
                if (size.height >= 8 && size.width >= 8) {
                        unsigned int previous = 0;

                        for (codepoint = 0x2581U; codepoint <= 0x2588U; ++codepoint) {
                                unsigned int ink;

                                if (!BoxMask(codepoint, size, False, b, NULL))
                                        goto done;
                                ink = BoxInk(b, size);
                                if (ink <= previous || ink % size.width != 0 ||
                                    b[(size.height - 1U) * size.width] == 0)
                                        goto done;
                                previous = ink;
                        }
                        previous = size.width * size.height + 1U;
                        for (codepoint = 0x2589U; codepoint <= 0x258FU; ++codepoint) {
                                unsigned int ink;

                                if (!BoxMask(codepoint, size, False, b, NULL))
                                        goto done;
                                ink = BoxInk(b, size);
                                if (ink >= previous || ink % size.height != 0 || b[0] == 0)
                                        goto done;
                                previous = ink;
                        }
                }
                /* Shades approximate one, two and three quarters. */
                for (codepoint = 0x2591U; codepoint <= 0x2593U; ++codepoint) {
                        unsigned int ink;
                        unsigned int expected;

                        if (!BoxMask(codepoint, size, False, b, &glyph) || glyph.count != 0)
                                goto done;
                        XtpBoxGlyphFree(&glyph);
                        ink = BoxInk(b, size);
                        expected = size.width * size.height * (codepoint - 0x2590U) / 4U;
                        if (ink + size.width + size.height < expected ||
                            ink > expected + size.width + size.height)
                                goto done;
                }
                /* Dashes keep the light band but leave gaps; vertical dashes likewise. */
                if (!BoxMask(0x2504U, size, False, b, NULL) || BoxInk(b, size) == 0 ||
                    BoxInk(b, size) >= thickness * size.width)
                        goto done;
                for (y = 0; y < size.height; ++y)
                        if ((y < band0 || y >= band0 + thickness) && b[y * size.width] != 0)
                                goto done;
                if (!BoxMask(0x250AU, size, False, b, NULL) || BoxInk(b, size) == 0 ||
                    BoxInk(b, size) >= thickness * size.height)
                        goto done;
                /* Arcs reach their two edges and leave the opposite corner empty. */
                if (!BoxMask(0x256DU, size, False, b, NULL) ||
                    b[(size.height - 1U) * size.width + (size.width - thickness) / 2U] == 0 ||
                    b[band0 * size.width + size.width - 1U] == 0 || b[0] != 0 ||
                    b[size.width - 1U] != 0 || b[(size.height - 1U) * size.width] != 0)
                        goto done;
                if (!BoxMask(0x2570U, size, False, b, NULL) ||
                    b[(size.width - thickness) / 2U] == 0 ||
                    b[band0 * size.width + size.width - 1U] == 0 ||
                    b[size.height * size.width - 1U] != 0)
                        goto done;
                /* Diagonals run corner to corner; the cross is their union. */
                if (!BoxMask(0x2572U, size, False, a, NULL) || a[0] == 0 ||
                    a[size.height * size.width - 1U] == 0 || a[size.width - 1U] != 0)
                        goto done;
                if (!BoxMask(0x2571U, size, False, b, NULL) || b[size.width - 1U] == 0 ||
                    b[(size.height - 1U) * size.width] == 0 || b[0] != 0)
                        goto done;
                if (!BoxMask(0x2573U, size, False, c, NULL))
                        goto done;
                for (index = 0; index < size.width * size.height; ++index)
                        if (c[index] != (a[index] | b[index]))
                                goto done;
        }
        result = 0;
done:
        XtpBoxGlyphFree(&glyph);
        free(a);
        free(b);
        free(c);
        return result;
}

static unsigned int box_alloc_budget;

static void *
BoxFailingRealloc(void *pointer, size_t size)
{
        if (box_alloc_budget == 0)
                return NULL;
        --box_alloc_budget;
        return realloc(pointer, size);
}

static Boolean
BoxBounds(const uint8_t *mask, BoxSize size, unsigned int *left, unsigned int *top,
          unsigned int *width, unsigned int *height)
{
        unsigned int right = 0;
        unsigned int bottom = 0;
        unsigned int x;
        unsigned int y;

        *left = size.width;
        *top = size.height;
        for (y = 0; y < size.height; ++y) {
                for (x = 0; x < size.width; ++x) {
                        if (mask[y * size.width + x] == 0)
                                continue;
                        *left = x < *left ? x : *left;
                        *top = y < *top ? y : *top;
                        right = x + 1U > right ? x + 1U : right;
                        bottom = y + 1U > bottom ? y + 1U : bottom;
                }
        }
        if (right == 0)
                return False;
        *width = right - *left;
        *height = bottom - *top;
        return True;
}

/* Returns the number of inked runs in row y, the first run's start and the last run's end. */
static unsigned int
BoxRowRuns(const uint8_t *mask, BoxSize size, unsigned int y, unsigned int *start,
           unsigned int *end)
{
        const uint8_t *row = mask + y * size.width;
        unsigned int runs = 0;
        unsigned int x;

        *start = 0;
        *end = 0;
        for (x = 0; x < size.width; ++x) {
                if (row[x] == 0)
                        continue;
                if (x == 0 || row[x - 1U] == 0) {
                        if (runs == 0)
                                *start = x;
                        ++runs;
                }
                *end = x + 1U;
        }
        return runs;
}

static Boolean
BoxMirrored(const uint8_t *left, const uint8_t *right, BoxSize size, Boolean horizontal,
            Boolean vertical)
{
        unsigned int x;
        unsigned int y;

        for (y = 0; y < size.height; ++y) {
                for (x = 0; x < size.width; ++x) {
                        unsigned int mx = horizontal ? size.width - 1U - x : x;
                        unsigned int my = vertical ? size.height - 1U - y : y;

                        if (left[y * size.width + x] != right[my * size.width + mx])
                                return False;
                }
        }
        return True;
}

static Boolean
BoxSubset(const uint8_t *inner, const uint8_t *outer, BoxSize size)
{
        unsigned int index;

        for (index = 0; index < size.width * size.height; ++index)
                if (inner[index] && !outer[index])
                        return False;
        return True;
}

/* Solid rows are one run from the flat side; thin rows end on the solid outline and connect. */
static Boolean
BoxPowerlineRows(const uint8_t *solid, const uint8_t *thin, BoxSize size)
{
        unsigned int previous_start = 0;
        unsigned int previous_end = 0;
        unsigned int y;

        for (y = 0; y < size.height; ++y) {
                unsigned int start;
                unsigned int end;
                unsigned int thin_start;
                unsigned int thin_end;

                if (BoxRowRuns(solid, size, y, &start, &end) != 1 || start != 0)
                        return False;
                if (thin == NULL)
                        continue;
                if (BoxRowRuns(thin, size, y, &thin_start, &thin_end) != 1 || thin_end != end ||
                    ((y == 0 || y + 1U == size.height) && thin_start != 0) ||
                    (y != 0 && (thin_start > previous_end || previous_start > thin_end)))
                        return False;
                previous_start = thin_start;
                previous_end = thin_end;
        }
        return True;
}

static Boolean
BoxPowerlineBulge(const uint8_t *mask, BoxSize size)
{
        unsigned int middle = (size.height - 1U) / 2U;
        unsigned int previous = 0;
        unsigned int y;

        if (!BoxMirrored(mask, mask, size, False, True))
                return False;
        for (y = 0; y <= middle; ++y) {
                unsigned int start;
                unsigned int end;

                (void)BoxRowRuns(mask, size, y, &start, &end);
                if (end < previous)
                        return False;
                previous = end;
        }
        return previous == size.width;
}

static int
SelfTestProceduralGlyphs(void)
{
        static const BoxSize sizes[] = {{6, 13},  {9, 19}, {7, 14},   {5, 8},      {3, 6},
                                        {10, 21}, {8, 16}, {24, 170}, {1030, 1100}};
        static const struct
        {
                BoxSize size;
                unsigned int dot;
                unsigned int x[2];
                unsigned int y[4];
        } layouts[] = {{{6, 13}, 1, {1, 4}, {1, 4, 7, 10}}, {{9, 19}, 2, {1, 6}, {1, 6, 11, 16}}};
        static const struct
        {
                uint32_t codepoint;
                Boolean fits_one_allocation;
        } failures[] = {{0x2500U, True},  {0x256DU, False}, {0x2573U, False}, {0x28FFU, True},
                        {0xE0B0U, False}, {0xE0B5U, False}, {0xE0B9U, False}};
        static const uint8_t dot_column[8] = {0, 0, 0, 1, 1, 1, 0, 1};
        static const uint8_t dot_row[8] = {0, 1, 2, 0, 1, 2, 3, 3};
        uint8_t *a = malloc(1030U * 1100U);
        uint8_t *b = malloc(1030U * 1100U);
        uint8_t *c = malloc(1030U * 1100U);
        uint32_t codepoint = 0;
        size_t size_index;
        size_t index;
        unsigned int bit;
        XtpBoxGlyph glyph = {0};
        int result = -1;

        if (a == NULL || b == NULL || c == NULL)
                goto done;
        if (XtpBoxGlyphCodepoint(0x27FFU) || !XtpBoxGlyphCodepoint(0x2800U) ||
            !XtpBoxGlyphCodepoint(0x28FFU) || XtpBoxGlyphCodepoint(0x2900U) ||
            XtpBoxGlyphCodepoint(0xE0A0U) || XtpBoxGlyphCodepoint(0xE0AFU) ||
            !XtpBoxGlyphCodepoint(0xE0B0U) || !XtpBoxGlyphCodepoint(0xE0BFU) ||
            XtpBoxGlyphCodepoint(0xE0C0U) || XtpBoxGlyphCodepoint(0xE0D2U) ||
            XtpBoxGlyphCodepoint(0xE0D4U) || !XtpBoxGlyphText("\xee\x82\xb0", 3, &codepoint) ||
            codepoint != 0xE0B0U || !XtpBoxGlyphText("\xe2\xa3\xbf", 3, &codepoint) ||
            codepoint != 0x28FFU)
                goto done;
        /* Braille declines when a dot would vanish, leaving the glyph empty. */
        if (XtpBoxGlyphPlan(0x28FFU, 1, 8, false, &glyph) || glyph.rects != NULL ||
            glyph.count != 0 || XtpBoxGlyphPlan(0x2801U, 2, 3, false, &glyph))
                goto done;
        /* Golden braille layouts at two odd cell sizes. */
        for (index = 0; index < XtNumber(layouts); ++index) {
                for (bit = 0; bit < 8; ++bit) {
                        unsigned int left;
                        unsigned int top;
                        unsigned int width;
                        unsigned int height;

                        if (!BoxMask(0x2800U | (1U << bit), layouts[index].size, False, a, NULL) ||
                            !BoxBounds(a, layouts[index].size, &left, &top, &width, &height) ||
                            left != layouts[index].x[dot_column[bit]] ||
                            top != layouts[index].y[dot_row[bit]] || width != layouts[index].dot ||
                            height != layouts[index].dot ||
                            BoxInk(a, layouts[index].size) != width * height)
                                goto done;
                }
        }
        for (size_index = 0; size_index < XtNumber(sizes); ++size_index) {
                BoxSize size = sizes[size_index];
                unsigned int cells = size.width * size.height;
                unsigned int dot_x[8];
                unsigned int dot_y[8];
                unsigned int dot = 0;
                unsigned int pattern;

                /* Braille: blank is empty, each bit is one equal square in its column and row. */
                if (!BoxMask(0x2800U, size, False, a, NULL) || BoxInk(a, size) != 0)
                        goto done;
                memset(c, 0, cells);
                for (bit = 0; bit < 8; ++bit) {
                        unsigned int width;
                        unsigned int height;

                        if (!BoxMask(0x2800U | (1U << bit), size, False, a, NULL) ||
                            !BoxBounds(a, size, &dot_x[bit], &dot_y[bit], &width, &height) ||
                            width != height || BoxInk(a, size) != width * height ||
                            (bit != 0 && width != dot))
                                goto done;
                        dot = width;
                        for (index = 0; index < cells; ++index)
                                c[index] |= a[index];
                }
                if (dot_x[0] != dot_x[1] || dot_x[0] != dot_x[2] || dot_x[0] != dot_x[6] ||
                    dot_x[3] != dot_x[4] || dot_x[3] != dot_x[5] || dot_x[3] != dot_x[7] ||
                    dot_x[3] < dot_x[0] + dot + 1U || dot_y[0] != dot_y[3] ||
                    dot_y[1] != dot_y[4] || dot_y[2] != dot_y[5] || dot_y[6] != dot_y[7] ||
                    dot_y[1] < dot_y[0] + dot || dot_y[2] < dot_y[1] + dot ||
                    dot_y[6] < dot_y[2] + dot)
                        goto done;
                if (size.height >= 8 && dot_y[1] == dot_y[0] + dot)
                        goto done;
                if (size.height >= 10 && (dot_y[0] == 0 || dot_y[6] + dot >= size.height))
                        goto done;
                if (size.width >= 5 && (dot_x[0] == 0 || dot_x[3] + dot >= size.width))
                        goto done;
                if (!BoxMask(0x28FFU, size, False, a, NULL) || memcmp(a, c, cells) != 0 ||
                    !BoxMask(0x28FFU, size, True, b, NULL) || memcmp(a, b, cells) != 0)
                        goto done;
                /* Every pattern inks exactly its own dots. */
                for (pattern = 0; cells <= 50000U && pattern < 256U; ++pattern) {
                        unsigned int count = 0;

                        if (!BoxMask(0x2800U + pattern, size, False, a, NULL))
                                goto done;
                        for (bit = 0; bit < 8; ++bit) {
                                Boolean set = (pattern >> bit) & 1U;

                                count += set ? 1U : 0U;
                                if ((a[dot_y[bit] * size.width + dot_x[bit]] != 0) != set)
                                        goto done;
                        }
                        if (BoxInk(a, size) != count * dot * dot)
                                goto done;
                }

                /* Powerline: every supported symbol plans inside the cell and has ink. */
                for (codepoint = 0xE0B0U; codepoint <= 0xE0BFU; ++codepoint) {
                        if (!BoxMask(codepoint, size, False, a, &glyph) || BoxInk(a, size) == 0)
                                goto done;
                        for (index = 0; index < glyph.count; ++index)
                                if (glyph.rects[index].x + glyph.rects[index].width > size.width ||
                                    glyph.rects[index].y + glyph.rects[index].height > size.height)
                                        goto done;
                        XtpBoxGlyphFree(&glyph);
                }
                /* Arrows fill every row from the flat side, bulge to the tip, and ignore bold. */
                if (!BoxMask(0xE0B0U, size, False, a, NULL) ||
                    !BoxMask(0xE0B1U, size, False, b, NULL) || !BoxPowerlineRows(a, b, size) ||
                    !BoxPowerlineBulge(a, size) || !BoxMask(0xE0B0U, size, True, c, NULL) ||
                    memcmp(a, c, cells) != 0 || !BoxMask(0xE0B2U, size, False, c, NULL) ||
                    !BoxMirrored(a, c, size, True, False) ||
                    !BoxMask(0xE0B3U, size, False, c, NULL) ||
                    !BoxMirrored(b, c, size, True, False) ||
                    !BoxMask(0xE0B1U, size, True, c, NULL) || !BoxSubset(b, c, size))
                        goto done;
                /* Half circles contain the arrow and follow the same row rules. */
                if (!BoxMask(0xE0B4U, size, False, c, NULL) || !BoxSubset(a, c, size) ||
                    !BoxMask(0xE0B5U, size, False, b, NULL) || !BoxPowerlineRows(c, b, size) ||
                    !BoxPowerlineBulge(c, size) || !BoxMask(0xE0B6U, size, False, a, NULL) ||
                    !BoxMirrored(c, a, size, True, False) ||
                    !BoxMask(0xE0B7U, size, False, a, NULL) ||
                    !BoxMirrored(b, a, size, True, False))
                        goto done;
                /* Slants widen downward to a full last row; the other three are its reflections. */
                if (!BoxMask(0xE0B8U, size, False, a, NULL) || !BoxPowerlineRows(a, NULL, size))
                        goto done;
                {
                        unsigned int previous = 0;
                        unsigned int y;

                        for (y = 0; y < size.height; ++y) {
                                unsigned int start;
                                unsigned int end;

                                (void)BoxRowRuns(a, size, y, &start, &end);
                                if (end < previous)
                                        goto done;
                                previous = end;
                        }
                        if (previous != size.width)
                                goto done;
                }
                if (!BoxMask(0xE0BAU, size, False, b, NULL) ||
                    !BoxMirrored(a, b, size, True, False) ||
                    !BoxMask(0xE0BCU, size, False, b, NULL) ||
                    !BoxMirrored(a, b, size, False, True) ||
                    !BoxMask(0xE0BEU, size, False, b, NULL) || !BoxMirrored(a, b, size, True, True))
                        goto done;
                /* Separator lines are the box-drawing diagonals. */
                if (!BoxMask(0x2572U, size, False, a, NULL) ||
                    !BoxMask(0xE0B9U, size, False, b, NULL) || memcmp(a, b, cells) != 0 ||
                    !BoxMask(0xE0BFU, size, False, b, NULL) || memcmp(a, b, cells) != 0 ||
                    !BoxMask(0x2571U, size, False, a, NULL) ||
                    !BoxMask(0xE0BBU, size, False, b, NULL) || memcmp(a, b, cells) != 0 ||
                    !BoxMask(0xE0BDU, size, False, b, NULL) || memcmp(a, b, cells) != 0)
                        goto done;
        }
        /* An allocation failure declines the plan and releases everything it took. */
        for (index = 0; index < XtNumber(failures); ++index) {
                bool planned;

                for (bit = 0; bit < 2; ++bit) {
                        box_alloc_budget = bit;
                        XtpBoxGlyphSetAllocator(BoxFailingRealloc);
                        planned =
                            XtpBoxGlyphPlan(failures[index].codepoint, 24, 170, false, &glyph);
                        XtpBoxGlyphSetAllocator(NULL);
                        if (planned != (bit == 1 && failures[index].fits_one_allocation))
                                goto done;
                        if (!planned && (glyph.rects != NULL || glyph.count != 0 || glyph.failed))
                                goto done;
                        XtpBoxGlyphFree(&glyph);
                }
                if (!XtpBoxGlyphPlan(failures[index].codepoint, 24, 170, false, &glyph))
                        goto done;
                XtpBoxGlyphFree(&glyph);
        }
        result = 0;
done:
        XtpBoxGlyphSetAllocator(NULL);
        XtpBoxGlyphFree(&glyph);
        free(a);
        free(b);
        free(c);
        return result;
}

typedef int (*SelfTestCaseFn)(void);

typedef struct
{
        const char *name;
        SelfTestCaseFn run;
} SelfTestCase;

static int
RunSelfTestCases(const SelfTestCase *cases, size_t count)
{
        size_t index;

        for (index = 0; index < count; ++index) {
                if (cases[index].run() != 0) {
                        XtpLog(XTP_LOG_ERROR, "self-test", "%s check failed", cases[index].name);
                        return -1;
                }
        }
        return 0;
}

int
XtpSelfTest(void)
{
        static const SelfTestCase foundation_cases[] = {
            {"log-level", SelfTestLogLevels},
            {"os-release", SelfTestOsRelease},
            {"welcome readability", SelfTestWelcomeReadability},
            {"URL matching", SelfTestUrlMatch},
            {"window-ops policy", SelfTestWindowOps},
            {"title stack", SelfTestTitleStack},
            {"emoji-presentation", SelfTestEmojiPresentation},
            {"box-glyphs", SelfTestBoxGlyphs},
            {"braille and Powerline glyphs", SelfTestProceduralGlyphs},
            {"Unicode Script=Han", SelfTestUnicodeScript},
            {"font-chain", SelfTestFontChain},
            {"font-metrics", SelfTestFontMetrics},
            {"font-report bound", SelfTestFontReportBound},
            {"font-route-cache", SelfTestFontRouteCache},
            {"background-opacity", SelfTestBackgroundOpacity},
        };
        static const SelfTestCase backend_cases[] = {
            {"terminal reports", SelfTestTerminalReports},
            {"cursor-blink policy", SelfTestCursorBlinkPolicy},
            {"cursor-blink report", SelfTestCursorBlinkReports},
            {"default-color", SelfTestDefaultColors},
            {"color-ops", SelfTestColorOps},
            {"dynamic colors", SelfTestDynamicColors},
            {"underline color", SelfTestUnderlineColor},
            {"startup cursor shape", SelfTestStartupCursorShape},
            {"answerback", SelfTestAnswerback},
            {"terminfo name", SelfTestTerminfoName},
            {"device attributes firmware", SelfTestDeviceAttributesFirmware},
            {"device attributes", SelfTestDeviceAttributes},
            {"device attributes evidence", SelfTestDeviceAttributesEvidence},
            {"unknown APC", SelfTestUnknownApc},
            {"notification effect", SelfTestNotificationEffectDelivery},
            {"prompt navigation", SelfTestPromptNavigation},
            {"working directory decode", SelfTestWorkingDirectoryDecode},
            {"working directory effect", SelfTestWorkingDirectoryEffectDelivery},
            {"color-ops policy", SelfTestColorOpsPolicy},
            {"request Ops", SelfTestRequestOps},
            {"color scheme", SelfTestColorScheme},
            {"ANSI-palette", SelfTestAnsiPalette},
            {"scrollback-limit", SelfTestScrollbackLimit},
            {"scrollback-selection", SelfTestSelectionScrollback},
            {"tty-output scroll", SelfTestScrollTtyOutput},
            {"focus", SelfTestFocus},
            {"synchronized output", SelfTestSynchronizedOutput},
            {"OSC 52 clipboard", SelfTestOsc52},
            {"XTWINOPS title ops", SelfTestTitleOps},
            {"Kitty keyboard", SelfTestKittyKeyboardState},
            {"mouse", SelfTestMouse},
        };
        static const SelfTestCase pty_cases[] = {
            {"PTY lifecycle", SelfTestPty},
            {"PTY queue", SelfTestPtyQueue},
        };
        static const uint8_t sample[] = "plain\033[31m red\033[0m wide=界\r\n";
        XtpTerminal *terminal = XtpTerminalNewWithGraphemeWidth(80, 24, 8, 16, false);
        XtpRenderer renderer = {
            .begin = SelfTestBegin,
            .cell = SelfTestCell,
            .end = SelfTestEnd,
        };
        SelfTestRender render = {0};
        XtpKeyEvent key = {
            .action = XTP_KEY_ACTION_PRESS,
            .key = XTP_KEY_A,
            .utf8 = "a",
            .utf8_length = 1,
            .unshifted_codepoint = 'a',
        };
        char encoded[32];
        size_t written = 0;
        XtpTerminalScrollbar before;
        XtpTerminalScrollbar after;
        int line;

        if (terminal == NULL)
                return EXIT_FAILURE;
        if (RunSelfTestCases(foundation_cases, XtNumber(foundation_cases)) != 0)
                goto failure;
        if (SelfTestCharClass(terminal) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "charClass check failed");
                goto failure;
        }
        if (XtpTerminalSetScrollbackLines(terminal, 64) != 0) {
                XtpTerminalFree(terminal);
                return EXIT_FAILURE;
        }
        XtpTerminalFeed(terminal, sample, sizeof(sample) - 1U);
        for (line = 0; line < 40; ++line) {
                char text[32];
                int length = snprintf(text, sizeof(text), "history-%02d\r\n", line);

                XtpTerminalFeed(terminal, (const uint8_t *)text, (size_t)length);
        }
        XtpTerminalFeed(terminal, sample, sizeof(sample) - 1U);
        if (XtpTerminalRender(terminal, &renderer, &render, true) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "basic render check failed");
                goto failure;
        }
        if (XtpTerminalEncodeKey(terminal, &key, encoded, sizeof(encoded), &written) != 0 ||
            written != 1 || encoded[0] != 'a') {
                XtpLog(XTP_LOG_ERROR, "self-test", "basic key check failed length=%zu", written);
                goto failure;
        }
        if (XtpTerminalGetScrollbar(terminal, &before) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "basic scrollbar query failed");
                goto failure;
        }
        if (!XtpTerminalBackendIsStub() &&
            (render.nonempty_cells == 0 || !render.saw_styled_cell || !render.saw_wide_cell ||
             !render.saw_wide_tail || before.total <= before.length || before.offset == 0 ||
             XtpTerminalScrollBy(terminal, -3) != 0 ||
             XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset >= before.offset ||
             XtpTerminalScrollToBottom(terminal) != 0 ||
             XtpTerminalGetScrollbar(terminal, &after) != 0 || after.offset != before.offset)) {
                XtpLog(XTP_LOG_ERROR, "self-test", "scrollback render check failed");
                goto failure;
        }
        if (SelfTestCursorOnly(terminal, &renderer, &render) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cursor-only check failed");
                goto failure;
        }
        if (SelfTestReverseColors(&renderer) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "reverse-colors check failed");
                goto failure;
        }
        if (SelfTestModes(terminal) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "mode check failed");
                goto failure;
        }
        if (SelfTestCursorStyles(&renderer) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "cursor-style check failed");
                goto failure;
        }
        if (SelfTestSelection(&renderer) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "selection check failed");
                goto failure;
        }
        if (SelfTestHyperlinks(&renderer) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "hyperlink check failed");
                goto failure;
        }
        if (SelfTestHyperlinkHover() != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "hyperlink hover check failed");
                goto failure;
        }
        if (RunSelfTestCases(backend_cases, XtNumber(backend_cases)) != 0)
                goto failure;
        if (XtpTerminalResize(terminal, 100, 30, 9, 18) != 0) {
                XtpLog(XTP_LOG_ERROR, "self-test", "resize check failed");
                goto failure;
        }
        XtpTerminalFree(terminal);
        if (RunSelfTestCases(pty_cases, XtNumber(pty_cases)) != 0)
                return EXIT_FAILURE;

        printf("xterm+ self-test: backend=%s menus=%d/%d/%d\n", XtpTerminalBackend(),
               XTP_MAIN_MENU_ENTRIES, XTP_VT_MENU_ENTRIES, XTP_FONT_MENU_ENTRIES);
        return EXIT_SUCCESS;

failure:
        XtpTerminalFree(terminal);
        return EXIT_FAILURE;
}
