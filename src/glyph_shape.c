#include "glyph_shape.h"

#include "diagnostics.h"

#include <hb.h>
#include <hb-ot.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define XTP_SHAPER_FONT_CACHE_SIZE 64

typedef struct
{
        XftFont *xft;
        hb_font_t *font;
        unsigned int units_per_em;
        double x_pixel_scale;
        double y_pixel_scale;
} ShaperFontEntry;

struct XtpShaper
{
        hb_buffer_t *buffer;
        ShaperFontEntry fonts[XTP_SHAPER_FONT_CACHE_SIZE];
        size_t font_count;
        size_t next_font;
};

static ShaperFontEntry *
FindFont(XtpShaper *shaper, XftFont *font)
{
        ShaperFontEntry *entry;
        FcChar8 *file;
        FT_Face xft_face;
        hb_blob_t *blob;
        hb_face_t *face;
        hb_font_t *hb_font;
        int face_index = 0;
        unsigned int units_per_em;
        double x_pixel_scale;
        double y_pixel_scale;
        size_t index;

        if (shaper == NULL || font == NULL)
                return NULL;
        for (index = 0; index < shaper->font_count; ++index) {
                entry = &shaper->fonts[index];
                if (entry->xft == font)
                        return entry;
        }
        if (font->pattern == NULL ||
            FcPatternGetString(font->pattern, FC_FILE, 0, &file) != FcResultMatch)
                return NULL;
        (void)FcPatternGetInteger(font->pattern, FC_INDEX, 0, &face_index);
        blob = hb_blob_create_from_file((const char *)file);
        if (hb_blob_get_length(blob) == 0) {
                hb_blob_destroy(blob);
                return NULL;
        }
        face = hb_face_create(blob, (unsigned int)face_index & 0xffffU);
        hb_blob_destroy(blob);
        if (hb_face_get_glyph_count(face) == 0) {
                hb_face_destroy(face);
                return NULL;
        }
        units_per_em = hb_face_get_upem(face);
        if (units_per_em == 0)
                units_per_em = 1000U;
        hb_font = hb_font_create(face);
        hb_face_destroy(face);
        hb_ot_font_set_funcs(hb_font);
        hb_font_set_scale(hb_font, (int)units_per_em, (int)units_per_em);

        /* Read Xft's pixel scale, but never retain or shape through its live FT_Face. */
        xft_face = XftLockFace(font);
        if (xft_face == NULL) {
                hb_font_destroy(hb_font);
                return NULL;
        }
        x_pixel_scale = xft_face->size != NULL
                            ? (double)xft_face->size->metrics.x_scale / (65536.0 * 64.0)
                            : 1.0 / (double)units_per_em;
        y_pixel_scale = xft_face->size != NULL
                            ? (double)xft_face->size->metrics.y_scale / (65536.0 * 64.0)
                            : 1.0 / (double)units_per_em;
        XftUnlockFace(font);
        if (shaper->font_count < XTP_SHAPER_FONT_CACHE_SIZE) {
                entry = &shaper->fonts[shaper->font_count++];
        } else {
                entry = &shaper->fonts[shaper->next_font];
                shaper->next_font = (shaper->next_font + 1U) % XTP_SHAPER_FONT_CACHE_SIZE;
                XtpLog(XTP_LOG_DEBUG, "font", "shape cache evicted xft=%p", (void *)entry->xft);
                hb_font_destroy(entry->font);
        }
        entry->xft = font;
        entry->font = hb_font;
        entry->units_per_em = units_per_em;
        entry->x_pixel_scale = x_pixel_scale;
        entry->y_pixel_scale = y_pixel_scale;
        return entry;
}

XtpShaper *
XtpShaperCreate(void)
{
        XtpShaper *shaper = calloc(1, sizeof(*shaper));

        if (shaper == NULL)
                return NULL;
        shaper->buffer = hb_buffer_create();
        if (hb_buffer_allocation_successful(shaper->buffer) == 0) {
                hb_buffer_destroy(shaper->buffer);
                free(shaper);
                return NULL;
        }
        return shaper;
}

void
XtpShaperDestroy(XtpShaper *shaper)
{
        size_t index;

        if (shaper == NULL)
                return;
        hb_buffer_destroy(shaper->buffer);
        for (index = 0; index < shaper->font_count; ++index)
                hb_font_destroy(shaper->fonts[index].font);
        free(shaper);
}

static Boolean
ShapeUtf8(XtpShaper *shaper, XftFont *font, const char *text, size_t length,
          hb_buffer_flags_t flags, XtpGlyphRun *run)
{
        ShaperFontEntry *entry;
        const hb_glyph_info_t *infos;
        const hb_glyph_position_t *positions;
        unsigned int count;
        unsigned int index;

        if (run == NULL)
                return False;
        memset(run, 0, sizeof(*run));
        if (shaper == NULL || font == NULL || text == NULL || length == 0 || length > INT_MAX)
                return False;
        entry = FindFont(shaper, font);
        if (entry == NULL)
                return False;
        hb_buffer_clear_contents(shaper->buffer);
        hb_buffer_set_cluster_level(shaper->buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
        hb_buffer_set_flags(shaper->buffer, HB_BUFFER_FLAG_BOT | HB_BUFFER_FLAG_EOT | flags);
        hb_buffer_add_utf8(shaper->buffer, text, (int)length, 0, (int)length);
        hb_buffer_guess_segment_properties(shaper->buffer);
        hb_shape(entry->font, shaper->buffer, NULL, 0);
        infos = hb_buffer_get_glyph_infos(shaper->buffer, &count);
        positions = hb_buffer_get_glyph_positions(shaper->buffer, NULL);
        if (count == 0 || count > XTP_GLYPH_RUN_CAPACITY) {
                XtpLog(XTP_LOG_DEBUG, "font", "shape rejected bytes=%zu glyphs=%u capacity=%u",
                       length, count, XTP_GLYPH_RUN_CAPACITY);
                return False;
        }
        run->count = count;
        run->units_per_em = entry->units_per_em;
        run->x_pixel_scale = entry->x_pixel_scale;
        run->y_pixel_scale = entry->y_pixel_scale;
        for (index = 0; index < count; ++index) {
                run->glyphs[index].index = infos[index].codepoint;
                run->glyphs[index].x_advance = positions[index].x_advance;
                run->glyphs[index].y_advance = positions[index].y_advance;
                run->glyphs[index].x_offset = positions[index].x_offset;
                run->glyphs[index].y_offset = positions[index].y_offset;
                run->glyphs[index].cluster = infos[index].cluster;
                if (infos[index].codepoint == 0)
                        run->missing = True;
        }
        return True;
}

Boolean
XtpShapeUtf8(XtpShaper *shaper, XftFont *font, const char *text, size_t length, XtpGlyphRun *run)
{
        return ShapeUtf8(shaper, font, text, length, HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES, run);
}

Boolean
XtpShapeUtf8ForComposition(XtpShaper *shaper, XftFont *font, const char *text, size_t length,
                           XtpGlyphRun *run)
{
        return ShapeUtf8(shaper, font, text, length, HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES,
                         run);
}
