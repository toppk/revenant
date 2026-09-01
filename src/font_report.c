#include "font_report.h"
#include "emoji_presentation.h"
#include "unicode_script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
        WARN_CAPACITY = 128,
        TEXT_CAPACITY = 512,
        ATOM_CAPACITY = 384,
        COORDS_CAPACITY = 256,
};

typedef struct
{
        char slot[24];
        int font_slot;
        char style[16];
        int entry;
        char configured[TEXT_CAPACITY];
        bool has_effective;
        char file[TEXT_CAPACITY];
        int index;
        char coords[COORDS_CAPACITY];
        char status[16];
        uint32_t generation;
} LoadRecord;

typedef struct
{
        XtpFontRouteKey key;
        char atom[ATOM_CAPACITY];
        char presentation[8];
        int widthclass;
        int font_slot;
        uint8_t capturing_slot;
        XtpFontRouteRung rung;
        uint8_t named_index;
        bool has_role;
        char file[TEXT_CAPACITY];
        int index;
        char coords[COORDS_CAPACITY];
        XtpFontRouteTrace trace;
        bool style_fallback;
        char requested_style[16];
} RouteRecord;

typedef enum
{
        WARN_BAD_PATTERN,
        WARN_DUPLICATE,
        WARN_STYLE_FAMILY,
        WARN_UVS_MISS,
        WARN_RELOAD_FAILURE,
} WarnKind;

typedef struct
{
        WarnKind kind;
        char first[TEXT_CAPACITY];
        char second[TEXT_CAPACITY];
        char third[TEXT_CAPACITY];
        char fourth[TEXT_CAPACITY];
        int index;
        char coords[COORDS_CAPACITY];
} WarnRecord;

struct XtpFontRoutingReport
{
        bool enabled;
        bool load_bound;
        bool route_bound;
        LoadRecord *loads;
        size_t load_capacity;
        size_t load_count;
        RouteRecord *routes;
        size_t route_count;
        WarnRecord warnings[WARN_CAPACITY];
        size_t warning_count;
};

static void
CopyText(char *destination, size_t capacity, const char *source)
{
        if (capacity == 0)
                return;
        if (source == NULL)
                source = "";
        (void)snprintf(destination, capacity, "%s", source);
}

static void
PatternIdentity(const FcPattern *pattern, bool *present, char *file, size_t file_capacity,
                int *index, char *coords, size_t coords_capacity)
{
        FcChar8 *value = NULL;

        *present =
            pattern != NULL && FcPatternGetString(pattern, FC_FILE, 0, &value) == FcResultMatch;
        CopyText(file, file_capacity, *present ? (const char *)value : "");
        *index = 0;
        if (pattern != NULL)
                (void)FcPatternGetInteger(pattern, FC_INDEX, 0, index);
        value = NULL;
        if (pattern != NULL &&
            FcPatternGetString(pattern, FC_FONT_VARIATIONS, 0, &value) == FcResultMatch)
                CopyText(coords, coords_capacity, (const char *)value);
        else
                CopyText(coords, coords_capacity, "");
}

static void
FormatAtom(const XtpFontRouteKey *key, char *output, size_t capacity)
{
        size_t input = 0;
        size_t used = 0;

        if (capacity == 0)
                return;
        output[0] = '\0';
        while (input < key->text_length && used + 2U < capacity) {
                uint32_t codepoint;
                size_t consumed;
                int amount;

                if (!XtpUtf8Decode(key->text + input, key->text_length - input, &codepoint,
                                   &consumed))
                        break;
                amount = snprintf(output + used, capacity - used, "%s%04X", used != 0 ? " " : "",
                                  codepoint);
                if (amount < 0 || (size_t)amount >= capacity - used)
                        break;
                used += (size_t)amount;
                input += consumed;
        }
}

