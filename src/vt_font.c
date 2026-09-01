#include "vt_font.h"

#include "diagnostics.h"
#include "font_chain.h"
#include "font_metrics.h"
#include "font_role.h"

#include <X11/StringDefs.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static Boolean
Nonempty(const char *value)
{
        return value != NULL && *value != '\0';
}

static Boolean
ResourceBoolean(const char *value, Boolean default_value)
{
        if (!Nonempty(value) || strcasecmp(value, "default") == 0)
                return default_value;
        if (strcasecmp(value, "true") == 0 || strcasecmp(value, "on") == 0 ||
            strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0)
                return True;
        if (strcasecmp(value, "false") == 0 || strcasecmp(value, "off") == 0 ||
            strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0 ||
            strcasecmp(value, "defaultOff") == 0)
                return False;
        return default_value;
}

static Boolean
VtFontRequested(const Vt100Rec *vt)
{
        return ResourceBoolean(vt->vt.render_font_name, Nonempty(vt->vt.face_name));
}

static XtpEmojiPolicy
ParseEmojiPolicy(const char *value)
{
        if (value == NULL || strcasecmp(value, "unicode") == 0)
                return XTP_EMOJI_POLICY_UNICODE;
        if (strcasecmp(value, "text") == 0)
                return XTP_EMOJI_POLICY_TEXT;
        if (strcasecmp(value, "emoji") == 0)
                return XTP_EMOJI_POLICY_EMOJI;
        XtpLog(XTP_LOG_ERROR, "config",
               "invalid emojiPresentation=%s; using unicode (expected unicode, text, or emoji)",
               value);
        return XTP_EMOJI_POLICY_UNICODE;
}

static double
PositiveNumber(const char *value, double fallback)
{
        char *end = NULL;
        double number;

        if (!Nonempty(value))
                return fallback;
        number = strtod(value, &end);
        return end != value && number > 0.0 ? number : fallback;
}

static Boolean
TrimFaceSize(char *face, double *size)
{
        char *field;
        char *end;
        char *tail;
        Boolean valid;
        double parsed;

        if (!Nonempty(face))
                return False;
        field = strstr(face, ":size=");
        if (field != NULL)
                ++field;
        else if (strncmp(face, "size=", 5) == 0)
                field = face;
        else
                return False;

        tail = strchr(field, ':');
        if (tail != NULL)
                *tail = '\0';
        parsed = strtod(field + 5, &end);
        valid = end != field + 5 && *end == '\0';
        if (tail != NULL)
                *tail = ':';
        if (!valid)
                return False;

        if (tail != NULL)
                memmove(field, tail + 1, strlen(tail + 1) + 1);
        else if (field == face)
                *field = '\0';
        else
                field[-1] = '\0';
        if (size != NULL)
                *size = parsed;
        return True;
}

unsigned int
VtBitmapFontWidth(const XFontStruct *font)
{
        int width = font->max_bounds.width;

        return width > 0 ? (unsigned int)width : 1U;
}

unsigned int
VtBitmapFontHeight(const XFontStruct *font)
{
        int height = font->ascent + font->descent;

        return height > 0 ? (unsigned int)height : 1U;
}

static unsigned int
XftFontWidth(const XftFont *font)
{
        return font != NULL && font->max_advance_width > 0 ? (unsigned int)font->max_advance_width
                                                           : 1U;
}

static unsigned int
XftFontHeight(const XftFont *font)
{
        int height = font != NULL ? font->ascent + font->descent : 0;

        return height > 0 ? (unsigned int)height : 1U;
}

unsigned int
VtSlotWidth(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft) {
                unsigned int width = vt->vt.font_universe->cell_widths[slot];

                return width != 0 ? width
                                  : XftFontWidth(vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                                     .fonts[XTP_XFT_STYLE_NORMAL][slot]);
        }
        return VtBitmapFontWidth(vt->vt.fonts[slot]);
}

unsigned int
VtSlotHeight(const Vt100Rec *vt, int slot)
{
        if (vt->vt.use_xft)
                return XftFontHeight(vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                         .fonts[XTP_XFT_STYLE_NORMAL][slot]);
        return VtBitmapFontHeight(vt->vt.fonts[slot]);
}

int
VtSlotAscent(const Vt100Rec *vt, int slot)
{
        XftFont *font;

        if (!vt->vt.use_xft)
                return vt->vt.fonts[slot] != NULL ? vt->vt.fonts[slot]->ascent : 0;
        font = vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot];
        return font != NULL ? font->ascent : 0;
}

XftFont *
VtFontRoleStyle(Vt100Rec *vt, XtpFontRoleIndex role, int slot, Boolean bold, Boolean italic)
{
        XtpXftRole *fonts;

        if (vt->vt.font_universe == NULL || role >= XTP_FONT_ROLE_COUNT || slot < 0 ||
            slot >= XTP_FONT_SLOTS)
                return NULL;
        fonts = &vt->vt.font_universe->roles[role];
        return XtpFontRoleSelect(fonts->fonts[XTP_XFT_STYLE_NORMAL][slot],
                                 fonts->fonts[XTP_XFT_STYLE_BOLD][slot],
                                 fonts->fonts[XTP_XFT_STYLE_ITALIC][slot],
                                 fonts->fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot], bold, italic);
}

Boolean
VtFontEnsureXftDraw(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        if (!vt->vt.use_xft)
                return False;
        if (vt->vt.font_universe->draw != NULL)
                return True;
        if (!XtIsRealized(widget))
                return False;
        {
                XWindowAttributes attributes;

                if (!XGetWindowAttributes(XtDisplay(widget), XtWindow(widget), &attributes))
                        return False;
                vt->vt.font_universe->draw = XftDrawCreate(XtDisplay(widget), XtWindow(widget),
                                                           attributes.visual, attributes.colormap);
        }
        if (vt->vt.font_universe->draw == NULL) {
                XtpLog(XTP_LOG_ERROR, "font", "cannot create Xft draw context");
                return False;
        }
        return True;
}

