#ifndef XTERM_PLUS_FONT_UNIVERSE_H
#define XTERM_PLUS_FONT_UNIVERSE_H

#include "emoji_presentation.h"
#include "font_chain.h"
#include "font_route_cache.h"
#include "glyph_cairo.h"
#include "glyph_shape.h"

#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>

#include <stddef.h>
#include <stdint.h>

#define XTP_FONT_SLOTS 8
#define XTP_XFT_STYLE_COUNT 4
#define XTP_FALLBACK_FACE_COUNT 16
#define XTP_XFT_FALLBACK_CAPACITY 32
#define XTP_GLYPH_INK_CACHE_SIZE 256
#define XTP_VISUAL_TEXT_CAPACITY 64

typedef struct
{
        XftFont *font;
        char text[XTP_VISUAL_TEXT_CAPACITY];
        uint8_t text_length;
        uint8_t width;
        Boolean color_glyphs;
        Boolean has_ink;
} GlyphInkCacheEntry;

typedef struct
{
        FcPattern *pattern;
        XftFont *font;
        Boolean attempted;
        Boolean activated;
        uint8_t named_index;
} XtpXftFallbackCandidate;

typedef struct
{
        XtpXftFallbackCandidate candidates[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT]
                                          [XTP_XFT_FALLBACK_CAPACITY];
        FcPattern *system_requests[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        XftFont *primaries[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        Boolean system_loaded[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        uint8_t counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* [0, explicit_counts) are explicit chain entries. */
        uint8_t explicit_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* [explicit_counts, named_counts) are numbered user fallbacks. */
        uint8_t named_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        uint8_t activated_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
} XtpXftFallbackSet;

typedef enum
{
        XTP_FONT_ROLE_PRIMARY,
        XTP_FONT_ROLE_WIDE,
        XTP_FONT_ROLE_EMOJI,
        XTP_FONT_ROLE_HAN,
        XTP_FONT_ROLE_COUNT,
} XtpFontRoleIndex;

typedef enum
{
        XTP_XFT_STYLE_NORMAL,
        XTP_XFT_STYLE_BOLD,
        XTP_XFT_STYLE_ITALIC,
        XTP_XFT_STYLE_BOLD_ITALIC,
} XtpXftStyleIndex;

typedef struct
{
        XftFont *fonts[XTP_XFT_STYLE_COUNT][XTP_FONT_SLOTS];
        XtpXftFallbackSet fallbacks;
} XtpXftRole;

typedef struct
{
        XtpXftRole roles[XTP_FONT_ROLE_COUNT];
        XtpFontChain chains[XTP_FONT_ROLE_COUNT];
        XtpFontChain primary_bold_chain;
        XtpFontChain wide_bold_chain;
        XtpFontRouteCache *route_cache;
        XftDraw *draw;
        XtpCairo *cairo;
        XtpShaper *shaper;
        XtpEmojiPolicy emoji_presentation;
        Boolean color_glyphs;
        Boolean system_fallback;
        int limit_fontsets;
        int limit_fontheight;
        int limit_fontwidth;
        uint32_t generation;
        double base_size;
        unsigned long bitmap_base_area;
        double sizes[XTP_FONT_SLOTS];
        unsigned int cell_widths[XTP_FONT_SLOTS];
        Boolean named_enabled[XTP_FALLBACK_FACE_COUNT];
        FcPattern *named_patterns[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT][XTP_FALLBACK_FACE_COUNT];
        Boolean slot_attempted[XTP_FONT_SLOTS];
        size_t system_sort_count;
        GlyphInkCacheEntry glyph_ink_cache[XTP_GLYPH_INK_CACHE_SIZE];
        size_t next_glyph_ink_cache;
} XtpFontUniverse;

#endif