static const char *
RungName(XtpFontRouteRung rung, uint8_t named_index, char buffer[32])
{
        switch (rung) {
        case XTP_FONT_RUNG_ENTRY1:
                return "entry1";
        case XTP_FONT_RUNG_ENTRY2:
                return "entry2";
        case XTP_FONT_RUNG_NAMED:
                (void)snprintf(buffer, 32, "fallbackFace%u", named_index);
                return buffer;
        case XTP_FONT_RUNG_SYSTEM:
                return "system";
        case XTP_FONT_RUNG_TOFU:
                return "tofu";
        }
        return "tofu";
}

static const char *
SlotName(uint8_t slot)
{
        switch (slot) {
        case 0:
                return "primary";
        case 1:
                return "doublesize";
        case 2:
                return "emoji";
        case 3:
                return "han";
        default:
                return "unknown";
        }
}

static const char *
MissName(XtpFontRouteMissCode code)
{
        switch (code) {
        case XTP_FONT_MISS_CMAP:
                return "cmap";
        case XTP_FONT_MISS_UVS:
                return "uvs";
        case XTP_FONT_MISS_SHAPE:
                return "shape";
        case XTP_FONT_MISS_INK:
                return "ink";
        case XTP_FONT_MISS_BUDGET:
                return "budget";
        case XTP_FONT_MISS_TRUNCATED:
                return "truncated";
        }
        return "shape";
}

static void
JsonString(FILE *stream, const char *text)
{
        const unsigned char *next = (const unsigned char *)(text != NULL ? text : "");

        fputc('"', stream);
        while (*next != '\0') {
                switch (*next) {
                case '"':
                        fputs("\\\"", stream);
                        break;
                case '\\':
                        fputs("\\\\", stream);
                        break;
                case '\b':
                        fputs("\\b", stream);
                        break;
                case '\f':
                        fputs("\\f", stream);
                        break;
                case '\n':
                        fputs("\\n", stream);
                        break;
                case '\r':
                        fputs("\\r", stream);
                        break;
                case '\t':
                        fputs("\\t", stream);
                        break;
                default:
                        if (*next < 0x20U)
                                fprintf(stream, "\\u%04x", *next);
                        else
                                fputc(*next, stream);
                        break;
                }
                ++next;
        }
        fputc('"', stream);
}

static void
JsonCoords(FILE *stream, const char *coords)
{
        char copy[COORDS_CAPACITY];
        char *save = NULL;
        char *field;
        bool first = true;

        fputc('{', stream);
        CopyText(copy, sizeof(copy), coords);
        for (field = strtok_r(copy, ",", &save); field != NULL;
             field = strtok_r(NULL, ",", &save)) {
                char *equals = strchr(field, '=');
                char *end = NULL;
                double value;

                if (equals == NULL)
                        continue;
                *equals = '\0';
                value = strtod(equals + 1, &end);
                if (end == equals + 1 || *end != '\0')
                        continue;
                if (!first)
                        fputc(',', stream);
                JsonString(stream, field);
                fprintf(stream, ":%.17g", value);
                first = false;
        }
        fputc('}', stream);
}

static void
JsonRole(FILE *stream, bool present, const char *file, int index, const char *coords)
{
        if (!present) {
                fputs("null", stream);
                return;
        }
        fputs("{\"file\":", stream);
        JsonString(stream, file);
        fprintf(stream, ",\"index\":%d,\"coords\":", index);
        JsonCoords(stream, coords);
        fputc('}', stream);
}

XtpFontRoutingReport *
XtpFontRoutingReportCreate(bool enabled)
{
        XtpFontRoutingReport *report = calloc(1, sizeof(*report));

        if (report == NULL)
                return NULL;
        report->enabled = enabled;
        if (enabled) {
                report->loads =
                    calloc(XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY, sizeof(*report->loads));
                report->routes = calloc(XTP_FONT_REPORT_ROUTE_CAPACITY, sizeof(*report->routes));
                if (report->loads == NULL || report->routes == NULL) {
                        free(report->loads);
                        free(report->routes);
                        free(report);
                        return NULL;
                }
                report->load_capacity = XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY;
        }
        return report;
}

