#include "font_router.h"

#include "diagnostics.h"
#include "emoji_presentation.h"
#include "font_metrics.h"
#include "font_role.h"
#include "unicode_script.h"
#include "vt_font.h"
#include "vt_widgetP.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(XTP_FONT_ROUTE_PRIMARY == 2 * XTP_FONT_ROLE_PRIMARY, "primary route/role layout");
_Static_assert(XTP_FONT_ROUTE_WIDE == 2 * XTP_FONT_ROLE_WIDE, "wide route/role layout");
_Static_assert(XTP_FONT_ROUTE_EMOJI == 2 * XTP_FONT_ROLE_EMOJI, "emoji route/role layout");
_Static_assert(XTP_FONT_ROUTE_HAN == 2 * XTP_FONT_ROLE_HAN, "Han route/role layout");
_Static_assert(XTP_FONT_ROUTE_PRIMARY_FALLBACK == XTP_FONT_ROUTE_PRIMARY + 1,
               "primary fallback route layout");
_Static_assert(XTP_FONT_ROUTE_WIDE_FALLBACK == XTP_FONT_ROUTE_WIDE + 1,
               "wide fallback route layout");
_Static_assert(XTP_FONT_ROUTE_EMOJI_FALLBACK == XTP_FONT_ROUTE_EMOJI + 1,
               "emoji fallback route layout");
_Static_assert(XTP_FONT_ROUTE_HAN_FALLBACK == XTP_FONT_ROUTE_HAN + 1, "Han fallback route layout");
_Static_assert((int)XTP_FONT_CAPTURE_PRIMARY == (int)XTP_FONT_ROLE_PRIMARY,
               "primary capture/role layout");
_Static_assert((int)XTP_FONT_CAPTURE_WIDE == (int)XTP_FONT_ROLE_WIDE, "wide capture/role layout");
_Static_assert((int)XTP_FONT_CAPTURE_EMOJI == (int)XTP_FONT_ROLE_EMOJI,
               "emoji capture/role layout");
_Static_assert((int)XTP_FONT_CAPTURE_HAN == (int)XTP_FONT_ROLE_HAN, "Han capture/role layout");

static Boolean
ExactHanVariationSupported(XftFont *font, const char *text, size_t length)
{
        uint32_t base;
        uint32_t selector = 0;
        size_t consumed;
        size_t offset;
        FT_Face face;
        FT_UInt glyph;

        if (!XtpUtf8Decode(text, length, &base, &consumed) || !XtpUnicodeScriptHan(base))
                return True;
        offset = consumed;
        while (offset < length) {
                uint32_t codepoint;

                if (!XtpUtf8Decode(text + offset, length - offset, &codepoint, &consumed))
                        return False;
                if ((codepoint >= 0xfe00U && codepoint <= 0xfe0fU) ||
                    (codepoint >= 0xe0100U && codepoint <= 0xe01efU)) {
                        selector = codepoint;
                        break;
                }
                offset += consumed;
        }
        if (selector == 0)
                return True;
        face = font != NULL ? XftLockFace(font) : NULL;
        if (face == NULL)
                return False;
        glyph = FT_Face_GetCharVariantIndex(face, base, selector);
        XftUnlockFace(font);
        return glyph != 0;
}

static XftFont *
FinishMissingCluster(Vt100Rec *vt, XftFont *blank_font, const char *text, size_t length,
                     const char **role_name, XtpGlyphRun *run)
{
        if (XtpUnicodeClusterRequiresInk(text, length)) {
                if (role_name != NULL)
                        *role_name = "tofu";
                if (run != NULL) {
                        memset(run, 0, sizeof(*run));
                        run->missing = true;
                }
                return NULL;
        }
        if (run != NULL && blank_font != NULL)
                (void)XtpShapeUtf8(vt->vt.font_universe->shaper, blank_font, text, length, run);
        return blank_font;
}