Boolean
VtFontEnsureCairoDraw(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;
        XWindowAttributes attributes;

        if (!vt->vt.use_xft)
                return False;
        if (vt->vt.font_universe->cairo != NULL)
                return True;
        if (!XtIsRealized(widget) ||
            !XGetWindowAttributes(XtDisplay(widget), XtWindow(widget), &attributes))
                return False;
        vt->vt.font_universe->cairo =
            XtpCairoCreate(XtDisplay(widget), XtWindow(widget), attributes.visual, attributes.width,
                           attributes.height);
        if (vt->vt.font_universe->cairo == NULL) {
                XtpLog(XTP_LOG_ERROR, "font", "cannot create Cairo draw context");
                return False;
        }
        return True;
}

static void
LogXftResolved(Vt100Rec *vt, const char *role, int slot, const char *style, int entry,
               const char *request, XftFont *font)
{
        FcChar8 *file = NULL;
        int index = 0;

        if (font == NULL || font->pattern == NULL) {
                XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot, style, entry,
                                         request, NULL, "active", vt->vt.font_universe->generation);
                return;
        }
        (void)FcPatternGetString(font->pattern, FC_FILE, 0, &file);
        (void)FcPatternGetInteger(font->pattern, FC_INDEX, 0, &index);
        XtpLog(XTP_LOG_INFO, "font",
               "resolved Xft role=%s slot=%d style=%s entry=%d request=%s file=%s index=%d", role,
               slot, style, entry, request != NULL ? request : "(unset)",
               file != NULL ? (const char *)file : "(unknown)", index);
        XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot, style, entry, request,
                                 font->pattern, "active", vt->vt.font_universe->generation);
}

static void
SetXftStyle(FcPattern *pattern, Boolean bold, Boolean italic)
{
        FcPatternDel(pattern, FC_WEIGHT);
        FcPatternDel(pattern, FC_SLANT);
        FcPatternAddInteger(pattern, FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
        FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
}

static FcPattern *
BuildXftRequest(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic)
{
        FcPattern *pattern;

        if (!Nonempty(face))
                return NULL;
        pattern = FcNameParse((const FcChar8 *)face);
        if (pattern == NULL)
                return NULL;
        FcPatternDel(pattern, FC_SIZE);
        FcPatternDel(pattern, FC_PIXEL_SIZE);
        FcPatternAddDouble(pattern, FC_SIZE, size);
        SetXftStyle(pattern, bold, italic);
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(XtDisplay((Widget)vt), XScreenNumberOfScreen(XtScreen((Widget)vt)),
                             pattern);
        return pattern;
}

static XftFont *
OpenXftFont(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic)
{
        FcPattern *pattern = BuildXftRequest(vt, face, size, bold, italic);
        FcPattern *match;
        FcResult result;
        XftFont *font;

        if (pattern == NULL)
                return NULL;
        match = FcFontMatch(NULL, pattern, &result);
        FcPatternDestroy(pattern);
        font = match != NULL ? XftFontOpenPattern(XtDisplay((Widget)vt), match) : NULL;
        if (font == NULL && match != NULL)
                FcPatternDestroy(match);
        return font;
}

static FcPattern *
ResolveXftPattern(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic)
{
        FcPattern *pattern;
        FcPattern *match;
        FcResult result;

        pattern = BuildXftRequest(vt, face, size, bold, italic);
        if (pattern == NULL)
                return NULL;
        match = FcFontMatch(NULL, pattern, &result);
        FcPatternDestroy(pattern);
        return match;
}

static Boolean
ScaleXftPatternSize(FcPattern *pattern, double scale)
{
        double point_size;
        double pixel_size;
        Boolean changed = False;

        if (pattern == NULL || !(scale > 0.0))
                return False;
        if (FcPatternGetDouble(pattern, FC_SIZE, 0, &point_size) == FcResultMatch) {
                FcPatternDel(pattern, FC_SIZE);
                changed = FcPatternAddDouble(pattern, FC_SIZE, point_size * scale) || changed;
        }
        if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &pixel_size) == FcResultMatch) {
                FcPatternDel(pattern, FC_PIXEL_SIZE);
                changed = FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixel_size * scale) || changed;
        }
        return changed;
}

XftFont *
VtOpenNormalizedXftPattern(Vt100Rec *vt, FcPattern *pattern, int slot, double *scale_out)
{
        XftFont *primary;
        XftFont *probe;
        XftFont *normalized;
        FcPattern *probe_pattern;
        unsigned int target_height;
        unsigned int source_height;
        double scale = 1.0;

        if (scale_out != NULL)
                *scale_out = 1.0;
        if (pattern == NULL)
                return NULL;
        probe_pattern = FcPatternDuplicate(pattern);
        probe =
            probe_pattern != NULL ? XftFontOpenPattern(XtDisplay((Widget)vt), probe_pattern) : NULL;
        if (probe == NULL) {
                /* Xft consumes a pattern only when opening succeeds. */
                if (probe_pattern != NULL)
                        FcPatternDestroy(probe_pattern);
                FcPatternDestroy(pattern);
                return NULL;
        }
        primary = slot >= 0 && slot < XTP_FONT_SLOTS
                      ? vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                            .fonts[XTP_XFT_STYLE_NORMAL][slot]
                      : NULL;
        target_height = XftFontHeight(primary);
        source_height = XftFontHeight(probe);
        if (primary == NULL || target_height == 0 || source_height == 0 ||
            target_height == source_height) {
                FcPatternDestroy(pattern);
                return probe;
        }
        scale = XtpFontHeightScale(target_height, source_height);
        if (!ScaleXftPatternSize(pattern, scale)) {
                FcPatternDestroy(pattern);
                return probe;
        }
        normalized = XftFontOpenPattern(XtDisplay((Widget)vt), pattern);
        if (normalized == NULL) {
                /* Xft consumes a pattern only when opening succeeds. */
                FcPatternDestroy(pattern);
                return probe;
        }
        XtpLog(XTP_LOG_DEBUG, "font",
               "normalized Xft slot=%d target-height=%u source-height=%u scale=%.9f "
               "result-height=%u",
               slot, target_height, source_height, scale, XftFontHeight(normalized));
        XftFontClose(XtDisplay((Widget)vt), probe);
        if (scale_out != NULL)
                *scale_out = scale;
        return normalized;
}