void
XtpFontRoutingReportDestroy(XtpFontRoutingReport *report)
{
        if (report == NULL)
                return;
        free(report->loads);
        free(report->routes);
        free(report);
}

static bool
EnsureLoadCapacity(XtpFontRoutingReport *report, size_t needed)
{
        LoadRecord *loads;
        size_t capacity;

        if (needed <= report->load_capacity)
                return true;
        capacity = report->load_capacity;
        while (capacity < needed) {
                if (capacity > SIZE_MAX / 2U || capacity > SIZE_MAX / sizeof(*loads)) {
                        report->load_bound = true;
                        return false;
                }
                capacity *= 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*loads)) {
                report->load_bound = true;
                return false;
        }
        loads = realloc(report->loads, capacity * sizeof(*loads));
        if (loads == NULL) {
                report->load_bound = true;
                return false;
        }
        report->loads = loads;
        report->load_capacity = capacity;
        return true;
}

void
XtpFontRouteTraceAdd(XtpFontRouteTrace *trace, XtpFontRouteRung rung, uint8_t named_index,
                     XtpFontRouteMissCode code)
{
        XtpFontRouteMiss *miss;

        if (trace == NULL)
                return;
        if (trace->count >= XTP_FONT_ROUTE_MISS_CAPACITY) {
                trace->bounded = true;
                return;
        }
        miss = &trace->entries[trace->count++];
        miss->rung = rung;
        miss->named_index = named_index;
        miss->code = code;
}

void
XtpFontRoutingReportLoad(XtpFontRoutingReport *report, const char *slot, int font_slot,
                         const char *style, int entry, const char *configured,
                         const FcPattern *effective, const char *status, uint32_t generation)
{
        LoadRecord *record;

        if (report == NULL || !report->enabled ||
            !EnsureLoadCapacity(report, report->load_count + 1U))
                return;
        record = &report->loads[report->load_count++];
        CopyText(record->slot, sizeof(record->slot), slot);
        record->font_slot = font_slot;
        CopyText(record->style, sizeof(record->style), style);
        record->entry = entry;
        CopyText(record->configured, sizeof(record->configured), configured);
        PatternIdentity(effective, &record->has_effective, record->file, sizeof(record->file),
                        &record->index, record->coords, sizeof(record->coords));
        CopyText(record->status, sizeof(record->status), status);
        record->generation = generation;
}

static void
AddUvsWarning(XtpFontRoutingReport *report, const char *atom)
{
        WarnRecord *warning;

        if (report->warning_count >= WARN_CAPACITY)
                return;
        warning = &report->warnings[report->warning_count++];
        warning->kind = WARN_UVS_MISS;
        CopyText(warning->first, sizeof(warning->first), atom);
}

void
XtpFontRoutingReportRoute(XtpFontRoutingReport *report, const XtpFontRouteKey *key,
                          XtpFontRouteValue value, const FcPattern *pattern,
                          const XtpFontRouteTrace *trace, const char *requested_style,
                          bool style_fallback)
{
        RouteRecord *record;
        size_t index;
        bool saw_uvs = false;

        if (report == NULL || !report->enabled)
                return;
        if (report->route_count >= XTP_FONT_REPORT_ROUTE_CAPACITY) {
                report->route_bound = true;
                return;
        }
        record = &report->routes[report->route_count++];
        record->key = *key;
        FormatAtom(key, record->atom, sizeof(record->atom));
        CopyText(record->presentation, sizeof(record->presentation),
                 key->presentation == XTP_EMOJI_STYLE_EMOJI ? "emoji" : "text");
        record->widthclass = key->width;
        record->font_slot = key->slot;
        record->capturing_slot = key->capturing_slot;
        record->rung = value.rung;
        record->named_index = value.named_index;
        PatternIdentity(pattern, &record->has_role, record->file, sizeof(record->file),
                        &record->index, record->coords, sizeof(record->coords));
        if (trace != NULL)
                record->trace = *trace;
        record->style_fallback = style_fallback;
        CopyText(record->requested_style, sizeof(record->requested_style), requested_style);
        for (index = 0; index < record->trace.count; ++index)
                saw_uvs = saw_uvs || record->trace.entries[index].code == XTP_FONT_MISS_UVS;
        if (saw_uvs && value.rung == XTP_FONT_RUNG_TOFU)
                AddUvsWarning(report, record->atom);
}

