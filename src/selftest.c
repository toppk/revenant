#include "selftest.h"

#include "char_class.h"
#include "cursor_blink.h"
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
#include "version.h"
#include "welcome.h"
#include "x11_opacity.h"

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
        XtpPty *pty = XtpPtySpawn(command, 80, 24, 8, 16);
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
            XtpBackgroundOpacityParse("1.0", &alpha) != 0 || alpha != UINT16_MAX)
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
        XtpPty *pty = XtpPtySpawn(command, 80, 24, 8, 16);
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
        result = 0;
done:
        free(uri);
        XtpTerminalFree(terminal);
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
        static const uint8_t query_order_expected[] = "\033[?0u\033[?62;22c";
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
            {"emoji-presentation", SelfTestEmojiPresentation},
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
            {"ANSI-palette", SelfTestAnsiPalette},
            {"scrollback-limit", SelfTestScrollbackLimit},
            {"scrollback-selection", SelfTestSelectionScrollback},
            {"tty-output scroll", SelfTestScrollTtyOutput},
            {"focus", SelfTestFocus},
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
