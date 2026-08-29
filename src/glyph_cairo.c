#include "glyph_cairo.h"

#include "diagnostics.h"

#include <cairo-ft.h>
#include <cairo-xlib.h>
#include <ft2build.h>
#include FT_OUTLINE_H
#include <stdlib.h>

#define XTP_CAIRO_FONT_CACHE_SIZE 96

typedef struct
{
        /*
         * Xft fonts currently live for the renderer's whole lifetime. Any future
         * slot reload must forget entries before XftFontClose so pointer reuse
         * cannot alias a stale key, and must also clear vt_draw's ink cache.
         */
        XftFont *font;
        unsigned int available_width;
        unsigned int cell_height;
        cairo_font_face_t *face;
        cairo_scaled_font_t *scaled;
        double size;
        double ascent;
        double descent;
} CairoFontEntry;

struct XtpCairo
{
        cairo_surface_t *surface;
        cairo_t *cr;
        CairoFontEntry fonts[XTP_CAIRO_FONT_CACHE_SIZE];
        size_t font_count;
        size_t next_font;
};

typedef struct
{
        cairo_t *cr;
        double origin_x;
        double origin_y;
} CairoOutlineContext;

static int
OutlineMoveTo(const FT_Vector *point, void *user)
{
        CairoOutlineContext *context = user;

        cairo_move_to(context->cr, context->origin_x + (double)point->x / 64.0,
                      context->origin_y - (double)point->y / 64.0);
        return 0;
}

static int
OutlineLineTo(const FT_Vector *point, void *user)
{
        CairoOutlineContext *context = user;

        cairo_line_to(context->cr, context->origin_x + (double)point->x / 64.0,
                      context->origin_y - (double)point->y / 64.0);
        return 0;
}

static int
OutlineConicTo(const FT_Vector *control, const FT_Vector *point, void *user)
{
        CairoOutlineContext *context = user;
        double current_x;
        double current_y;
        double control_x = context->origin_x + (double)control->x / 64.0;
        double control_y = context->origin_y - (double)control->y / 64.0;
        double point_x = context->origin_x + (double)point->x / 64.0;
        double point_y = context->origin_y - (double)point->y / 64.0;

        cairo_get_current_point(context->cr, &current_x, &current_y);
        cairo_curve_to(context->cr, current_x + (2.0 / 3.0) * (control_x - current_x),
                       current_y + (2.0 / 3.0) * (control_y - current_y),
                       point_x + (2.0 / 3.0) * (control_x - point_x),
                       point_y + (2.0 / 3.0) * (control_y - point_y), point_x, point_y);
        return 0;
}

static int
OutlineCubicTo(const FT_Vector *first, const FT_Vector *second, const FT_Vector *point, void *user)
{
        CairoOutlineContext *context = user;

        cairo_curve_to(context->cr, context->origin_x + (double)first->x / 64.0,
                       context->origin_y - (double)first->y / 64.0,
                       context->origin_x + (double)second->x / 64.0,
                       context->origin_y - (double)second->y / 64.0,
                       context->origin_x + (double)point->x / 64.0,
                       context->origin_y - (double)point->y / 64.0);
        return 0;
}

Boolean
XtpCairoFontIsColor(const XftFont *font)
{
        FcBool color = FcFalse;

        return font != NULL && font->pattern != NULL &&
               FcPatternGetBool(font->pattern, FC_COLOR, 0, &color) == FcResultMatch && color;
}

static cairo_scaled_font_t *
CreateScaledFont(cairo_font_face_t *face, double size, cairo_font_options_t *options)
{
        cairo_matrix_t font_matrix;
        cairo_matrix_t identity;

        cairo_matrix_init_scale(&font_matrix, size, size);
        cairo_matrix_init_identity(&identity);
        return cairo_scaled_font_create(face, &font_matrix, &identity, options);
}