static Boolean
FontHasCluster(Vt100Rec *vt, XftFont *font, int slot, const char *text, size_t length,
               unsigned int width, Boolean color_glyphs, Boolean requires_composition,
               XtpGlyphRun *run, XtpFontRouteMissCode *miss_out)
{
        FT_UInt glyphs[XTP_GLYPH_RUN_CAPACITY];
        XGlyphInfo extents = {0};
        size_t offset = 0;
        size_t index;

        if (miss_out != NULL)
                *miss_out = XTP_FONT_MISS_SHAPE;
        if (font == NULL || length == 0 || length >= XTP_VISUAL_TEXT_CAPACITY) {
                if (miss_out != NULL)
                        *miss_out = XTP_FONT_MISS_CMAP;
                return False;
        }
        if (!ExactHanVariationSupported(font, text, length)) {
                if (miss_out != NULL)
                        *miss_out = XTP_FONT_MISS_UVS;
                return False;
        }
        while (offset < length) {
                uint32_t codepoint;
                size_t consumed;

                if (!XtpUtf8Decode(text + offset, length - offset, &codepoint, &consumed))
                        return False;
                if (!XtpUnicodeSequenceControl(codepoint) &&
                    !XftCharExists(XtDisplay((Widget)vt), font, codepoint)) {
                        if (miss_out != NULL)
                                *miss_out = XTP_FONT_MISS_CMAP;
                        return False;
                }
                offset += consumed;
        }
        if (!(requires_composition
                  ? XtpShapeUtf8ForComposition(vt->vt.font_universe->shaper, font, text, length,
                                               run)
                  : XtpShapeUtf8(vt->vt.font_universe->shaper, font, text, length, run)) ||
            run->missing || (requires_composition && run->count != 1U))
                return False;
        if (!XtpUnicodeClusterRequiresInk(text, length))
                return True;
        if (XtpCairoFontIsColor(font)) {
                Boolean has_ink;
                GlyphInkCacheEntry *entry;

                for (index = 0; index < XTP_GLYPH_INK_CACHE_SIZE; ++index) {
                        entry = &vt->vt.font_universe->glyph_ink_cache[index];
                        if (entry->font == font && entry->text_length == length &&
                            memcmp(entry->text, text, length) == 0 && entry->width == width &&
                            entry->color_glyphs == color_glyphs) {
                                if (!entry->has_ink && miss_out != NULL)
                                        *miss_out = XTP_FONT_MISS_INK;
                                return entry->has_ink;
                        }
                }
                has_ink =
                    VtFontEnsureCairoDraw(vt) &&
                    XtpCairoGlyphRunHasInk(vt->vt.font_universe->cairo, font, run, color_glyphs,
                                           width * VtSlotWidth(vt, slot), VtSlotHeight(vt, slot));
                entry = &vt->vt.font_universe
                             ->glyph_ink_cache[vt->vt.font_universe->next_glyph_ink_cache];
                entry->font = font;
                memcpy(entry->text, text, length);
                entry->text_length = (uint8_t)length;
                entry->width = (uint8_t)width;
                entry->color_glyphs = color_glyphs;
                entry->has_ink = has_ink;
                vt->vt.font_universe->next_glyph_ink_cache =
                    (vt->vt.font_universe->next_glyph_ink_cache + 1U) % XTP_GLYPH_INK_CACHE_SIZE;
                if (!has_ink && miss_out != NULL)
                        *miss_out = XTP_FONT_MISS_INK;
                return has_ink;
        }
        for (index = 0; index < run->count; ++index)
                glyphs[index] = (FT_UInt)run->glyphs[index].index;
        XftGlyphExtents(XtDisplay((Widget)vt), font, glyphs, (int)run->count, &extents);
        if (extents.width == 0 || extents.height == 0) {
                if (miss_out != NULL)
                        *miss_out = XTP_FONT_MISS_INK;
                return False;
        }
        return True;
}

static double
GlyphRunAdvance(const XtpGlyphRun *run)
{
        double advance = 0.0;
        unsigned int index;

        if (run == NULL)
                return 0.0;
        for (index = 0; index < run->count; ++index)
                advance += (double)run->glyphs[index].x_advance * run->x_pixel_scale;
        return advance;
}

static XftFont *
RoleFontWithCluster(Vt100Rec *vt, XftFont *normal, int slot, const char *text, size_t length,
                    unsigned int width, Boolean color_glyphs, Boolean requires_composition,
                    XtpGlyphRun *run, XtpFontRouteMissCode *miss_out)
{
        if (!FontHasCluster(vt, normal, slot, text, length, width, color_glyphs,
                            requires_composition, run, miss_out))
                return NULL;
        return normal;
}