void
XtpFontRoutingReportStyleFallback(XtpFontRoutingReport *report, const XtpFontRouteKey *key,
                                  const char *requested_style)
{
        size_t index;

        if (report == NULL || key == NULL || !report->enabled || key->text_length == 0)
                return;
        for (index = 0; index < report->route_count; ++index) {
                RouteRecord *record = &report->routes[index];

                if (!XtpFontRouteKeysEqual(&record->key, key))
                        continue;
                record->style_fallback = true;
                CopyText(record->requested_style, sizeof(record->requested_style), requested_style);
                return;
        }
}

void
XtpFontRoutingReportBadPattern(XtpFontRoutingReport *report, const char *resource,
                               const char *value)
{
        WarnRecord *warning;

        if (report == NULL || !report->enabled || report->warning_count >= WARN_CAPACITY)
                return;
        warning = &report->warnings[report->warning_count++];
        warning->kind = WARN_BAD_PATTERN;
        CopyText(warning->first, sizeof(warning->first), resource);
        CopyText(warning->second, sizeof(warning->second), value);
}

void
XtpFontRoutingReportDuplicate(XtpFontRoutingReport *report, const char *kept, const char *dropped,
                              const FcPattern *signature)
{
        WarnRecord *warning;
        bool present;

        if (report == NULL || !report->enabled || report->warning_count >= WARN_CAPACITY)
                return;
        warning = &report->warnings[report->warning_count++];
        warning->kind = WARN_DUPLICATE;
        CopyText(warning->first, sizeof(warning->first), kept);
        CopyText(warning->second, sizeof(warning->second), dropped);
        PatternIdentity(signature, &present, warning->third, sizeof(warning->third),
                        &warning->index, warning->coords, sizeof(warning->coords));
}

void
XtpFontRoutingReportStyleFamily(XtpFontRoutingReport *report, const char *slot, const char *style,
                                const char *role_family, const char *resolved_family)
{
        WarnRecord *warning;
        size_t index;

        if (report == NULL || !report->enabled || report->warning_count >= WARN_CAPACITY)
                return;
        for (index = 0; index < report->warning_count; ++index) {
                warning = &report->warnings[index];
                if (warning->kind == WARN_STYLE_FAMILY && strcmp(warning->first, slot) == 0 &&
                    strcmp(warning->second, style) == 0 &&
                    strcmp(warning->third, role_family) == 0 &&
                    strcmp(warning->fourth, resolved_family) == 0)
                        return;
        }
        warning = &report->warnings[report->warning_count++];
        warning->kind = WARN_STYLE_FAMILY;
        CopyText(warning->first, sizeof(warning->first), slot);
        CopyText(warning->second, sizeof(warning->second), style);
        CopyText(warning->third, sizeof(warning->third), role_family);
        CopyText(warning->fourth, sizeof(warning->fourth), resolved_family);
}

void
XtpFontRoutingReportReloadFailure(XtpFontRoutingReport *report, const char *slot, const char *cause)
{
        WarnRecord *warning;

        if (report == NULL || !report->enabled || report->warning_count >= WARN_CAPACITY)
                return;
        warning = &report->warnings[report->warning_count++];
        warning->kind = WARN_RELOAD_FAILURE;
        CopyText(warning->first, sizeof(warning->first), slot);
        CopyText(warning->second, sizeof(warning->second), cause);
}

