#ifndef XTERM_PLUS_FONT_ROUTE_CACHE_H
#define XTERM_PLUS_FONT_ROUTE_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XTP_FONT_ROUTE_CACHE_CAPACITY 8192U
#define XTP_FONT_ROUTE_TEXT_CAPACITY 64U

typedef enum
{
        XTP_FONT_ROUTE_PRIMARY,
        XTP_FONT_ROUTE_PRIMARY_FALLBACK,
        XTP_FONT_ROUTE_WIDE,
        XTP_FONT_ROUTE_WIDE_FALLBACK,
        XTP_FONT_ROUTE_EMOJI,
        XTP_FONT_ROUTE_EMOJI_FALLBACK,
        XTP_FONT_ROUTE_HAN,
        XTP_FONT_ROUTE_HAN_FALLBACK,
        XTP_FONT_ROUTE_TOFU,
} XtpFontRouteKind;

typedef enum
{
        XTP_FONT_RUNG_ENTRY1,
        XTP_FONT_RUNG_ENTRY2,
        XTP_FONT_RUNG_NAMED,
        XTP_FONT_RUNG_SYSTEM,
        XTP_FONT_RUNG_TOFU,
} XtpFontRouteRung;

typedef struct
{
        char text[XTP_FONT_ROUTE_TEXT_CAPACITY];
        uint8_t text_length;
        uint8_t width;
        uint8_t presentation;
        uint8_t presentation_policy;
        uint8_t slot;
        uint8_t capturing_slot;
        bool color_glyphs;
        bool system_fallback;
        uint32_t generation;
} XtpFontRouteKey;

typedef struct
{
        XtpFontRouteKind kind;
        XtpFontRouteRung rung;
        uint8_t named_index;
        void *normal_font;
} XtpFontRouteValue;

typedef struct XtpFontRouteCache XtpFontRouteCache;

XtpFontRouteCache *XtpFontRouteCacheCreate(size_t capacity);
void XtpFontRouteCacheDestroy(XtpFontRouteCache *cache);
bool XtpFontRouteCacheLookup(XtpFontRouteCache *cache, const XtpFontRouteKey *key,
                             XtpFontRouteValue *value);
bool XtpFontRouteCacheStore(XtpFontRouteCache *cache, const XtpFontRouteKey *key,
                            XtpFontRouteValue value);
size_t XtpFontRouteCacheCount(const XtpFontRouteCache *cache);

#endif