static CairoFontEntry *
FindFont(XtpCairo *renderer, XftFont *font, unsigned int available_width, unsigned int cell_height)
{
        cairo_font_options_t *options;
        cairo_font_extents_t extents;
        cairo_font_face_t *face;
        cairo_scaled_font_t *scaled;
        CairoFontEntry *entry;
        double size;
        double fit;
        size_t index;

        if (renderer == NULL || font == NULL || font->pattern == NULL || available_width == 0 ||
            cell_height == 0)
                return NULL;
        for (index = 0; index < renderer->font_count; ++index) {
                entry = &renderer->fonts[index];
                if (entry->font == font && entry->available_width == available_width &&
                    entry->cell_height == cell_height)
                        return entry;
        }
        face = cairo_ft_font_face_create_for_pattern(font->pattern);
        if (cairo_font_face_status(face) != CAIRO_STATUS_SUCCESS) {
                XtpLog(XTP_LOG_DEBUG, "font", "Cairo font face rejected: %s",
                       cairo_status_to_string(cairo_font_face_status(face)));
                cairo_font_face_destroy(face);
                return NULL;
        }
        options = cairo_font_options_create();
        cairo_font_options_set_color_mode(options, CAIRO_COLOR_MODE_COLOR);
        size = font->height > 0 ? (double)font->height : 1.0;
        if (size > (double)available_width)
                size = (double)available_width;
        scaled = CreateScaledFont(face, size, options);
        if (cairo_scaled_font_status(scaled) != CAIRO_STATUS_SUCCESS) {
                cairo_scaled_font_destroy(scaled);
                cairo_font_options_destroy(options);
                cairo_font_face_destroy(face);
                return NULL;
        }
        cairo_scaled_font_extents(scaled, &extents);
        fit = extents.ascent + extents.descent;
        if (fit > (double)cell_height && fit > 0.0) {
                size *= (double)cell_height / fit;
                cairo_scaled_font_destroy(scaled);
                scaled = CreateScaledFont(face, size, options);
                if (cairo_scaled_font_status(scaled) != CAIRO_STATUS_SUCCESS) {
                        cairo_scaled_font_destroy(scaled);
                        cairo_font_options_destroy(options);
                        cairo_font_face_destroy(face);
                        return NULL;
                }
                cairo_scaled_font_extents(scaled, &extents);
        }
        cairo_font_options_destroy(options);
        if (renderer->font_count < XTP_CAIRO_FONT_CACHE_SIZE) {
                entry = &renderer->fonts[renderer->font_count++];
        } else {
                /* Ink verdicts intentionally outlive these recreatable scaled fonts. */
                entry = &renderer->fonts[renderer->next_font];
                cairo_scaled_font_destroy(entry->scaled);
                cairo_font_face_destroy(entry->face);
                renderer->next_font = (renderer->next_font + 1U) % XTP_CAIRO_FONT_CACHE_SIZE;
        }
        entry->font = font;
        entry->available_width = available_width;
        entry->cell_height = cell_height;
        entry->face = face;
        entry->scaled = scaled;
        entry->size = size;
        entry->ascent = extents.ascent;
        entry->descent = extents.descent;
        XtpLog(XTP_LOG_DEBUG, "font",
               "Cairo role cached width=%u height=%u size=%.2f ascent=%.2f descent=%.2f",
               available_width, cell_height, size, entry->ascent, entry->descent);
        return entry;
}

static Boolean
BuildCairoGlyphs(const CairoFontEntry *entry, const XtpGlyphRun *run, const XRectangle *area,
                 cairo_glyph_t glyphs[XTP_GLYPH_RUN_CAPACITY])
{
        cairo_text_extents_t extents;
        double scale;
        double pen_x = 0.0;
        double pen_y = 0.0;
        double origin_x;
        double baseline;
        double line_height;
        unsigned int index;

        if (entry == NULL || run == NULL || run->count == 0 ||
            run->count > XTP_GLYPH_RUN_CAPACITY || run->units_per_em == 0)
                return False;
        scale = entry->size / (double)run->units_per_em;
        for (index = 0; index < run->count; ++index) {
                glyphs[index].index = run->glyphs[index].index;
                glyphs[index].x = pen_x + (double)run->glyphs[index].x_offset * scale;
                glyphs[index].y = -(pen_y + (double)run->glyphs[index].y_offset * scale);
                pen_x += (double)run->glyphs[index].x_advance * scale;
                pen_y += (double)run->glyphs[index].y_advance * scale;
        }
        cairo_scaled_font_glyph_extents(entry->scaled, glyphs, (int)run->count, &extents);
        origin_x =
            (double)area->x + ((double)area->width - extents.width) / 2.0 - extents.x_bearing;
        line_height = entry->ascent + entry->descent;
        baseline = (double)area->y + ((double)area->height - line_height) / 2.0 + entry->ascent;
        for (index = 0; index < run->count; ++index) {
                glyphs[index].x += origin_x;
                glyphs[index].y += baseline;
        }
        return True;
}