static XftFont *
OpenFallbackCandidate(Vt100Rec *vt, XtpXftFallbackCandidate *candidate, int slot)
{
        FcPattern *pattern;

        if (!candidate->attempted) {
                pattern =
                    candidate->pattern != NULL ? FcPatternDuplicate(candidate->pattern) : NULL;
                candidate->attempted = True;
                candidate->font = VtOpenNormalizedXftPattern(vt, pattern, slot, NULL);
        }
        return candidate->font;
}

static XftFont *
FallbackStyleRangeWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, XftFont *normal,
                              Boolean bold, Boolean italic, const char *text, size_t length,
                              unsigned int width, Boolean color_glyphs,
                              Boolean requires_composition, XtpGlyphRun *run, uint8_t first,
                              uint8_t limit)
{
        unsigned int style = XtpFontStyleIndex(bold, italic);
        uint8_t index;

        if (limit > fallbacks->counts[slot][style])
                limit = fallbacks->counts[slot][style];
        for (index = first; index < limit; ++index) {
                XtpXftFallbackCandidate *candidate = &fallbacks->candidates[slot][style][index];
                XftFont *font;

                if (!XtpFontSameFamily(normal->pattern, candidate->pattern))
                        continue;
                font = OpenFallbackCandidate(vt, candidate, slot);
                if (XtpFontStyleIsReal(normal, font, bold, italic) &&
                    FontHasCluster(vt, font, slot, text, length, width, color_glyphs,
                                   requires_composition, run, NULL))
                        return font;
        }
        return NULL;
}

static XftFont *
FallbackStyleWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, XftFont *normal,
                         Boolean bold, Boolean italic, const char *text, size_t length,
                         unsigned int width, Boolean color_glyphs, Boolean requires_composition,
                         XtpGlyphRun *run)
{
        unsigned int style = XtpFontStyleIndex(bold, italic);
        XftFont *font;

        if (style == 0 || normal == NULL)
                return normal;
        font = FallbackStyleRangeWithCluster(vt, fallbacks, slot, normal, bold, italic, text,
                                             length, width, color_glyphs, requires_composition, run,
                                             0, fallbacks->named_counts[slot][style]);
        if (font != NULL)
                return font;
        VtFontEnsureSystemFallbacks(vt, fallbacks, slot, style);
        font = FallbackStyleRangeWithCluster(vt, fallbacks, slot, normal, bold, italic, text,
                                             length, width, color_glyphs, requires_composition, run,
                                             fallbacks->named_counts[slot][style],
                                             fallbacks->counts[slot][style]);
        return font != NULL ? font : normal;
}

static XftFont *
FallbackFontRangeWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, uint8_t first,
                             uint8_t limit, const char *text, size_t length, unsigned int width,
                             Boolean color_glyphs, Boolean requires_composition, XtpGlyphRun *run,
                             XtpFontRouteTrace *trace, XtpFontRouteRung *rung_out,
                             uint8_t *named_out)
{
        const unsigned int style = 0;
        uint8_t fallback;

        if (limit > fallbacks->counts[slot][style])
                limit = fallbacks->counts[slot][style];
        for (fallback = first; fallback < limit; ++fallback) {
                XtpXftFallbackCandidate *candidate = &fallbacks->candidates[slot][style][fallback];
                XftFont *font;
                XtpFontRouteRung rung;
                XtpFontRouteMissCode miss;

                if (fallback < fallbacks->explicit_counts[slot][style])
                        rung = XTP_FONT_RUNG_ENTRY2;
                else if (candidate->named_index != 0)
                        rung = XTP_FONT_RUNG_NAMED;
                else
                        rung = XTP_FONT_RUNG_SYSTEM;

                if (!candidate->activated &&
                    fallbacks->activated_counts[slot][style] >=
                        (unsigned int)vt->vt.font_universe->limit_fontsets) {
                        XtpFontRouteTraceAdd(trace, rung, candidate->named_index,
                                             XTP_FONT_MISS_BUDGET);
                        continue;
                }
                font = OpenFallbackCandidate(vt, candidate, slot);

                if (FontHasCluster(vt, font, slot, text, length, width, color_glyphs,
                                   requires_composition, run, &miss)) {
                        double advance;

                        if (!candidate->activated) {
                                candidate->activated = True;
                                ++fallbacks->activated_counts[slot][style];
                                XtpLog(
                                    XTP_LOG_INFO, "font",
                                    "activated Xft fallback slot=%d style=%u entry=%u source=%s%u "
                                    "budget=%u/%d",
                                    slot, style, (unsigned int)fallback + 1U,
                                    candidate->named_index != 0 ? "fallbackFace" : "chain",
                                    candidate->named_index,
                                    fallbacks->activated_counts[slot][style],
                                    vt->vt.font_universe->limit_fontsets);
                        }
                        advance = GlyphRunAdvance(run);
                        if (!XtpFontFallbackAdvanceFits(advance, VtSlotWidth(vt, slot), width,
                                                        vt->vt.font_universe->limit_fontwidth)) {
                                XtpLog(XTP_LOG_DEBUG, "font",
                                       "deferred Xft fallback slot=%d entry=%u advance=%.3f "
                                       "cell=%u width=%u limit=%d",
                                       slot, (unsigned int)fallback + 1U, advance,
                                       VtSlotWidth(vt, slot), width,
                                       vt->vt.font_universe->limit_fontwidth);
                                XtpFontRouteTraceAdd(trace, rung, candidate->named_index,
                                                     XTP_FONT_MISS_SHAPE);
                                continue;
                        }
                        if (rung_out != NULL)
                                *rung_out = rung;
                        if (named_out != NULL)
                                *named_out = candidate->named_index;
                        return font;
                }
                XtpFontRouteTraceAdd(trace, rung, candidate->named_index, miss);
        }
        return NULL;
}