static XftFont *
OpenNormalizedXftFont(Vt100Rec *vt, const char *face, double size, Boolean bold, Boolean italic,
                      int slot, double *scale_out)
{
        return VtOpenNormalizedXftPattern(vt, ResolveXftPattern(vt, face, size, bold, italic), slot,
                                          scale_out);
}

static void
LoadXftBoldOverride(Vt100Rec *vt, int slot, const char *role, const char *face, double size,
                    XftFont *normal, XftFont **bold_font)
{
        XftFont *font;

        if (!Nonempty(face) || normal == NULL || bold_font == NULL)
                return;
        font = OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
        if (font == NULL) {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft boldFont role=%s slot=%d face=%s", role,
                       slot, face);
                return;
        }
        if (*bold_font != NULL)
                XftFontClose(XtDisplay((Widget)vt), *bold_font);
        *bold_font = font;
        LogXftResolved(vt, role, slot, "bold", 1, face, font);
        if (!XtpFontSameFamily(normal->pattern, font->pattern)) {
                XtpLog(XTP_LOG_WARNING, "font",
                       "FR-STYLEFAMILY slot=%s style=bold roleFamily=%s resolvedFamily=%s", role,
                       XtpFontPatternFamily(normal->pattern), XtpFontPatternFamily(font->pattern));
                XtpFontRoutingReportStyleFamily(vt->vt.font_routing_report, role, "bold",
                                                XtpFontPatternFamily(normal->pattern),
                                                XtpFontPatternFamily(font->pattern));
        }
}

static unsigned int
PackedXftCellWidth(Vt100Rec *vt, XftFont *font)
{
        unsigned int width = 0;
        FcChar32 codepoint;

        if (font == NULL)
                return 1U;
        for (codepoint = 32; codepoint < 256; ++codepoint) {
                XGlyphInfo extents;

                if (!XftCharExists(XtDisplay((Widget)vt), font, codepoint))
                        continue;
                XftTextExtents32(XtDisplay((Widget)vt), font, &codepoint, 1, &extents);
                if (extents.xOff > 0 && (unsigned int)extents.xOff > width &&
                    extents.xOff <= font->max_advance_width)
                        width = (unsigned int)extents.xOff;
        }
        /* Preserve the packed fractional pixel without trusting specialist glyph maxima. */
        if (width != 0 && font->max_advance_width > (int)width &&
            (font->max_advance_width == (int)width + 1 ||
             font->max_advance_width > (int)(2U * width)))
                ++width;
        return width != 0 ? width : XftFontWidth(font);
}

static XftFont *
LoadXftSlot(Vt100Rec *vt, int slot, const char *face, double size, Boolean derive_bold)
{
        if (slot < 0 || slot >= XTP_FONT_SLOTS)
                return NULL;
        if (vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] !=
            NULL)
                return vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                    .fonts[XTP_XFT_STYLE_NORMAL][slot];
        vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] =
            OpenXftFont(vt, face, size, False, False);
        if (vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] !=
            NULL) {
                if (derive_bold)
                        vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                            .fonts[XTP_XFT_STYLE_BOLD][slot] =
                            OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
                vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                    .fonts[XTP_XFT_STYLE_ITALIC][slot] =
                    OpenNormalizedXftFont(vt, face, size, False, True, slot, NULL);
                vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                    .fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot] =
                    OpenNormalizedXftFont(vt, face, size, True, True, slot, NULL);
                vt->vt.font_universe->sizes[slot] = size;
                vt->vt.font_universe->cell_widths[slot] =
                    PackedXftCellWidth(vt, vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                               .fonts[XTP_XFT_STYLE_NORMAL][slot]);
                LogXftResolved(vt, "primary", slot, "normal", 1, face,
                               vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_NORMAL][slot]);
                if (derive_bold)
                        LogXftResolved(vt, "primary", slot, "bold", 1, face,
                                       vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                           .fonts[XTP_XFT_STYLE_BOLD][slot]);
                LogXftResolved(vt, "primary", slot, "italic", 1, face,
                               vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_ITALIC][slot]);
                LogXftResolved(vt, "primary", slot, "bold-italic", 1, face,
                               vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot]);
                XtpLog(XTP_LOG_INFO, "font",
                       "loaded Xft slot=%d face=%s points=%.2f cell=%ux%u ascent=%d "
                       "bold=%s italic=%s bold-italic=%s",
                       slot, face, size, vt->vt.font_universe->cell_widths[slot],
                       XftFontHeight(vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                         .fonts[XTP_XFT_STYLE_NORMAL][slot]),
                       vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                           .fonts[XTP_XFT_STYLE_NORMAL][slot]
                           ->ascent,
                       vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_BOLD][slot] != NULL
                           ? "yes"
                           : "fallback",
                       vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_ITALIC][slot] != NULL
                           ? "yes"
                           : "fallback",
                       vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY]
                                   .fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot] != NULL
                           ? "yes"
                           : "fallback");
        } else {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft slot=%d face=%s points=%.2f", slot,
                       face, size);
                LogXftResolved(vt, "primary", slot, "normal", 1, face, NULL);
        }
        return vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot];
}

static void
LoadXftRoleSlot(Vt100Rec *vt, int slot, const char *role, const char *face, double size,
                Boolean derive_bold, XftFont **fonts, XftFont **bold_fonts, XftFont **italic_fonts,
                XftFont **bold_italic_fonts)
{
        double cell_scale = 1.0;

        if (slot < 0 || slot >= XTP_FONT_SLOTS || !Nonempty(face) ||
            vt->vt.font_universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] ==
                NULL)
                return;
        fonts[slot] = OpenNormalizedXftFont(vt, face, size, False, False, slot, &cell_scale);
        if (fonts[slot] == NULL) {
                XtpLog(XTP_LOG_WARNING, "font", "failed Xft role=%s slot=%d face=%s points=%.2f",
                       role, slot, face, size);
                return;
        }
        if (derive_bold)
                bold_fonts[slot] = OpenNormalizedXftFont(vt, face, size, True, False, slot, NULL);
        italic_fonts[slot] = OpenNormalizedXftFont(vt, face, size, False, True, slot, NULL);
        bold_italic_fonts[slot] = OpenNormalizedXftFont(vt, face, size, True, True, slot, NULL);
        LogXftResolved(vt, role, slot, "normal", 1, face, fonts[slot]);
        if (derive_bold)
                LogXftResolved(vt, role, slot, "bold", 1, face, bold_fonts[slot]);
        LogXftResolved(vt, role, slot, "italic", 1, face, italic_fonts[slot]);
        LogXftResolved(vt, role, slot, "bold-italic", 1, face, bold_italic_fonts[slot]);
        XtpLog(XTP_LOG_INFO, "font",
               "loaded Xft role=%s slot=%d face=%s points=%.2f glyph-box=%ux%u ascent=%d "
               "cell-scale=%.3f bold=%s italic=%s bold-italic=%s",
               role, slot, face, size, XftFontWidth(fonts[slot]), XftFontHeight(fonts[slot]),
               fonts[slot]->ascent, cell_scale, bold_fonts[slot] != NULL ? "yes" : "fallback",
               italic_fonts[slot] != NULL ? "yes" : "fallback",
               bold_italic_fonts[slot] != NULL ? "yes" : "fallback");
}