static Boolean
GlyphHasOutline(cairo_scaled_font_t *scaled, FT_UInt glyph)
{
        FT_Face face;
        FT_Error error;
        Boolean available;

        face = cairo_ft_scaled_font_lock_face(scaled);
        if (face == NULL)
                return False;
        error = FT_Load_Glyph(face, glyph, FT_LOAD_NO_BITMAP);
        available = error == 0 && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE &&
                    face->glyph->outline.n_contours > 0 && face->glyph->outline.n_points >= 3;
        if (!available)
                XtpLog(XTP_LOG_DEBUG, "font",
                       "outline fallback rejected glyph=%u error=%d format=%lu contours=%d "
                       "points=%d",
                       glyph, error, (unsigned long)face->glyph->format,
                       face->glyph->outline.n_contours, face->glyph->outline.n_points);
        cairo_ft_scaled_font_unlock_face(scaled);
        return available;
}

static Boolean
PaintOutline(cairo_t *cr, cairo_scaled_font_t *scaled, FT_UInt glyph, double x, double baseline)
{
        static const FT_Outline_Funcs functions = {
            OutlineMoveTo, OutlineLineTo, OutlineConicTo, OutlineCubicTo, 0, 0,
        };
        FT_Face face;
        CairoOutlineContext context;
        FT_Error error;

        face = cairo_ft_scaled_font_lock_face(scaled);
        if (face == NULL)
                return False;
        error = FT_Load_Glyph(face, glyph, FT_LOAD_NO_BITMAP);
        if (error != 0 || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE ||
            face->glyph->outline.n_contours <= 0 || face->glyph->outline.n_points < 3) {
                cairo_ft_scaled_font_unlock_face(scaled);
                return False;
        }
        context.cr = cr;
        context.origin_x = x;
        context.origin_y = baseline;
        cairo_new_path(cr);
        error = FT_Outline_Decompose(&face->glyph->outline, &functions, &context);
        cairo_ft_scaled_font_unlock_face(scaled);
        if (error != 0) {
                cairo_new_path(cr);
                return False;
        }
        cairo_fill(cr);
        return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

static Boolean
PaintGlyphRun(cairo_t *cr, const CairoFontEntry *entry, const XtpGlyphRun *run,
              Boolean color_glyphs, const XRectangle *area)
{
        cairo_glyph_t glyphs[XTP_GLYPH_RUN_CAPACITY];
        unsigned int index;

        if (!BuildCairoGlyphs(entry, run, area, glyphs))
                return False;
        if (!color_glyphs) {
                /* Decline the complete run before painting any partial outline. */
                for (index = 0; index < run->count; ++index) {
                        if (!GlyphHasOutline(entry->scaled, (FT_UInt)glyphs[index].index))
                                return False;
                }
                for (index = 0; index < run->count; ++index) {
                        if (!PaintOutline(cr, entry->scaled, (FT_UInt)glyphs[index].index,
                                          glyphs[index].x, glyphs[index].y))
                                return False;
                }
        } else {
                cairo_set_scaled_font(cr, entry->scaled);
                cairo_show_glyphs(cr, glyphs, (int)run->count);
        }
        return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

XtpCairo *
XtpCairoCreate(Display *display, Drawable drawable, Visual *visual, int width, int height)
{
        XtpCairo *renderer;

        if (display == NULL || drawable == None || visual == NULL || width <= 0 || height <= 0)
                return NULL;
        renderer = calloc(1, sizeof(*renderer));
        if (renderer == NULL)
                return NULL;
        renderer->surface = cairo_xlib_surface_create(display, drawable, visual, width, height);
        if (cairo_surface_status(renderer->surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(renderer->surface);
                free(renderer);
                return NULL;
        }
        renderer->cr = cairo_create(renderer->surface);
        if (cairo_status(renderer->cr) != CAIRO_STATUS_SUCCESS) {
                cairo_destroy(renderer->cr);
                cairo_surface_destroy(renderer->surface);
                free(renderer);
                return NULL;
        }
        return renderer;
}

void
XtpCairoResize(XtpCairo *renderer, int width, int height)
{
        if (renderer != NULL && width > 0 && height > 0)
                cairo_xlib_surface_set_size(renderer->surface, width, height);
}

void
XtpCairoDestroy(XtpCairo *renderer)
{
        size_t index;

        if (renderer == NULL)
                return;
        cairo_destroy(renderer->cr);
        cairo_surface_destroy(renderer->surface);
        for (index = 0; index < renderer->font_count; ++index) {
                cairo_scaled_font_destroy(renderer->fonts[index].scaled);
                cairo_font_face_destroy(renderer->fonts[index].face);
        }
        free(renderer);
}

Boolean
XtpCairoGlyphRunHasInk(XtpCairo *renderer, XftFont *font, const XtpGlyphRun *run,
                       Boolean color_glyphs, unsigned int available_width, unsigned int cell_height)
{
        unsigned int width = 3U * available_width;
        unsigned int height = 3U * cell_height;
        cairo_surface_t *surface;
        cairo_t *cr;
        CairoFontEntry *entry;
        XRectangle area;
        unsigned char *data;
        int stride;
        unsigned int x;
        unsigned int y;
        Boolean ink = False;

        entry = FindFont(renderer, font, available_width, cell_height);
        if (entry == NULL || run == NULL || run->missing)
                return False;
        surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)width, (int)height);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(surface);
                return False;
        }
        cr = cairo_create(surface);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        area.x = (short)available_width;
        area.y = (short)cell_height;
        area.width = (unsigned short)available_width;
        area.height = (unsigned short)cell_height;
        cairo_rectangle(cr, area.x, area.y, area.width, area.height);
        cairo_clip(cr);
        if (!PaintGlyphRun(cr, entry, run, color_glyphs, &area)) {
                cairo_destroy(cr);
                cairo_surface_destroy(surface);
                return False;
        }
        cairo_destroy(cr);
        cairo_surface_flush(surface);
        data = cairo_image_surface_get_data(surface);
        stride = cairo_image_surface_get_stride(surface);
        for (y = 0; !ink && y < height; ++y) {
                const uint32_t *row = (const uint32_t *)(data + (size_t)y * (size_t)stride);

                for (x = 0; x < width; ++x) {
                        if ((row[x] >> 24) != 0U) {
                                ink = True;
                                break;
                        }
                }
        }
        cairo_surface_destroy(surface);
        return ink;
}