static XftFont *
ExplicitFallbackWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, const char *text,
                            size_t length, unsigned int width, Boolean color_glyphs,
                            Boolean requires_composition, XtpGlyphRun *run,
                            XtpFontRouteTrace *trace, XtpFontRouteRung *rung_out,
                            uint8_t *named_out)
{
        return FallbackFontRangeWithCluster(
            vt, fallbacks, slot, 0, fallbacks->explicit_counts[slot][0], text, length, width,
            color_glyphs, requires_composition, run, trace, rung_out, named_out);
}

static XftFont *
SystemFallbackWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, const char *text,
                          size_t length, unsigned int width, Boolean color_glyphs,
                          Boolean requires_composition, XtpGlyphRun *run, XtpFontRouteTrace *trace,
                          XtpFontRouteRung *rung_out, uint8_t *named_out)
{
        VtFontEnsureSystemFallbacks(vt, fallbacks, slot, XTP_XFT_STYLE_NORMAL);
        return FallbackFontRangeWithCluster(
            vt, fallbacks, slot, fallbacks->named_counts[slot][0], fallbacks->counts[slot][0], text,
            length, width, color_glyphs, requires_composition, run, trace, rung_out, named_out);
}

static XftFont *
NamedFallbackWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, const char *text,
                         size_t length, unsigned int width, Boolean color_glyphs,
                         Boolean requires_composition, XtpGlyphRun *run, XtpFontRouteTrace *trace,
                         XtpFontRouteRung *rung_out, uint8_t *named_out)
{
        return FallbackFontRangeWithCluster(
            vt, fallbacks, slot, fallbacks->explicit_counts[slot][0],
            fallbacks->named_counts[slot][0], text, length, width, color_glyphs,
            requires_composition, run, trace, rung_out, named_out);
}

static XftFont *
AllFallbacksWithCluster(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot, const char *text,
                        size_t length, unsigned int width, Boolean color_glyphs,
                        Boolean requires_composition, XtpGlyphRun *run, XtpFontRouteTrace *trace,
                        XtpFontRouteRung *rung_out, uint8_t *named_out)
{
        XftFont *font;

        font = FallbackFontRangeWithCluster(
            vt, fallbacks, slot, 0, fallbacks->named_counts[slot][0], text, length, width,
            color_glyphs, requires_composition, run, trace, rung_out, named_out);
        if (font != NULL)
                return font;
        VtFontEnsureSystemFallbacks(vt, fallbacks, slot, XTP_XFT_STYLE_NORMAL);
        return FallbackFontRangeWithCluster(
            vt, fallbacks, slot, fallbacks->named_counts[slot][0], fallbacks->counts[slot][0], text,
            length, width, color_glyphs, requires_composition, run, trace, rung_out, named_out);
}