static Boolean
SameFontPattern(const FcPattern *left, const FcPattern *right)
{
        FcChar8 *left_file;
        FcChar8 *right_file;
        FcChar8 *left_variations = NULL;
        FcChar8 *right_variations = NULL;
        int left_index = 0;
        int right_index = 0;
        FcResult left_variation_result;
        FcResult right_variation_result;

        if (left == NULL || right == NULL ||
            FcPatternGetString(left, FC_FILE, 0, &left_file) != FcResultMatch ||
            FcPatternGetString(right, FC_FILE, 0, &right_file) != FcResultMatch)
                return False;
        (void)FcPatternGetInteger(left, FC_INDEX, 0, &left_index);
        (void)FcPatternGetInteger(right, FC_INDEX, 0, &right_index);
        left_variation_result = FcPatternGetString(left, FC_FONT_VARIATIONS, 0, &left_variations);
        right_variation_result =
            FcPatternGetString(right, FC_FONT_VARIATIONS, 0, &right_variations);
        if (left_index != right_index ||
            strcmp((const char *)left_file, (const char *)right_file) != 0 ||
            (left_variation_result == FcResultMatch) != (right_variation_result == FcResultMatch))
                return False;
        return left_variation_result != FcResultMatch ||
               strcmp((const char *)left_variations, (const char *)right_variations) == 0;
}

static void
ResolveNamedFallbackRoles(Vt100Rec *vt, double size, XtpFontUniverse *universe)
{
        FcPattern *(*roles)[XTP_FALLBACK_FACE_COUNT] = universe->named_patterns[0];
        Boolean *enabled = universe->named_enabled;
        int fallback;

        memset(enabled, 0, sizeof(Boolean) * XTP_FALLBACK_FACE_COUNT);
        for (fallback = 0; fallback < XTP_FALLBACK_FACE_COUNT; ++fallback) {
                const char *face = vt->vt.fallback_face_names[fallback];
                unsigned int style;
                Boolean valid = True;
                int earlier;

                if (!Nonempty(face))
                        continue;
                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        roles[style][fallback] =
                            ResolveXftPattern(vt, face, size, (style & 1U) != 0, (style & 2U) != 0);
                        if (roles[style][fallback] == NULL)
                                valid = False;
                }
                if (!valid) {
                        XtpLog(XTP_LOG_WARNING, "font",
                               "FR-BADPATTERN resource=fallbackFace%d value=%s", fallback + 1,
                               face);
                        {
                                char resource[32];

                                (void)snprintf(resource, sizeof(resource), "fallbackFace%d",
                                               fallback + 1);
                                XtpFontRoutingReportBadPattern(vt->vt.font_routing_report, resource,
                                                               face);
                        }
                        continue;
                }
                for (earlier = 0; earlier < fallback; ++earlier) {
                        Boolean duplicate = enabled[earlier];

                        for (style = 0; duplicate && style < XTP_XFT_STYLE_COUNT; ++style)
                                duplicate =
                                    SameFontPattern(roles[style][fallback], roles[style][earlier]);
                        if (duplicate) {
                                FcChar8 *file = NULL;
                                int index = 0;

                                (void)FcPatternGetString(roles[0][fallback], FC_FILE, 0, &file);
                                (void)FcPatternGetInteger(roles[0][fallback], FC_INDEX, 0, &index);
                                XtpLog(XTP_LOG_WARNING, "font",
                                       "FR-DUPROLE kept=fallbackFace%d dropped=fallbackFace%d "
                                       "file=%s index=%d",
                                       earlier + 1, fallback + 1,
                                       file != NULL ? (const char *)file : "(unknown)", index);
                                {
                                        char kept[32];
                                        char dropped[32];

                                        (void)snprintf(kept, sizeof(kept), "fallbackFace%d",
                                                       earlier + 1);
                                        (void)snprintf(dropped, sizeof(dropped), "fallbackFace%d",
                                                       fallback + 1);
                                        XtpFontRoutingReportDuplicate(vt->vt.font_routing_report,
                                                                      kept, dropped,
                                                                      roles[0][fallback]);
                                }
                                valid = False;
                                break;
                        }
                }
                enabled[fallback] = valid;
        }
}

static void
ResolveNamedFallbackSlot(Vt100Rec *vt, int slot, double size)
{
        XtpFontUniverse *universe = vt->vt.font_universe;
        unsigned int style;

        if (slot == 0)
                return;
        for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                int fallback;

                for (fallback = 0; fallback < XTP_FALLBACK_FACE_COUNT; ++fallback) {
                        if (!universe->named_enabled[fallback])
                                continue;
                        universe->named_patterns[slot][style][fallback] =
                            ResolveXftPattern(vt, vt->vt.fallback_face_names[fallback], size,
                                              (style & 1U) != 0, (style & 2U) != 0);
                }
        }
}

static Boolean
XftFallbackDuplicate(XtpXftFallbackSet *fallbacks, int slot, unsigned int style, XftFont *primary,
                     FcPattern *pattern)
{
        uint8_t fallback;

        if (SameFontPattern(primary->pattern, pattern))
                return True;
        for (fallback = 0; fallback < fallbacks->counts[slot][style]; ++fallback) {
                if (SameFontPattern(fallbacks->candidates[slot][style][fallback].pattern, pattern))
                        return True;
        }
        return False;
}

