#ifndef XTERM_PLUS_FONT_REPORT_H
#define XTERM_PLUS_FONT_REPORT_H

#include "font_route_cache.h"

#include <fontconfig/fontconfig.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XTP_FONT_REPORT_ROUTE_CAPACITY 4096U
#define XTP_FONT_REPORT_LOAD_INITIAL_CAPACITY 256U
#define XTP_FONT_ROUTE_MISS_CAPACITY 64U

typedef enum
{
        XTP_FONT_MISS_CMAP,
        XTP_FONT_MISS_UVS,
        XTP_FONT_MISS_SHAPE,
        XTP_FONT_MISS_INK,
        XTP_FONT_MISS_BUDGET,
        XTP_FONT_MISS_TRUNCATED,
} XtpFontRouteMissCode;

typedef struct
{
        XtpFontRouteRung rung;
        uint8_t named_index;
        XtpFontRouteMissCode code;
} XtpFontRouteMiss;

typedef struct
{
        XtpFontRouteMiss entries[XTP_FONT_ROUTE_MISS_CAPACITY];
        uint8_t count;
        bool bounded;
} XtpFontRouteTrace;

typedef struct XtpFontRoutingReport XtpFontRoutingReport;

XtpFontRoutingReport *XtpFontRoutingReportCreate(bool enabled);
void XtpFontRoutingReportDestroy(XtpFontRoutingReport *report);
void XtpFontRouteTraceAdd(XtpFontRouteTrace *trace, XtpFontRouteRung rung, uint8_t named_index,
                          XtpFontRouteMissCode code);
void XtpFontRoutingReportLoad(XtpFontRoutingReport *report, const char *slot, int font_slot,
                              const char *style, int entry, const char *configured,
                              const FcPattern *effective, const char *status, uint32_t generation);
void XtpFontRoutingReportRoute(XtpFontRoutingReport *report, const XtpFontRouteKey *key,
                               XtpFontRouteValue value, const FcPattern *pattern,
                               const XtpFontRouteTrace *trace, const char *requested_style,
                               bool style_fallback);
void XtpFontRoutingReportStyleFallback(XtpFontRoutingReport *report, const XtpFontRouteKey *key,
                                       const char *requested_style);
void XtpFontRoutingReportBadPattern(XtpFontRoutingReport *report, const char *resource,
                                    const char *value);
void XtpFontRoutingReportDuplicate(XtpFontRoutingReport *report, const char *kept,
                                   const char *dropped, const FcPattern *signature);
void XtpFontRoutingReportStyleFamily(XtpFontRoutingReport *report, const char *slot,
                                     const char *style, const char *role_family,
                                     const char *resolved_family);
void XtpFontRoutingReportReloadFailure(XtpFontRoutingReport *report, const char *slot,
                                       const char *cause);
void XtpFontRoutingReportMergeBuild(XtpFontRoutingReport *destination,
                                    const XtpFontRoutingReport *source);
size_t XtpFontRoutingReportRouteCount(const XtpFontRoutingReport *report);
bool XtpFontRoutingReportRouteBounded(const XtpFontRoutingReport *report);
size_t XtpFontRoutingReportLoadCount(const XtpFontRoutingReport *report);
bool XtpFontRoutingReportLoadBounded(const XtpFontRoutingReport *report);
void XtpFontRoutingReportSnapshot(const XtpFontRoutingReport *report, uint32_t generation,
                                  double dpi, int limit_fontsets, int limit_fontheight,
                                  int limit_fontwidth, bool system_fallback);

#endif