static const char *
FontRouteName(XtpFontRouteKind kind)
{
        switch (kind) {
        case XTP_FONT_ROUTE_PRIMARY:
                return "primary";
        case XTP_FONT_ROUTE_PRIMARY_FALLBACK:
                return "fallback";
        case XTP_FONT_ROUTE_WIDE:
                return "doublesize";
        case XTP_FONT_ROUTE_WIDE_FALLBACK:
                return "doublesize-fallback";
        case XTP_FONT_ROUTE_EMOJI:
                return "emoji";
        case XTP_FONT_ROUTE_EMOJI_FALLBACK:
                return "emoji-fallback";
        case XTP_FONT_ROUTE_HAN:
                return "han";
        case XTP_FONT_ROUTE_HAN_FALLBACK:
                return "han-fallback";
        case XTP_FONT_ROUTE_TOFU:
                return "tofu";
        }
        return "primary-missing";
}

static XtpFontRouteKind
CapturedRouteKind(XtpFontCaptureSlot slot)
{
        return (XtpFontRouteKind)(2U * (unsigned int)slot);
}

static XftFont *
CapturedNormalFont(Vt100Rec *vt, XtpFontCaptureSlot capture, int slot)
{
        return vt->vt.font_universe->roles[(XtpFontRoleIndex)capture]
            .fonts[XTP_XFT_STYLE_NORMAL][slot];
}

static XtpXftFallbackSet *
FontRouteFallbacks(Vt100Rec *vt, XtpFontRouteKind kind)
{
        if (kind >= XTP_FONT_ROUTE_TOFU || ((unsigned int)kind & 1U) == 0)
                return NULL;
        return &vt->vt.font_universe->roles[(XtpFontRoleIndex)((unsigned int)kind / 2U)].fallbacks;
}

static XftFont *
FontRouteStyle(Vt100Rec *vt, XtpFontRouteKind kind, XftFont *normal, int slot, Boolean bold,
               Boolean italic, const char *text, size_t length, unsigned int width,
               Boolean color_glyphs, Boolean requires_composition, const XtpGlyphRun *normal_run,
               XtpGlyphRun *run, Boolean *style_fallback_out)
{
        XtpXftFallbackSet *fallbacks = FontRouteFallbacks(vt, kind);
        XftFont *font = normal;

        if (run != NULL)
                *run = *normal_run;
        if (style_fallback_out != NULL)
                *style_fallback_out = False;
        if (!bold && !italic)
                return normal;
        if (fallbacks != NULL) {
                font = FallbackStyleWithCluster(vt, fallbacks, slot, normal, bold, italic, text,
                                                length, width, color_glyphs, requires_composition,
                                                run);
                if (font == normal) {
                        if (run != NULL)
                                *run = *normal_run;
                        if (style_fallback_out != NULL)
                                *style_fallback_out = True;
                }
                return font;
        }
        if (kind >= XTP_FONT_ROUTE_TOFU)
                return normal;
        font = VtFontRoleStyle(vt, (XtpFontRoleIndex)((unsigned int)kind / 2U), slot, bold, italic);
        if (font != normal && XtpFontStyleIsReal(normal, font, bold, italic) &&
            FontHasCluster(vt, font, slot, text, length, width, color_glyphs, requires_composition,
                           run, NULL))
                return font;
        if (run != NULL)
                *run = *normal_run;
        if (style_fallback_out != NULL)
                *style_fallback_out = True;
        return normal;
}

static Boolean
BuildFontRouteKey(Vt100Rec *vt, const char *text, size_t length, unsigned int width,
                  XtpEmojiStyle presentation, XtpFontCaptureSlot capturing_slot,
                  XtpFontRouteKey *key)
{
        if (key == NULL || length == 0 || length >= XTP_FONT_ROUTE_TEXT_CAPACITY ||
            width > UINT8_MAX)
                return False;
        memset(key, 0, sizeof(*key));
        memcpy(key->text, text, length);
        key->text_length = (uint8_t)length;
        key->width = (uint8_t)width;
        key->presentation = (uint8_t)presentation;
        key->presentation_policy = (uint8_t)vt->vt.font_universe->emoji_presentation;
        key->slot = (uint8_t)vt->vt.current_font;
        key->capturing_slot = capturing_slot;
        key->color_glyphs = vt->vt.font_universe->color_glyphs;
        key->system_fallback = vt->vt.font_universe->system_fallback;
        key->generation = vt->vt.font_universe->generation;
        return True;
}