static Boolean
AppendXftFallback(XtpXftFallbackSet *fallbacks, int slot, unsigned int style, XftFont *primary,
                  FcPattern *pattern, uint8_t named_index)
{
        uint8_t fallback;

        if (pattern == NULL)
                return False;
        if (fallbacks->counts[slot][style] == XTP_XFT_FALLBACK_CAPACITY ||
            XftFallbackDuplicate(fallbacks, slot, style, primary, pattern)) {
                FcPatternDestroy(pattern);
                return False;
        }
        fallback = fallbacks->counts[slot][style]++;
        fallbacks->candidates[slot][style][fallback].pattern = pattern;
        fallbacks->candidates[slot][style][fallback].named_index = named_index;
        return True;
}

static void
LoadXftFallbacks(Vt100Rec *vt, int slot, const char *role, const char *face,
                 const char *explicit_face, double size, Boolean bold, Boolean italic,
                 XftFont *primary, XtpXftFallbackSet *fallbacks,
                 const Boolean named_enabled[XTP_FALLBACK_FACE_COUNT])
{
        unsigned int style = XtpFontStyleIndex(bold, italic);
        FcPattern *request;
        int index;

        if (slot < 0 || slot >= XTP_FONT_SLOTS || !Nonempty(face) || primary == NULL ||
            fallbacks == NULL)
                return;
        if (vt->vt.font_universe->limit_fontsets == 0)
                return;
        fallbacks->primaries[slot][style] = primary;
        if (Nonempty(explicit_face)) {
                FcPattern *pattern = ResolveXftPattern(vt, explicit_face, size, bold, italic);
                uint8_t before = fallbacks->counts[slot][style];

                (void)AppendXftFallback(fallbacks, slot, style, primary, pattern, 0);
                if (fallbacks->counts[slot][style] != before) {
                        XtpLog(XTP_LOG_INFO, "font",
                               "queued Xft explicit fallback role=%s slot=%d style=%u face=%s",
                               role, slot, style, explicit_face);
                        XtpFontRoutingReportLoad(vt->vt.font_routing_report, role, slot,
                                                 XtpFontStyleName(bold, italic), 2, explicit_face,
                                                 fallbacks->candidates[slot][style][before].pattern,
                                                 "active", vt->vt.font_universe->generation);
                }
        }
        fallbacks->explicit_counts[slot][style] = fallbacks->counts[slot][style];
        for (index = 0; index < XTP_FALLBACK_FACE_COUNT; ++index) {
                const char *named_face = vt->vt.fallback_face_names[index];
                FcPattern *pattern;
                uint8_t before;

                if (!named_enabled[index])
                        continue;
                pattern = vt->vt.font_universe->named_patterns[slot][style][index];
                if (pattern == NULL)
                        continue;
                pattern = FcPatternDuplicate(pattern);
                if (pattern == NULL)
                        continue;
                before = fallbacks->counts[slot][style];
                if (!AppendXftFallback(fallbacks, slot, style, primary, pattern,
                                       (uint8_t)(index + 1)))
                        continue;
                XtpLog(XTP_LOG_INFO, "font",
                       "queued Xft named fallback role=%s slot=%d style=%u resource=fallbackFace%d "
                       "entry=%u face=%s",
                       role, slot, style, index + 1, (unsigned int)before + 1U, named_face);
        }
        fallbacks->named_counts[slot][style] = fallbacks->counts[slot][style];
        if (!vt->vt.font_universe->system_fallback)
                return;
        request = BuildXftRequest(vt, face, size, bold, italic);
        if (request == NULL)
                return;
        fallbacks->system_requests[slot][style] = request;
        XtpLog(XTP_LOG_DEBUG, "font", "prepared lazy Xft fallback role=%s slot=%d style=%u", role,
               slot, style);
}

void
VtFontEnsureSystemFallbacks(Vt100Rec *vt, XtpXftFallbackSet *fallbacks, int slot,
                            unsigned int style)
{
        FcPattern *request;
        FcFontSet *set;
        FcResult result;
        int index;

        if (fallbacks == NULL || slot < 0 || slot >= XTP_FONT_SLOTS ||
            style >= XTP_XFT_STYLE_COUNT || fallbacks->system_loaded[slot][style])
                return;
        fallbacks->system_loaded[slot][style] = True;
        request = fallbacks->system_requests[slot][style];
        fallbacks->system_requests[slot][style] = NULL;
        if (request == NULL)
                return;
        ++vt->vt.font_universe->system_sort_count;
        set = FcFontSort(NULL, request, FcTrue, NULL, &result);
        if (set != NULL) {
                for (index = 0; index < set->nfont &&
                                fallbacks->counts[slot][style] < XTP_XFT_FALLBACK_CAPACITY;
                     ++index) {
                        FcPattern *render;

                        if (XftFallbackDuplicate(fallbacks, slot, style,
                                                 fallbacks->primaries[slot][style],
                                                 set->fonts[index]))
                                continue;
                        render = FcFontRenderPrepare(NULL, request, set->fonts[index]);
                        if (render == NULL)
                                continue;
                        (void)AppendXftFallback(fallbacks, slot, style,
                                                fallbacks->primaries[slot][style], render, 0);
                }
                FcFontSetDestroy(set);
        }
        FcPatternDestroy(request);
        XtpLog(XTP_LOG_INFO, "font", "queued lazy Xft system fallback slot=%d style=%u count=%u",
               slot, style, fallbacks->counts[slot][style]);
}

static void
LoadXftRoleFallbacks(Vt100Rec *vt, int slot, const char *role, const char *face,
                     const char *explicit_face, const char *bold_face,
                     const char *bold_explicit_face, double size, XftFont *normal, XftFont *bold,
                     XftFont *italic, XftFont *bold_italic, XtpXftFallbackSet *fallbacks,
                     const Boolean named_enabled[XTP_FALLBACK_FACE_COUNT])
{
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, False, False, normal, fallbacks,
                         named_enabled);
        LoadXftFallbacks(vt, slot, role, Nonempty(bold_face) ? bold_face : face,
                         Nonempty(bold_face) ? bold_explicit_face : explicit_face, size, True,
                         False, bold, fallbacks, named_enabled);
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, False, True, italic, fallbacks,
                         named_enabled);
        LoadXftFallbacks(vt, slot, role, face, explicit_face, size, True, True, bold_italic,
                         fallbacks, named_enabled);
}

