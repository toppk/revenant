#ifndef XTERM_PLUS_FONT_UNIVERSE_H
#define XTERM_PLUS_FONT_UNIVERSE_H

#include "emoji_presentation.h"
#include "font_chain.h"
#include "font_metrics.h"
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
/* Ordinary system candidates stored per (slot, style).  This is an inventory
 * bound on enumeration, independent of limitFontsets, which budgets how many
 * glyph-bearing faces may be opened: raising that budget cannot recover a
 * candidate this bound excluded. */
#define XTP_XFT_FALLBACK_CAPACITY 32
/* Reserved beyond that bound for candidates a presentation-aware pass finds,
 * which the ordinary coverage-trimmed sort drops as redundant. */
#define XTP_XFT_PRESENTATION_RESERVE 8
#define XTP_XFT_FALLBACK_SLOTS (XTP_XFT_FALLBACK_CAPACITY + XTP_XFT_PRESENTATION_RESERVE)
#define XTP_GLYPH_INK_CACHE_SIZE 256
#define XTP_VISUAL_TEXT_CAPACITY 64
#define XTP_FITTED_FACE_CACHE_SIZE 64

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
        Boolean presentation_reserved;
        uint8_t named_index;
} XtpXftFallbackCandidate;

typedef struct
{
        XtpXftFallbackCandidate candidates[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT]
                                          [XTP_XFT_FALLBACK_SLOTS];
        FcPattern *system_requests[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* The presentation-aware sort is retained rather than consumed, so a
         * candidate can be chosen for the atom that actually missed instead of
         * from a blind prefix.  Each atom searches it from the start with its own
         * cursor: a candidate that does not cover one atom must stay available to
         * every other atom. */
        FcPattern *monochrome_requests[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        FcFontSet *monochrome_sets[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        uint8_t presentation_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        XftFont *primaries[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        Boolean system_loaded[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        uint8_t counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* [0, explicit_counts) are explicit chain entries. */
        uint8_t explicit_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* [explicit_counts, named_counts) are numbered user fallbacks. */
        uint8_t named_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        uint8_t activated_counts[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
        /* When set, activations count against another role's budget instead of
         * this set's own.  The unnamed emoji role shares the primary role's
         * budget, so automatic color discovery does not hand the default
         * configuration a second allowance of limitFontsets. */
        uint8_t *shared_activations[XTP_FONT_SLOTS][XTP_XFT_STYLE_COUNT];
} XtpXftFallbackSet;

typedef enum
{
        XTP_FONT_ROLE_PRIMARY,
        XTP_FONT_ROLE_WIDE,
        XTP_FONT_ROLE_EMOJI,
        XTP_FONT_ROLE_HAN,
        /* Not a capture slot, so it follows every slot-mapped role. */
        XTP_FONT_ROLE_EMOJI_TEXT,
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
        Boolean fit_emoji_text;
        /* Shrunk faces; see font-resolution(7).  The route and glyph-ink caches
         * hold these pointers, so an entry is never evicted or closed before
         * the universe is destroyed. */
        XtpFittedFaceSlot fitted_faces[XTP_FITTED_FACE_CACHE_SIZE];
        size_t fitted_face_count;
        Boolean fitted_faces_exhausted;
} XtpFontUniverse;

#endif