typedef struct
{
        Vt100Rec *vt;
        const char *text;
        size_t length;
        unsigned int width;
        Boolean bold;
        Boolean italic;
        Boolean color_glyphs;
        Boolean requires_composition;
        int slot;
        XtpFontRouteKey *key;
        const char **role_name;
        XtpGlyphRun *output_run;
} RouteContext;

static XftFont *
FinishFontRoute(RouteContext *context, Boolean cacheable, XtpFontRouteKind kind,
                XtpFontRouteRung rung, uint8_t named_index, XftFont *normal,
                const XtpGlyphRun *normal_run, const XtpFontRouteTrace *trace)
{
        XtpFontRouteValue value = {kind, rung, named_index, normal};
        Boolean style_fallback = False;
        XftFont *font;

        if (context->role_name != NULL)
                *context->role_name = FontRouteName(kind);
        font = FontRouteStyle(context->vt, kind, normal, context->slot, context->bold,
                              context->italic, context->text, context->length, context->width,
                              context->color_glyphs, context->requires_composition, normal_run,
                              context->output_run, &style_fallback);
        if (cacheable) {
                (void)XtpFontRouteCacheStore(context->vt->vt.font_universe->route_cache,
                                             context->key, value);
                XtpFontRoutingReportRoute(context->vt->vt.font_routing_report, context->key, value,
                                          normal->pattern, trace,
                                          XtpFontStyleName(context->bold, context->italic),
                                          style_fallback != False);
        } else if (style_fallback)
                XtpFontRoutingReportStyleFallback(context->vt->vt.font_routing_report, context->key,
                                                  XtpFontStyleName(context->bold, context->italic));
        return font;
}

static XftFont *
FinishTofuRoute(RouteContext *context, Boolean cacheable, const XtpFontRouteTrace *trace)
{
        XtpFontRouteValue value = {XTP_FONT_ROUTE_TOFU, XTP_FONT_RUNG_TOFU, 0, NULL};

        if (cacheable) {
                (void)XtpFontRouteCacheStore(context->vt->vt.font_universe->route_cache,
                                             context->key, value);
                XtpFontRoutingReportRoute(context->vt->vt.font_routing_report, context->key, value,
                                          NULL, trace, "normal", false);
        }
        return FinishMissingCluster(context->vt, NULL, context->text, context->length,
                                    context->role_name, context->output_run);
}