static void
CloseFallbackSet(Vt100Rec *vt, XtpXftFallbackSet *set)
{
        int slot;

        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                unsigned int style;

                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        uint8_t fallback;

                        for (fallback = 0; fallback < set->counts[slot][style]; ++fallback) {
                                XtpXftFallbackCandidate *candidate =
                                    &set->candidates[slot][style][fallback];

                                if (candidate->font != NULL)
                                        XftFontClose(XtDisplay((Widget)vt), candidate->font);
                                if (candidate->pattern != NULL)
                                        FcPatternDestroy(candidate->pattern);
                        }
                        if (set->system_requests[slot][style] != NULL)
                                FcPatternDestroy(set->system_requests[slot][style]);
                }
        }
}

void
VtFontUniverseDestroy(Vt100Rec *vt, XtpFontUniverse *universe)
{
        unsigned int role;
        int slot;

        if (universe == NULL)
                return;
        if (universe->draw != NULL)
                XftDrawDestroy(universe->draw);
        XtpCairoDestroy(universe->cairo);
        XtpShaperDestroy(universe->shaper);
        XtpFontRouteCacheDestroy(universe->route_cache);
        for (role = 0; role < XTP_FONT_ROLE_COUNT; ++role) {
                unsigned int style;

                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                                XftFont *font = universe->roles[role].fonts[style][slot];

                                if (font != NULL)
                                        XftFontClose(XtDisplay((Widget)vt), font);
                        }
                }
                CloseFallbackSet(vt, &universe->roles[role].fallbacks);
                XtpFontChainClear(&universe->chains[role]);
        }
        XtpFontChainClear(&universe->primary_bold_chain);
        XtpFontChainClear(&universe->wide_bold_chain);
        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                unsigned int style;

                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        int fallback;

                        for (fallback = 0; fallback < XTP_FALLBACK_FACE_COUNT; ++fallback) {
                                FcPattern *pattern =
                                    universe->named_patterns[slot][style][fallback];

                                if (pattern != NULL)
                                        FcPatternDestroy(pattern);
                        }
                }
        }
        free(universe);
}

void
VtFontUniverseClose(Vt100Rec *vt)
{
        VtFontUniverseDestroy(vt, vt->vt.font_universe);
        vt->vt.font_universe = NULL;
        vt->vt.use_xft = False;
}

Boolean
VtFontEnsureSlot(Vt100Rec *vt, int slot)
{
        XtpFontUniverse *universe = vt->vt.font_universe;
        XtpFontChain *face_chain;
        XtpFontChain *wide_chain;
        XtpFontChain *emoji_chain;
        XtpFontChain *han_chain;
        char *face;
        char *wide_face;
        char *emoji_face;
        char *han_face;
        char *bold_face;
        char *wide_bold_face;
        double size;

        if (universe == NULL || slot < 0 || slot >= XTP_FONT_SLOTS)
                return False;
        if (universe->slot_attempted[slot])
                return universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] !=
                       NULL;
        universe->slot_attempted[slot] = True;
        face_chain = &universe->chains[XTP_FONT_ROLE_PRIMARY];
        wide_chain = &universe->chains[XTP_FONT_ROLE_WIDE];
        emoji_chain = &universe->chains[XTP_FONT_ROLE_EMOJI];
        han_chain = &universe->chains[XTP_FONT_ROLE_HAN];
        face = face_chain->count != 0 ? face_chain->entries[0] : NULL;
        wide_face = wide_chain->count != 0 ? wide_chain->entries[0] : NULL;
        emoji_face = emoji_chain->count != 0 ? emoji_chain->entries[0] : NULL;
        han_face = han_chain->count != 0 ? han_chain->entries[0] : NULL;
        bold_face = universe->primary_bold_chain.count != 0
                        ? universe->primary_bold_chain.entries[0]
                        : NULL;
        wide_bold_face =
            universe->wide_bold_chain.count != 0 ? universe->wide_bold_chain.entries[0] : NULL;
        size = slot == 0 ? universe->base_size : PositiveNumber(vt->vt.face_size_names[slot], 0.0);
        if (size <= 0.0) {
                XFontStruct *bitmap =
                    vt->vt.fonts[slot] != NULL ? vt->vt.fonts[slot] : vt->vt.initial_font;
                unsigned long area =
                    (unsigned long)VtBitmapFontWidth(bitmap) * VtBitmapFontHeight(bitmap);

                size =
                    universe->base_size * sqrt((double)area / (double)universe->bitmap_base_area);
        }
        ResolveNamedFallbackSlot(vt, slot, size);
        (void)LoadXftSlot(vt, slot, face, size, !Nonempty(bold_face));
        LoadXftBoldOverride(
            vt, slot, "primary", bold_face, size,
            universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot],
            &universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_BOLD][slot]);
        LoadXftRoleSlot(vt, slot, "doublesize", wide_face, size, !Nonempty(wide_bold_face),
                        universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_NORMAL],
                        universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_BOLD],
                        universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_ITALIC],
                        universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_BOLD_ITALIC]);
        LoadXftBoldOverride(vt, slot, "doublesize", wide_bold_face, size,
                            universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_NORMAL][slot],
                            &universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_BOLD][slot]);
        LoadXftRoleSlot(vt, slot, "emoji", emoji_face, size, True,
                        universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_NORMAL],
                        universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_BOLD],
                        universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_ITALIC],
                        universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_BOLD_ITALIC]);
        LoadXftRoleSlot(vt, slot, "han", han_face, size, True,
                        universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_NORMAL],
                        universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_BOLD],
                        universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_ITALIC],
                        universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_BOLD_ITALIC]);
        LoadXftRoleFallbacks(
            vt, slot, "primary", face, face_chain->count > 1 ? face_chain->entries[1] : NULL,
            bold_face,
            universe->primary_bold_chain.count > 1 ? universe->primary_bold_chain.entries[1] : NULL,
            size, universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot],
            universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_BOLD][slot],
            universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_ITALIC][slot],
            universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot],
            &universe->roles[XTP_FONT_ROLE_PRIMARY].fallbacks, universe->named_enabled);
        LoadXftRoleFallbacks(
            vt, slot, "doublesize", wide_face,
            wide_chain->count > 1 ? wide_chain->entries[1] : NULL, wide_bold_face,
            universe->wide_bold_chain.count > 1 ? universe->wide_bold_chain.entries[1] : NULL, size,
            universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_NORMAL][slot],
            universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_BOLD][slot],
            universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_ITALIC][slot],
            universe->roles[XTP_FONT_ROLE_WIDE].fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot],
            &universe->roles[XTP_FONT_ROLE_WIDE].fallbacks, universe->named_enabled);
        LoadXftRoleFallbacks(
            vt, slot, "emoji", emoji_face, emoji_chain->count > 1 ? emoji_chain->entries[1] : NULL,
            NULL, NULL, size,
            universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_NORMAL][slot],
            universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_BOLD][slot],
            universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_ITALIC][slot],
            universe->roles[XTP_FONT_ROLE_EMOJI].fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot],
            &universe->roles[XTP_FONT_ROLE_EMOJI].fallbacks, universe->named_enabled);
        LoadXftRoleFallbacks(
            vt, slot, "han", han_face, han_chain->count > 1 ? han_chain->entries[1] : NULL, NULL,
            NULL, size, universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_NORMAL][slot],
            universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_BOLD][slot],
            universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_ITALIC][slot],
            universe->roles[XTP_FONT_ROLE_HAN].fonts[XTP_XFT_STYLE_BOLD_ITALIC][slot],
            &universe->roles[XTP_FONT_ROLE_HAN].fallbacks, universe->named_enabled);
        return universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][slot] != NULL;
}