Boolean
XtpCairoDrawGlyphRun(XtpCairo *renderer, XftFont *font, const XtpGlyphRun *run,
                     Boolean color_glyphs, const XRenderColor *foreground, const XRectangle *area,
                     const XRectangle *clip)
{
        CairoFontEntry *entry;
        Boolean drawn;

        if (renderer == NULL || foreground == NULL || area == NULL || clip == NULL)
                return False;
        entry = FindFont(renderer, font, area->width, area->height);
        if (entry == NULL || run == NULL || run->missing)
                return False;
        cairo_surface_flush(renderer->surface);
        cairo_surface_mark_dirty_rectangle(renderer->surface, clip->x, clip->y, clip->width,
                                           clip->height);
        cairo_save(renderer->cr);
        cairo_rectangle(renderer->cr, clip->x, clip->y, clip->width, clip->height);
        cairo_clip(renderer->cr);
        cairo_set_source_rgba(
            renderer->cr, (double)foreground->red / 65535.0, (double)foreground->green / 65535.0,
            (double)foreground->blue / 65535.0, (double)foreground->alpha / 65535.0);
        drawn = PaintGlyphRun(renderer->cr, entry, run, color_glyphs, area);
        cairo_restore(renderer->cr);
        cairo_surface_flush(renderer->surface);
        return drawn;
}