XftFont *
VtSelectXftFont(Vt100Rec *vt, const char *text, size_t length, unsigned int width, Boolean bold,
                Boolean italic, const char **role_name, uint32_t *base_out,
                XtpEmojiStyle *style_out, XtpGlyphRun *run_out)
{
        int slot = vt->vt.current_font;
        XftFont *primary =
            vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot];
        XtpEmojiClusterStyle cluster =
            XtpEmojiResolveClusterStyle(text, length, vt->vt.font_universe->emoji_presentation);
        uint32_t base = cluster.base;
        XtpEmojiStyle style = cluster.style;
        Boolean emoji_slot_set = vt->vt.font_universe->chains[XTP_FONT_ROLE_EMOJI].count != 0;
        Boolean wide_slot_set = vt->vt.font_universe->chains[XTP_FONT_ROLE_WIDE].count != 0;
        Boolean han_slot_set = vt->vt.font_universe->chains[XTP_FONT_ROLE_HAN].count != 0;
        Boolean color_glyphs;
        Boolean cacheable;
        XtpFontCaptureSlot capturing_slot;
        XtpFontRouteKey key = {0};
        XtpFontRouteValue cached;
        XtpFontRouteTrace trace = {0};
        XtpFontRouteRung route_rung = XTP_FONT_RUNG_ENTRY1;
        XtpFontRouteMissCode route_miss = XTP_FONT_MISS_SHAPE;
        uint8_t named_index = 0;
        XtpGlyphRun route_run = {0};
        XtpGlyphRun *output_run = run_out != NULL ? run_out : &route_run;
        RouteContext context;
        XftFont *font;

        memset(output_run, 0, sizeof(*output_run));
        if (length == 0) {
                if (role_name != NULL)
                        *role_name = "primary";
                if (base_out != NULL)
                        *base_out = 0;
                if (style_out != NULL)
                        *style_out = XTP_EMOJI_STYLE_TEXT;
                return primary;
        }

        if (base_out != NULL)
                *base_out = base;
        if (style_out != NULL)
                *style_out = style;
        color_glyphs = vt->vt.font_universe->color_glyphs && style != XTP_EMOJI_STYLE_TEXT;
        context = (RouteContext){vt,   text,   length,       width,
                                 bold, italic, color_glyphs, cluster.requires_composition,
                                 slot, &key,   role_name,    output_run};
        if (style != XTP_EMOJI_STYLE_EMOJI && han_slot_set && XtpUnicodeScriptHan(base))
                capturing_slot = XTP_FONT_CAPTURE_HAN;
        else if (style == XTP_EMOJI_STYLE_EMOJI && emoji_slot_set)
                capturing_slot = XTP_FONT_CAPTURE_EMOJI;
        else if ((style == XTP_EMOJI_STYLE_EMOJI && wide_slot_set) ||
                 (style != XTP_EMOJI_STYLE_EMOJI && width > 1U && wide_slot_set))
                capturing_slot = XTP_FONT_CAPTURE_WIDE;
        else
                capturing_slot = XTP_FONT_CAPTURE_PRIMARY;
        if (!XtpUnicodeClusterRequiresInk(text, length)) {
                XtpFontRouteKind kind = CapturedRouteKind(capturing_slot);

                font = CapturedNormalFont(vt, capturing_slot, slot);
                if (font == NULL) {
                        kind = XTP_FONT_ROUTE_PRIMARY;
                        font = primary;
                }
                if (role_name != NULL)
                        *role_name = FontRouteName(kind);
                return font;
        }
        cacheable = vt->vt.font_universe->route_cache != NULL &&
                    BuildFontRouteKey(vt, text, length, width, style, capturing_slot, &key);
        if (cacheable &&
            XtpFontRouteCacheLookup(vt->vt.font_universe->route_cache, &key, &cached)) {
                XtpGlyphRun normal_run = {0};

                if (XtpLogEnabled(XTP_LOG_DEBUG))
                        XtpLog(XTP_LOG_DEBUG, "font",
                               "route-cache hit base=U+%04X slot=%d width=%u role=%s", base, slot,
                               width, FontRouteName(cached.kind));
                if (cached.kind == XTP_FONT_ROUTE_TOFU)
                        return FinishTofuRoute(&context, False, NULL);
                font = cached.normal_font;
                if (FontHasCluster(vt, font, slot, text, length, width, color_glyphs,
                                   cluster.requires_composition, &normal_run, NULL))
                        return FinishFontRoute(&context, False, cached.kind, cached.rung,
                                               cached.named_index, font, &normal_run, NULL);
                if (XtpLogEnabled(XTP_LOG_DEBUG))
                        XtpLog(XTP_LOG_DEBUG, "font",
                               "route-cache stale base=U+%04X slot=%d width=%u role=%s", base, slot,
                               width, FontRouteName(cached.kind));
        } else if (cacheable) {
                if (XtpLogEnabled(XTP_LOG_DEBUG))
                        XtpLog(XTP_LOG_DEBUG, "font",
                               "route-cache miss base=U+%04X slot=%d width=%u capture=%u", base,
                               slot, width, (unsigned int)capturing_slot);
        }
        if (style != XTP_EMOJI_STYLE_EMOJI && han_slot_set && XtpUnicodeScriptHan(base)) {
                font = RoleFontWithCluster(vt,
                                           vt->vt.font_universe->roles[XTP_FONT_ROLE_HAN]
                                               .fonts[XTP_XFT_STYLE_NORMAL][slot],
                                           slot, text, length, width, color_glyphs,
                                           cluster.requires_composition, &route_run, &route_miss);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_HAN,
                                               XTP_FONT_RUNG_ENTRY1, 0, font, &route_run, &trace);
                XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_ENTRY1, 0, route_miss);
                font = AllFallbacksWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_HAN].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_HAN_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
        }
        if (style == XTP_EMOJI_STYLE_EMOJI) {
                font = RoleFontWithCluster(vt,
                                           vt->vt.font_universe->roles[XTP_FONT_ROLE_EMOJI]
                                               .fonts[XTP_XFT_STYLE_NORMAL][slot],
                                           slot, text, length, width, color_glyphs,
                                           cluster.requires_composition, &route_run, &route_miss);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_EMOJI,
                                               XTP_FONT_RUNG_ENTRY1, 0, font, &route_run, &trace);
                if (emoji_slot_set)
                        XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_ENTRY1, 0, route_miss);
                font = ExplicitFallbackWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_EMOJI].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_EMOJI_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
                font = RoleFontWithCluster(vt,
                                           vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE]
                                               .fonts[XTP_XFT_STYLE_NORMAL][slot],
                                           slot, text, length, width, color_glyphs,
                                           cluster.requires_composition, &route_run, &route_miss);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_WIDE,
                                               XTP_FONT_RUNG_ENTRY1, 0, font, &route_run, &trace);
                if (wide_slot_set)
                        XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_ENTRY1, 0, route_miss);
                font = AllFallbacksWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_WIDE_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
                font = NamedFallbackWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_EMOJI].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_EMOJI_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
                font = SystemFallbackWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_EMOJI].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_EMOJI_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
                if (emoji_slot_set || wide_slot_set) {
                        if (!vt->vt.font_universe->system_fallback)
                                XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_SYSTEM, 0,
                                                     XTP_FONT_MISS_TRUNCATED);
                        if (XtpUnicodeClusterRequiresInk(text, length))
                                return FinishTofuRoute(&context, cacheable, &trace);
                        font = emoji_slot_set ? vt->vt.font_universe->roles[XTP_FONT_ROLE_EMOJI]
                                                    .fonts[XTP_XFT_STYLE_NORMAL][slot]
                                              : vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE]
                                                    .fonts[XTP_XFT_STYLE_NORMAL][slot];
                        return FinishMissingCluster(vt, font, text, length, role_name, output_run);
                }
        } else if (width > 1U) {
                font = RoleFontWithCluster(vt,
                                           vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE]
                                               .fonts[XTP_XFT_STYLE_NORMAL][slot],
                                           slot, text, length, width, color_glyphs,
                                           cluster.requires_composition, &route_run, &route_miss);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_WIDE,
                                               XTP_FONT_RUNG_ENTRY1, 0, font, &route_run, &trace);
                if (wide_slot_set)
                        XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_ENTRY1, 0, route_miss);
                font = AllFallbacksWithCluster(
                    vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE].fallbacks, slot, text,
                    length, width, color_glyphs, cluster.requires_composition, &route_run, &trace,
                    &route_rung, &named_index);
                if (font != NULL)
                        return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_WIDE_FALLBACK,
                                               route_rung, named_index, font, &route_run, &trace);
                if (wide_slot_set) {
                        if (!vt->vt.font_universe->system_fallback)
                                XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_SYSTEM, 0,
                                                     XTP_FONT_MISS_TRUNCATED);
                        if (XtpUnicodeClusterRequiresInk(text, length))
                                return FinishTofuRoute(&context, cacheable, &trace);
                        return FinishMissingCluster(vt,
                                                    vt->vt.font_universe->roles[XTP_FONT_ROLE_WIDE]
                                                        .fonts[XTP_XFT_STYLE_NORMAL][slot],
                                                    text, length, role_name, output_run);
                }
        }
        font = RoleFontWithCluster(
            vt,
            vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot],
            slot, text, length, width, color_glyphs, cluster.requires_composition, &route_run,
            &route_miss);
        if (font != NULL)
                return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_PRIMARY,
                                       XTP_FONT_RUNG_ENTRY1, 0, font, &route_run, &trace);
        XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_ENTRY1, 0, route_miss);
        font = AllFallbacksWithCluster(
            vt, &vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fallbacks, slot, text, length,
            width, color_glyphs, cluster.requires_composition, &route_run, &trace, &route_rung,
            &named_index);
        if (font != NULL)
                return FinishFontRoute(&context, cacheable, XTP_FONT_ROUTE_PRIMARY_FALLBACK,
                                       route_rung, named_index, font, &route_run, &trace);
        if (XtpUnicodeClusterRequiresInk(text, length)) {
                if (!vt->vt.font_universe->system_fallback)
                        XtpFontRouteTraceAdd(&trace, XTP_FONT_RUNG_SYSTEM, 0,
                                             XTP_FONT_MISS_TRUNCATED);
                return FinishTofuRoute(&context, cacheable, &trace);
        }
        if (role_name != NULL)
                *role_name = "primary-missing";
        return FinishMissingCluster(
            vt,
            vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot],
            text, length, role_name, output_run);
}