void
VtFontUniverseInitialize(Vt100Rec *vt)
{
        XtpFontUniverse *universe = vt->vt.font_universe;
        XtpFontChain *face_chain;
        XtpFontChain *wide_chain;
        XtpFontChain *emoji_chain;
        XtpFontChain *han_chain;
        XtpFontChain *bold_chain;
        XtpFontChain *wide_bold_chain;
        char *face;
        char *wide_face;
        char *emoji_face;
        char *han_face;
        Boolean requested = VtFontRequested(vt);
        double base_size = PositiveNumber(vt->vt.face_size_names[0], 8.0);
        double embedded_size;

        if (universe == NULL)
                return;
        face_chain = &universe->chains[XTP_FONT_ROLE_PRIMARY];
        wide_chain = &universe->chains[XTP_FONT_ROLE_WIDE];
        emoji_chain = &universe->chains[XTP_FONT_ROLE_EMOJI];
        han_chain = &universe->chains[XTP_FONT_ROLE_HAN];
        bold_chain = &universe->primary_bold_chain;
        wide_bold_chain = &universe->wide_bold_chain;
        XtpFontRouteCacheDestroy(universe->route_cache);
        universe->route_cache = XtpFontRouteCacheCreate(XTP_FONT_ROUTE_CACHE_CAPACITY);
        ++universe->generation;
        if (universe->generation == 0)
                universe->generation = 1;
        if (universe->route_cache == NULL)
                XtpLog(XTP_LOG_WARNING, "font",
                       "cannot allocate font routing cache; routing remains uncached");
        vt->vt.use_xft = False;
        universe->emoji_presentation = ParseEmojiPolicy(vt->vt.emoji_presentation_name);
        universe->color_glyphs = vt->vt.color_glyphs;
        universe->system_fallback = vt->vt.system_fallback;
        universe->limit_fontsets = vt->vt.limit_fontsets;
        /* Retained for the backend-owned DEC double-height gap (LM-04/05). */
        universe->limit_fontheight = vt->vt.limit_fontheight;
        universe->limit_fontwidth = vt->vt.limit_fontwidth;
        if (universe->limit_fontsets < 0) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting number of fontsets to 255 (was %d)",
                       universe->limit_fontsets);
                universe->limit_fontsets = 255;
        } else if (universe->limit_fontsets > 255) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting number of fontsets to 255 (was %d)",
                       universe->limit_fontsets);
                universe->limit_fontsets = 255;
        }
        if (universe->limit_fontheight > 50) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting extra fontheight percent to 50 (was %d)",
                       universe->limit_fontheight);
                universe->limit_fontheight = 50;
        }
        if (universe->limit_fontwidth > 50) {
                XtpLog(XTP_LOG_WARNING, "font", "limiting extra fontwidth percent to 50 (was %d)",
                       universe->limit_fontwidth);
                universe->limit_fontwidth = 50;
        }
        if (XtpFontChainParse(vt->vt.face_name, face_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_doublesize, wide_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_emoji, emoji_chain) != 0 ||
            XtpFontChainParse(vt->vt.face_name_han, han_chain) != 0 ||
            XtpFontChainParseXftEntries(vt->vt.bold_font_name, bold_chain) != 0 ||
            XtpFontChainParseXftEntries(vt->vt.wide_bold_font_name, wide_bold_chain) != 0) {
                XtpLog(XTP_LOG_WARNING, "font", "cannot parse Xft slot chain");
                goto done;
        }
        face = face_chain->count != 0 ? face_chain->entries[0] : NULL;
        wide_face = wide_chain->count != 0 ? wide_chain->entries[0] : NULL;
        emoji_face = emoji_chain->count != 0 ? emoji_chain->entries[0] : NULL;
        han_face = han_chain->count != 0 ? han_chain->entries[0] : NULL;
        if (face_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font", "faceName discarded %zu Xft list entries after 2",
                       face_chain->discarded);
        if (wide_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameDoublesize discarded %zu Xft list entries after 2",
                       wide_chain->discarded);
        if (emoji_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameEmoji discarded %zu Xft list entries after 2",
                       emoji_chain->discarded);
        if (han_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "faceNameHan discarded %zu Xft list entries after 2", han_chain->discarded);
        if (bold_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font", "boldFont discarded %zu Xft list entries after 2",
                       bold_chain->discarded);
        if (wide_bold_chain->discarded != 0)
                XtpLog(XTP_LOG_WARNING, "font",
                       "wideBoldFont discarded %zu Xft list entries after 2",
                       wide_bold_chain->discarded);
        if (TrimFaceSize(face, &embedded_size)) {
                base_size = embedded_size > 0.0 ? embedded_size : 8.0;
                XtpLog(XTP_LOG_DEBUG, "font", "faceName embedded size selects points=%.2f",
                       base_size);
        }
        (void)TrimFaceSize(wide_face, NULL);
        (void)TrimFaceSize(emoji_face, NULL);
        (void)TrimFaceSize(han_face, NULL);
        if (!Nonempty(face)) {
                if (requested)
                        XtpLog(XTP_LOG_WARNING, "font",
                               "renderFont=%s ignored because faceName is empty",
                               vt->vt.render_font_name != NULL ? vt->vt.render_font_name
                                                               : "(null)");
                goto done;
        }

        universe->base_size = base_size;
        universe->bitmap_base_area = (unsigned long)VtBitmapFontWidth(vt->vt.initial_font) *
                                     VtBitmapFontHeight(vt->vt.initial_font);
        if (universe->bitmap_base_area == 0)
                universe->bitmap_base_area = 1;
        ResolveNamedFallbackRoles(vt, base_size, universe);
        (void)VtFontEnsureSlot(vt, 0);
        if (requested &&
            universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][0] != NULL) {
                vt->vt.use_xft = True;
                universe->shaper = XtpShaperCreate();
                if (universe->shaper == NULL)
                        XtpLog(XTP_LOG_WARNING, "font",
                               "cannot create HarfBuzz font_universe->shaper");
        }