void
XtpFontRoutingReportMergeBuild(XtpFontRoutingReport *destination,
                               const XtpFontRoutingReport *source)
{
        size_t count;

        if (destination == NULL || source == NULL || !destination->enabled || !source->enabled)
                return;
        destination->load_count = 0;
        destination->load_bound = source->load_bound;
        count = source->load_count;
        if (!EnsureLoadCapacity(destination, count))
                count = destination->load_capacity;
        memcpy(destination->loads, source->loads, count * sizeof(source->loads[0]));
        destination->load_count = count;
        count = source->warning_count;
        if (count > WARN_CAPACITY - destination->warning_count)
                count = WARN_CAPACITY - destination->warning_count;
        memcpy(destination->warnings + destination->warning_count, source->warnings,
               count * sizeof(source->warnings[0]));
        destination->warning_count += count;
}

size_t
XtpFontRoutingReportRouteCount(const XtpFontRoutingReport *report)
{
        return report != NULL ? report->route_count : 0U;
}

bool
XtpFontRoutingReportRouteBounded(const XtpFontRoutingReport *report)
{
        return report != NULL && report->route_bound;
}

size_t
XtpFontRoutingReportLoadCount(const XtpFontRoutingReport *report)
{
        return report != NULL ? report->load_count : 0U;
}

bool
XtpFontRoutingReportLoadBounded(const XtpFontRoutingReport *report)
{
        return report != NULL && report->load_bound;
}

static void
EmitLoad(FILE *stream, const LoadRecord *record, int limit_fontsets, int limit_fontheight,
         int limit_fontwidth, bool system_fallback)
{
        fputs("{\"schema\":1,\"type\":\"load\",\"slot\":", stream);
        JsonString(stream, record->slot);
        fprintf(stream, ",\"fontslot\":%d,\"style\":", record->font_slot);
        JsonString(stream, record->style);
        fprintf(stream, ",\"entry\":%d,\"configured\":", record->entry);
        JsonString(stream, record->configured);
        fputs(",\"effective\":", stream);
        JsonRole(stream, record->has_effective, record->file, record->index, record->coords);
        fputs(",\"status\":", stream);
        JsonString(stream, record->status);
        fprintf(stream,
                ",\"generation\":%u,\"limits\":{\"fontsets\":%d,\"fontheight\":%d,"
                "\"fontwidth\":%d,\"systemfallback\":%s}}\n",
                record->generation, limit_fontsets, limit_fontheight, limit_fontwidth,
                system_fallback ? "true" : "false");
}

static void
EmitRoute(FILE *stream, const RouteRecord *record)
{
        char rung[32];
        size_t index;

        fputs("{\"schema\":1,\"type\":\"route\",\"atom\":", stream);
        JsonString(stream, record->atom);
        fputs(",\"presentation\":", stream);
        JsonString(stream, record->presentation);
        fprintf(stream, ",\"widthclass\":%d,\"slot\":", record->widthclass);
        JsonString(stream, SlotName(record->capturing_slot));
        fprintf(stream, ",\"fontslot\":%d,\"rung\":", record->font_slot);
        JsonString(stream, RungName(record->rung, record->named_index, rung));
        if (record->has_role) {
                fputs(",\"file\":", stream);
                JsonString(stream, record->file);
                fprintf(stream, ",\"index\":%d,\"coords\":", record->index);
                JsonCoords(stream, record->coords);
        } else {
                fputs(",\"file\":null,\"index\":null,\"coords\":null", stream);
        }
        fputs(",\"misses\":[", stream);
        for (index = 0; index < record->trace.count; ++index) {
                const XtpFontRouteMiss *miss = &record->trace.entries[index];

                if (index != 0)
                        fputc(',', stream);
                fputs("{\"rung\":", stream);
                JsonString(stream, RungName(miss->rung, miss->named_index, rung));
                fputs(",\"code\":", stream);
                JsonString(stream, MissName(miss->code));
                fputc('}', stream);
        }
        fputc(']', stream);
        if (record->trace.bounded)
                fputs(",\"missesTruncated\":true", stream);
        if (record->style_fallback) {
                fputs(",\"styleFallback\":{\"requested\":", stream);
                JsonString(stream, record->requested_style);
                fputs(",\"served\":\"normal\"}", stream);
        }
        fputs("}\n", stream);
}