done:
        XtpLog(XTP_LOG_INFO, "font",
               "renderer=%s renderFont=%s faceName=%s faceNameDoublesize=%s faceNameEmoji=%s "
               "faceNameHan=%s "
               "faceSize=%.2f emojiPresentation=%s colorGlyphs=%s Unicode=%s",
               vt->vt.use_xft ? "xft" : "xlib-bitmap",
               vt->vt.render_font_name != NULL ? vt->vt.render_font_name : "(null)",
               vt->vt.face_name != NULL ? vt->vt.face_name : "(null)",
               vt->vt.face_name_doublesize != NULL ? vt->vt.face_name_doublesize : "(null)",
               vt->vt.face_name_emoji != NULL ? vt->vt.face_name_emoji : "(null)",
               vt->vt.face_name_han != NULL ? vt->vt.face_name_han : "(null)", base_size,
               vt->vt.emoji_presentation_name != NULL ? vt->vt.emoji_presentation_name : "unicode",
               vt->vt.color_glyphs ? "true" : "false", XtpEmojiUnicodeVersion());
}

static void
ReportRetainedPrimary(Vt100Rec *vt, const char *configured)
{
        XtpFontUniverse *universe = vt->vt.font_universe;
        static const char *const names[] = {"normal", "bold", "italic", "bold-italic"};
        unsigned int style;
        int slot;

        if (universe == NULL)
                return;
        for (slot = 0; slot < XTP_FONT_SLOTS; ++slot) {
                for (style = 0; style < XTP_XFT_STYLE_COUNT; ++style) {
                        XftFont *font = universe->roles[XTP_FONT_ROLE_PRIMARY].fonts[style][slot];

                        XtpFontRoutingReportLoad(vt->vt.font_routing_report, "primary", slot,
                                                 names[style], 1, configured,
                                                 font != NULL ? font->pattern : NULL, "retained",
                                                 universe->generation);
                }
        }
}

Boolean
VtFontUniverseReload(Vt100Rec *vt)
{
        XtpFontUniverse *previous = vt->vt.font_universe;
        XtpFontUniverse *candidate;
        XtpFontRoutingReport *report = vt->vt.font_routing_report;
        XtpFontRoutingReport *build_report =
            report != NULL ? XtpFontRoutingReportCreate(true) : NULL;
        Boolean requested = VtFontRequested(vt);
        Boolean previous_use_xft = vt->vt.use_xft;

        candidate = calloc(1, sizeof(*candidate));
        if (candidate == NULL) {
                XtpLog(XTP_LOG_WARNING, "font",
                       "FR-RELOADFAIL slot=primary cause=cannot allocate font universe");
                XtpFontRoutingReportDestroy(build_report);
                return False;
        }
        candidate->generation = previous != NULL ? previous->generation : 0;
        vt->vt.font_universe = candidate;
        vt->vt.font_routing_report = build_report;
        vt->vt.use_xft = False;
        VtFontUniverseInitialize(vt);
        vt->vt.font_routing_report = report;
        if (requested &&
            candidate->roles[XTP_FONT_ROLE_PRIMARY].fonts[XTP_XFT_STYLE_NORMAL][0] == NULL) {
                static const char cause[] = "configured faceName has no usable primary";

                XtpLog(XTP_LOG_WARNING, "font", "FR-RELOADFAIL slot=primary cause=%s", cause);
                XtpFontRoutingReportMergeBuild(report, build_report);
                XtpFontRoutingReportReloadFailure(report, "primary", cause);
                vt->vt.font_universe = previous;
                vt->vt.use_xft = previous_use_xft;
                ReportRetainedPrimary(vt, vt->vt.face_name);
                VtFontUniverseDestroy(vt, candidate);
                XtpFontRoutingReportDestroy(build_report);
                return False;
        }
        VtFontUniverseDestroy(vt, previous);
        XtpFontRoutingReportMergeBuild(report, build_report);
        XtpFontRoutingReportDestroy(build_report);
        if (vt->vt.use_xft && vt->vt.current_font != 0 &&
            !VtFontEnsureSlot(vt, vt->vt.current_font))
                vt->vt.current_font = 0;
        if (!vt->vt.use_xft && vt->vt.fonts[vt->vt.current_font] == NULL)
                vt->vt.current_font = 0;
        VtFontReloadApplied(vt);
        XtpLog(XTP_LOG_INFO, "font", "transactional reload generation=%u renderer=%s",
               candidate->generation, vt->vt.use_xft ? "xft" : "xlib-bitmap");
        return True;
}