static void
EmitWarning(FILE *stream, const WarnRecord *warning)
{
        fputs("{\"schema\":1,\"type\":\"warn\",\"code\":", stream);
        switch (warning->kind) {
        case WARN_BAD_PATTERN:
                JsonString(stream, "FR-BADPATTERN");
                fputs(",\"resource\":", stream);
                JsonString(stream, warning->first);
                fputs(",\"value\":", stream);
                JsonString(stream, warning->second);
                break;
        case WARN_DUPLICATE:
                JsonString(stream, "FR-DUPROLE");
                fputs(",\"kept\":", stream);
                JsonString(stream, warning->first);
                fputs(",\"dropped\":", stream);
                JsonString(stream, warning->second);
                fputs(",\"signature\":", stream);
                JsonRole(stream, true, warning->third, warning->index, warning->coords);
                break;
        case WARN_STYLE_FAMILY:
                JsonString(stream, "FR-STYLEFAMILY");
                fputs(",\"slot\":", stream);
                JsonString(stream, warning->first);
                fputs(",\"style\":", stream);
                JsonString(stream, warning->second);
                fputs(",\"roleFamily\":", stream);
                JsonString(stream, warning->third);
                fputs(",\"resolvedFamily\":", stream);
                JsonString(stream, warning->fourth);
                break;
        case WARN_UVS_MISS:
                JsonString(stream, "FR-UVSMISS");
                fputs(",\"atom\":", stream);
                JsonString(stream, warning->first);
                break;
        case WARN_RELOAD_FAILURE:
                JsonString(stream, "FR-RELOADFAIL");
                fputs(",\"slot\":", stream);
                JsonString(stream, warning->first);
                fputs(",\"cause\":", stream);
                JsonString(stream, warning->second);
                break;
        }
        fputs("}\n", stream);
}

void
XtpFontRoutingReportSnapshot(const XtpFontRoutingReport *report, uint32_t generation, double dpi,
                             int limit_fontsets, int limit_fontheight, int limit_fontwidth,
                             bool system_fallback)
{
        size_t index;

        flockfile(stderr);
        if (report == NULL || !report->enabled) {
                fprintf(stderr,
                        "{\"schema\":1,\"type\":\"snapshot\",\"generation\":%u,"
                        "\"dpi\":%.17g,\"collection\":\"disabled\",\"records\":0}\n",
                        generation, dpi);
                fflush(stderr);
                funlockfile(stderr);
                return;
        }
        for (index = 0; index < report->load_count; ++index)
                EmitLoad(stderr, &report->loads[index], limit_fontsets, limit_fontheight,
                         limit_fontwidth, system_fallback);
        for (index = 0; index < report->warning_count; ++index)
                EmitWarning(stderr, &report->warnings[index]);
        for (index = 0; index < report->route_count; ++index)
                EmitRoute(stderr, &report->routes[index]);
        if (report->route_bound)
                fprintf(stderr,
                        "{\"schema\":1,\"type\":\"bound\",\"code\":\"FR-REPORTBOUND\","
                        "\"records\":%zu}\n",
                        report->route_count);
        if (report->load_bound)
                fprintf(stderr,
                        "{\"schema\":1,\"type\":\"bound\",\"code\":\"FR-LOADBOUND\","
                        "\"records\":%zu}\n",
                        report->load_count);
        fprintf(stderr,
                "{\"schema\":1,\"type\":\"snapshot\",\"generation\":%u,\"dpi\":%.17g,"
                "\"collection\":\"enabled\",\"records\":%zu}\n",
                generation, dpi, report->route_count);
        fflush(stderr);
        funlockfile(stderr);
}
