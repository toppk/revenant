#include "vt_widgetP.h"

#include "diagnostics.h"
#include "emoji_presentation.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static Pixel
RgbPixel(Vt100Rec *vt, uint8_t red, uint8_t green, uint8_t blue)
{
        size_t index;
        XColor color;

        for (index = 0; index < vt->vt.color_count; ++index) {
                ColorCacheEntry *entry = &vt->vt.colors[index];

                if (entry->used && entry->owned && entry->red == red && entry->green == green &&
                    entry->blue == blue)
                        return entry->pixel;
        }

        color.red = (unsigned short)(red * 257U);
        color.green = (unsigned short)(green * 257U);
        color.blue = (unsigned short)(blue * 257U);
        color.flags = DoRed | DoGreen | DoBlue;
        if (!XAllocColor(XtDisplay((Widget)vt), vt->core.colormap, &color))
                return vt->vt.foreground;
        {
                Pixel allocation_pixel = color.pixel;

                color.pixel = VtOpaquePixel(vt, color.pixel);

                if (vt->vt.color_count < XTP_COLOR_CACHE_SIZE) {
                        ColorCacheEntry *entry = &vt->vt.colors[vt->vt.color_count++];

                        entry->used = True;
                        entry->owned = True;
                        entry->red = red;
                        entry->green = green;
                        entry->blue = blue;
                        entry->pixel = color.pixel;
                        entry->allocation_pixel = allocation_pixel;
                        entry->xft.pixel = color.pixel;
                        entry->xft.color.red = color.red;
                        entry->xft.color.green = color.green;
                        entry->xft.color.blue = color.blue;
                        entry->xft.color.alpha = UINT16_MAX;
                }
        }
        return color.pixel;
}

static Pixel
RenderOpaqueColor(Vt100Rec *vt, XtpColor color, Boolean foreground)
{
        Pixel pixel = foreground ? vt->vt.foreground : vt->core.background_pixel;

        switch (color.kind) {
        case XTP_COLOR_DEFAULT:
                break;
        case XTP_COLOR_PALETTE:
        case XTP_COLOR_RGB:
                pixel = RgbPixel(vt, color.red, color.green, color.blue);
                break;
        }
        return VtOpaquePixel(vt, pixel);
}

static Pixel
RenderBackgroundSurface(Vt100Rec *vt, Pixel pixel)
{
        return XtpX11PixelWithAlpha(VtOpaquePixel(vt, pixel),
                                    vt->vt.alpha_visual ? &vt->vt.alpha_format : NULL,
                                    vt->vt.background_alpha);
}

static Pixel
RenderFrameColor(Vt100Rec *vt, XtpColor color, Boolean foreground)
{
        if (color.kind != XTP_COLOR_DEFAULT)
                return RenderOpaqueColor(vt, color, foreground);
        if (vt->vt.render_reverse_colors)
                return foreground ? vt->vt.opaque_background_pixel : vt->vt.foreground;
        return foreground ? vt->vt.foreground : vt->vt.opaque_background_pixel;
}

int
VtTerminalX(Vt100Rec *vt)
{
        int x = (int)vt->vt.internal_border;

        if (vt->vt.scroll_bar && !vt->vt.right_scroll_bar)
                x += (int)VtScrollbarTotalWidth(vt);
        return x;
}

static void
ResetVisualCells(Vt100Rec *vt, VisualCell *cells, size_t count)
{
        size_t index;

        memset(cells, 0, count * sizeof(*cells));
        for (index = 0; index < count; ++index) {
                cells[index].foreground = vt->vt.foreground;
                cells[index].background = vt->core.background_pixel;
                cells[index].opaque_background = vt->vt.opaque_background_pixel;
                cells[index].width = 1;
        }
}

static Boolean
EnsureFrameStorage(Vt100Rec *vt, unsigned int columns, unsigned int rows)
{
        size_t count = (size_t)columns * rows;

        if (columns == vt->vt.frame_columns && rows == vt->vt.frame_rows &&
            count <= vt->vt.frame_capacity && vt->vt.frame_cells != NULL &&
            vt->vt.pending_cells != NULL)
                return True;

        free(vt->vt.frame_cells);
        free(vt->vt.pending_cells);
        vt->vt.frame_cells = calloc(count, sizeof(*vt->vt.frame_cells));
        vt->vt.pending_cells = calloc(count, sizeof(*vt->vt.pending_cells));
        if (vt->vt.frame_cells == NULL || vt->vt.pending_cells == NULL) {
                free(vt->vt.frame_cells);
                free(vt->vt.pending_cells);
                vt->vt.frame_cells = NULL;
                vt->vt.pending_cells = NULL;
                vt->vt.frame_capacity = 0;
                vt->vt.frame_columns = 0;
                vt->vt.frame_rows = 0;
                vt->vt.frame_valid = False;
                return False;
        }
        vt->vt.frame_capacity = count;
        vt->vt.frame_columns = columns;
        vt->vt.frame_rows = rows;
        vt->vt.frame_valid = False;
        ResetVisualCells(vt, vt->vt.frame_cells, count);
        ResetVisualCells(vt, vt->vt.pending_cells, count);
        return True;
}

static VisualCell
MakeVisualCell(Vt100Rec *vt, const XtpRenderCell *cell)
{
        VisualCell visual = {0};
        Boolean translucent_background;
        size_t index;
        Boolean drawable = cell->utf8_length < sizeof(visual.text);

        visual.foreground = RenderFrameColor(vt, cell->foreground, True);
        visual.background = RenderFrameColor(vt, cell->background, False);
        visual.width = cell->width;
        if (cell->inverse) {
                Pixel temporary = visual.foreground;

                visual.foreground = visual.background;
                visual.background = temporary;
        }
        translucent_background =
            vt->vt.render_reverse_colors
                ? (cell->inverse ? cell->foreground.kind == XTP_COLOR_DEFAULT
                                 : cell->background.kind == XTP_COLOR_DEFAULT)
                : (!cell->inverse && cell->background.kind == XTP_COLOR_DEFAULT);
        if (cell->selected) {
                Pixel temporary = visual.foreground;

                visual.foreground = visual.background;
                visual.background = temporary;
                translucent_background = False;
        }
        visual.opaque_background = visual.background;
        if (translucent_background)
                visual.background = RenderBackgroundSurface(vt, visual.background);
        if (drawable) {
                for (index = 0; index < cell->utf8_length; ++index) {
                        unsigned char byte = (unsigned char)cell->utf8[index];

                        if (byte < 0x20U || (!vt->vt.use_xft && byte >= 0x7fU)) {
                                drawable = False;
                                break;
                        }
                        visual.text[index] = (char)byte;
                }
        }
        if (!cell->invisible) {
                if (!drawable && cell->utf8_length != 0) {
                        visual.text[0] = '?';
                        visual.text_length = 1;
                } else {
                        visual.text_length = cell->utf8_length;
                }
        }
        visual.bold = cell->bold;
        visual.underline = cell->underline != 0 || VtHyperlinkUriEqualsCell(vt, cell);
        visual.strikethrough = cell->strikethrough;
        visual.overline = cell->overline;
        return visual;
}

static Boolean
EnsureXftDraw(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        if (!vt->vt.use_xft)
                return False;
        if (vt->vt.xft_draw != NULL)
                return True;
        if (!XtIsRealized(widget))
                return False;
        {
                XWindowAttributes attributes;

                if (!XGetWindowAttributes(XtDisplay(widget), XtWindow(widget), &attributes))
                        return False;
                vt->vt.xft_draw = XftDrawCreate(XtDisplay(widget), XtWindow(widget),
                                                attributes.visual, attributes.colormap);
        }
        if (vt->vt.xft_draw == NULL) {
                XtpLog(XTP_LOG_ERROR, "font", "cannot create Xft draw context");
                return False;
        }
        return True;
}

static Boolean
EnsureCairoDraw(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;
        XWindowAttributes attributes;

        if (!vt->vt.use_xft)
                return False;
        if (vt->vt.cairo_draw != NULL)
                return True;
        if (!XtIsRealized(widget) ||
            !XGetWindowAttributes(XtDisplay(widget), XtWindow(widget), &attributes))
                return False;
        vt->vt.cairo_draw = XtpCairoCreate(XtDisplay(widget), XtWindow(widget), attributes.visual,
                                           attributes.width, attributes.height);
        if (vt->vt.cairo_draw == NULL) {
                XtpLog(XTP_LOG_ERROR, "font", "cannot create Cairo draw context");
                return False;
        }
        return True;
}

static XftFont *
RoleFont(XftFont *normal, XftFont *bold_font, Boolean bold)
{
        return bold && bold_font != NULL ? bold_font : normal;
}

static Boolean
FontHasCluster(Vt100Rec *vt, XftFont *font, const char *text, size_t length, unsigned int width,
               Boolean color_glyphs, Boolean requires_composition, XtpGlyphRun *run)
{
        FT_UInt glyphs[XTP_GLYPH_RUN_CAPACITY];
        XGlyphInfo extents = {0};
        size_t index;

        if (font == NULL || length == 0 || length >= XTP_VISUAL_TEXT_CAPACITY ||
            !(requires_composition
                  ? XtpShapeUtf8ForComposition(vt->vt.shaper, font, text, length, run)
                  : XtpShapeUtf8(vt->vt.shaper, font, text, length, run)) ||
            run->missing || (requires_composition && run->count != 1U))
                return False;
        if (XtpCairoFontIsColor(font)) {
                Boolean has_ink;
                GlyphInkCacheEntry *entry;

                for (index = 0; index < XTP_GLYPH_INK_CACHE_SIZE; ++index) {
                        entry = &vt->vt.glyph_ink_cache[index];
                        if (entry->font == font && entry->text_length == length &&
                            memcmp(entry->text, text, length) == 0 && entry->width == width &&
                            entry->color_glyphs == color_glyphs)
                                return entry->has_ink;
                }
                has_ink = EnsureCairoDraw(vt) &&
                          XtpCairoGlyphRunHasInk(vt->vt.cairo_draw, font, run, color_glyphs,
                                                 width * VtSlotWidth(vt, vt->vt.current_font),
                                                 VtSlotHeight(vt, vt->vt.current_font));
                entry = &vt->vt.glyph_ink_cache[vt->vt.next_glyph_ink_cache];
                entry->font = font;
                memcpy(entry->text, text, length);
                entry->text_length = (uint8_t)length;
                entry->width = (uint8_t)width;
                entry->color_glyphs = color_glyphs;
                entry->has_ink = has_ink;
                vt->vt.next_glyph_ink_cache =
                    (vt->vt.next_glyph_ink_cache + 1U) % XTP_GLYPH_INK_CACHE_SIZE;
                return has_ink;
        }
        for (index = 0; index < run->count; ++index)
                glyphs[index] = (FT_UInt)run->glyphs[index].index;
        XftGlyphExtents(XtDisplay((Widget)vt), font, glyphs, (int)run->count, &extents);
        return extents.width != 0 && extents.height != 0;
}

static XftFont *
RoleFontWithCluster(Vt100Rec *vt, XftFont *normal, XftFont *bold_font, Boolean bold,
                    const char *text, size_t length, unsigned int width, Boolean color_glyphs,
                    Boolean requires_composition, XtpGlyphRun *run)
{
        XftFont *font = RoleFont(normal, bold_font, bold);

        if (FontHasCluster(vt, font, text, length, width, color_glyphs, requires_composition, run))
                return font;
        if (bold && font != normal &&
            FontHasCluster(vt, normal, text, length, width, color_glyphs, requires_composition,
                           run))
                return normal;
        return NULL;
}

static XftFont *
SelectXftFont(Vt100Rec *vt, const char *text, size_t length, unsigned int width, Boolean bold,
              const char **role_name, uint32_t *base_out, XtpEmojiStyle *style_out,
              XtpGlyphRun *run_out)
{
        int slot = vt->vt.current_font;
        XtpEmojiClusterStyle cluster =
            XtpEmojiResolveClusterStyle(text, length, vt->vt.emoji_presentation);
        uint32_t base = cluster.base;
        XtpEmojiStyle style = cluster.style;
        Boolean color_glyphs;
        XftFont *font;

        if (run_out != NULL)
                memset(run_out, 0, sizeof(*run_out));

        if (base_out != NULL)
                *base_out = base;
        if (style_out != NULL)
                *style_out = style;
        color_glyphs = vt->vt.color_glyphs && style != XTP_EMOJI_STYLE_TEXT;
        if (style == XTP_EMOJI_STYLE_EMOJI) {
                font = RoleFontWithCluster(
                    vt, vt->vt.xft_emoji_fonts[slot], vt->vt.xft_emoji_bold_fonts[slot], bold, text,
                    length, width, color_glyphs, cluster.requires_composition, run_out);
                if (font != NULL) {
                        if (role_name != NULL)
                                *role_name = "emoji";
                        return font;
                }
                font = RoleFontWithCluster(
                    vt, vt->vt.xft_wide_fonts[slot], vt->vt.xft_wide_bold_fonts[slot], bold, text,
                    length, width, color_glyphs, cluster.requires_composition, run_out);
                if (font != NULL) {
                        if (role_name != NULL)
                                *role_name = "doublesize";
                        return font;
                }
        } else if (width > 1U) {
                font = RoleFontWithCluster(
                    vt, vt->vt.xft_wide_fonts[slot], vt->vt.xft_wide_bold_fonts[slot], bold, text,
                    length, width, color_glyphs, cluster.requires_composition, run_out);
                if (font != NULL) {
                        if (role_name != NULL)
                                *role_name = "doublesize";
                        return font;
                }
        }
        if (role_name != NULL)
                *role_name = "primary";
        font = RoleFont(vt->vt.xft_fonts[slot], vt->vt.xft_bold_fonts[slot], bold);
        if (run_out != NULL)
                (void)XtpShapeUtf8(vt->vt.shaper, font, text, length, run_out);
        return font;
}

static XftColor
CachedXftColor(Vt100Rec *vt, Pixel pixel)
{
        size_t index;
        XColor xcolor = {0};
        XftColor color = {0};

        for (index = 0; index < vt->vt.color_count; ++index) {
                ColorCacheEntry *entry = &vt->vt.colors[index];

                if (entry->used && entry->pixel == pixel)
                        return entry->xft;
        }
        xcolor.pixel = pixel;
        XQueryColor(XtDisplay((Widget)vt), vt->core.colormap, &xcolor);
        color.pixel = pixel;
        color.color.red = xcolor.red;
        color.color.green = xcolor.green;
        color.color.blue = xcolor.blue;
        color.color.alpha = VtPixelAlpha(vt, pixel);
        if (vt->vt.color_count < XTP_COLOR_CACHE_SIZE) {
                ColorCacheEntry *entry = &vt->vt.colors[vt->vt.color_count++];

                entry->used = True;
                entry->owned = False;
                entry->red = (uint8_t)(xcolor.red >> 8);
                entry->green = (uint8_t)(xcolor.green >> 8);
                entry->blue = (uint8_t)(xcolor.blue >> 8);
                entry->pixel = pixel;
                entry->xft = color;
        }
        return color;
}

static Boolean
DrawCairoFontRun(Vt100Rec *vt, XftFont *font, Pixel pixel, const XtpGlyphRun *run,
                 Boolean color_glyphs, const XRectangle *area, const XRectangle *clip)
{
        XftColor color;

        if (!XtpCairoFontIsColor(font) || run == NULL || run->count == 0 || run->missing ||
            area == NULL || clip == NULL || !EnsureCairoDraw(vt))
                return False;
        color = CachedXftColor(vt, pixel);
        return XtpCairoDrawGlyphRun(vt->vt.cairo_draw, font, run, color_glyphs, &color.color, area,
                                    clip);
}

static void
DrawXftGlyphRun(Vt100Rec *vt, XftFont *font, const XftColor *color, int x, int baseline,
                const XtpGlyphRun *run)
{
        XftGlyphFontSpec specs[XTP_GLYPH_RUN_CAPACITY];
        double pen_x = 0.0;
        double pen_y = 0.0;
        unsigned int index;

        for (index = 0; index < run->count; ++index) {
                specs[index].font = font;
                specs[index].glyph = run->glyphs[index].index;
                specs[index].x = (short)lround((double)x + (pen_x + run->glyphs[index].x_offset) *
                                                               run->x_pixel_scale);
                specs[index].y = (short)lround(
                    (double)baseline - (pen_y + run->glyphs[index].y_offset) * run->y_pixel_scale);
                pen_x += run->glyphs[index].x_advance;
                pen_y += run->glyphs[index].y_advance;
        }
        XftDrawGlyphFontSpec(vt->vt.xft_draw, color, specs, (int)run->count);
}

static void
PaintShapedText(Vt100Rec *vt, XftFont *font, Pixel pixel, const XftColor *color,
                const XtpGlyphRun *run, const char *text, size_t length, Boolean color_glyphs,
                int x, int baseline, const XRectangle *area, const XRectangle *clip)
{
        Boolean color_font = XtpCairoFontIsColor(font);
        Boolean drawn = DrawCairoFontRun(vt, font, pixel, run, color_glyphs, area, clip);

        if (drawn)
                return;
        if (!color_font && run != NULL && run->count != 0 && !run->missing) {
                DrawXftGlyphRun(vt, font, color, x, baseline, run);
                return;
        }
        if ((!color_font || color_glyphs) && (run == NULL || run->count == 0 || run->missing))
                XftDrawStringUtf8(vt->vt.xft_draw, color, font, x, baseline, (const FcChar8 *)text,
                                  (int)length);
        /* A declined color font without a genuine outline intentionally paints nothing. */
}

static Boolean
IntersectRectangles(const XRectangle *left, const XRectangle *right, XRectangle *result)
{
        int x1 = left->x > right->x ? left->x : right->x;
        int y1 = left->y > right->y ? left->y : right->y;
        int x2 = left->x + (int)left->width < right->x + (int)right->width
                     ? left->x + (int)left->width
                     : right->x + (int)right->width;
        int y2 = left->y + (int)left->height < right->y + (int)right->height
                     ? left->y + (int)left->height
                     : right->y + (int)right->height;

        if (x2 <= x1 || y2 <= y1)
                return False;
        result->x = (short)x1;
        result->y = (short)y1;
        result->width = (unsigned short)(x2 - x1);
        result->height = (unsigned short)(y2 - y1);
        return True;
}

static Boolean
SetTextClip(Vt100Rec *vt, const XRectangle *requested, XRectangle *effective)
{
        Widget widget = (Widget)vt;
        XRectangle clipped;
        const XRectangle *clip = requested;

        if (vt->vt.damage_clip_active) {
                if (!IntersectRectangles(requested, &vt->vt.damage_clip, &clipped))
                        return False;
                clip = &clipped;
        }
        if (effective != NULL)
                *effective = *clip;
        XSetClipRectangles(XtDisplay(widget), vt->vt.gc, 0, 0, (XRectangle *)clip, 1, Unsorted);
        if (vt->vt.use_xft && EnsureXftDraw(vt))
                (void)XftDrawSetClipRectangles(vt->vt.xft_draw, 0, 0, clip, 1);
        return True;
}

static void
ClearTextClip(Vt100Rec *vt)
{
        Widget widget = (Widget)vt;

        XSetClipMask(XtDisplay(widget), vt->vt.gc, None);
        if (vt->vt.use_xft && vt->vt.xft_draw != NULL)
                (void)XftDrawSetClip(vt->vt.xft_draw, NULL);
}

static void
DrawTextClipped(Vt100Rec *vt, Pixel pixel, int x, int baseline, const char *text, size_t length,
                Boolean bold, XftFont *selected_font, const XtpGlyphRun *run, Boolean color_glyphs,
                const XRectangle *area, const XRectangle *clip)
{
        Widget widget = (Widget)vt;
        XRectangle effective;
        const XRectangle *effective_clip = NULL;
        Boolean clip_set = False;

        if (length == 0)
                return;
        if (clip != NULL) {
                if (!SetTextClip(vt, clip, &effective))
                        return;
                effective_clip = &effective;
                clip_set = True;
        } else if (vt->vt.damage_clip_active) {
                if (!SetTextClip(vt, &vt->vt.damage_clip, &effective))
                        return;
                effective_clip = &effective;
                clip_set = True;
        }
        if (vt->vt.use_xft && EnsureXftDraw(vt)) {
                XftColor color = CachedXftColor(vt, pixel);
                XftFont *font = selected_font;

                if (font == NULL)
                        font = RoleFont(vt->vt.xft_fonts[vt->vt.current_font],
                                        vt->vt.xft_bold_fonts[vt->vt.current_font], bold);
                PaintShapedText(vt, font, pixel, &color, run, text, length, color_glyphs, x,
                                baseline, area, effective_clip);
        } else {
                XSetForeground(XtDisplay(widget), vt->vt.gc, pixel);
                XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, baseline, text,
                            (int)length);
                if (bold)
                        XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x + 1, baseline,
                                    text, (int)length);
        }
        if (clip_set)
                ClearTextClip(vt);
}

static void
DrawText(Vt100Rec *vt, Pixel pixel, int x, int baseline, const char *text, size_t length,
         Boolean bold, XftFont *selected_font)
{
        DrawTextClipped(vt, pixel, x, baseline, text, length, bold, selected_font, 0,
                        vt->vt.color_glyphs, NULL, NULL);
}

static void
PaintVisualRun(Vt100Rec *vt, const VisualCell *style, const XRectangle *area, int x, int baseline,
               const char *xft_text, size_t xft_length, const char *bitmap_text,
               size_t bitmap_length, XftFont *selected_font, const XtpGlyphRun *run,
               Boolean color_glyphs)
{
        Widget widget = (Widget)vt;
        XRectangle effective;

        if (!SetTextClip(vt, area, &effective))
                return;
        if (vt->vt.use_xft && EnsureXftDraw(vt)) {
                XftColor background = CachedXftColor(vt, style->background);
                XftColor foreground = CachedXftColor(vt, style->foreground);
                XftFont *font = selected_font;

                if (font == NULL)
                        font = RoleFont(vt->vt.xft_fonts[vt->vt.current_font],
                                        vt->vt.xft_bold_fonts[vt->vt.current_font], style->bold);
                XRenderFillRectangle(XtDisplay(widget), PictOpSrc, XftDrawPicture(vt->vt.xft_draw),
                                     &background.color, area->x, area->y, area->width,
                                     area->height);
                if (xft_length != 0)
                        PaintShapedText(vt, font, style->foreground, &foreground, run, xft_text,
                                        xft_length, color_glyphs, x, baseline, area, &effective);
        } else {
                XSetForeground(XtDisplay(widget), vt->vt.gc, style->foreground);
                XSetBackground(XtDisplay(widget), vt->vt.gc, style->background);
                XDrawImageString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, baseline,
                                 bitmap_text, (int)bitmap_length);
                if (style->bold)
                        XDrawString(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x + 1, baseline,
                                    bitmap_text, (int)bitmap_length);
        }
        ClearTextClip(vt);
}

static void
DrawDecorations(Vt100Rec *vt, const VisualCell *cell, const XRectangle *area)
{
        Widget widget = (Widget)vt;
        int right = area->x + (int)area->width - 1;

        if (!SetTextClip(vt, area, NULL))
                return;
        XSetForeground(XtDisplay(widget), vt->vt.gc, cell->foreground);
        if (cell->underline)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x,
                          area->y + (int)area->height - 1, right, area->y + (int)area->height - 1);
        if (cell->strikethrough)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x,
                          area->y + (int)area->height / 2, right, area->y + (int)area->height / 2);
        if (cell->overline)
                XDrawLine(XtDisplay(widget), XtWindow(widget), vt->vt.gc, area->x, area->y, right,
                          area->y);
        ClearTextClip(vt);
}

static void
DrawVisualCell(Vt100Rec *vt, const VisualCell *cell, unsigned int column, unsigned int row)
{
        unsigned int cell_width = VtSlotWidth(vt, vt->vt.current_font);
        unsigned int height = VtSlotHeight(vt, vt->vt.current_font);
        unsigned int columns = cell->width != 0 ? cell->width : 1U;
        XRectangle area;
        int x = VtTerminalX(vt) + (int)column * (int)cell_width;
        int y = (int)vt->vt.internal_border + (int)row * (int)height;

        if (cell->width == 0)
                return;
        area.x = (short)x;
        area.y = (short)y;
        area.width = (unsigned short)(columns * cell_width);
        area.height = (unsigned short)height;
        {
                char image[2] = {' ', ' '};
                XftFont *font = NULL;
                const char *role = "bitmap";
                uint32_t base = 0;
                XtpEmojiStyle style = XTP_EMOJI_STYLE_NONE;
                XtpGlyphRun run = {0};

                if (cell->text_length != 0)
                        image[0] = cell->text[0];
                if (vt->vt.use_xft)
                        font = SelectXftFont(vt, cell->text, cell->text_length, columns, cell->bold,
                                             &role, &base, &style, &run);
                PaintVisualRun(vt, cell, &area, x, y + VtSlotAscent(vt, vt->vt.current_font),
                               cell->text, cell->text_length, image, columns, font, &run,
                               vt->vt.color_glyphs && style != XTP_EMOJI_STYLE_TEXT);
                if (cell->text_length != 0)
                        XtpLog(XTP_LOG_DEBUG, "font",
                               "route base=U+%04X width=%u presentation=%s role=%s glyphs=%u", base,
                               columns,
                               style == XTP_EMOJI_STYLE_EMOJI
                                   ? "emoji"
                                   : (style == XTP_EMOJI_STYLE_TEXT ? "text" : "none"),
                               role, run.count);
        }
        DrawDecorations(vt, cell, &area);
}

static Boolean
SameVisualStyle(const VisualCell *left, const VisualCell *right)
{
        return left->foreground == right->foreground && left->background == right->background &&
               left->bold == right->bold && left->underline == right->underline &&
               left->strikethrough == right->strikethrough && left->overline == right->overline;
}

static void
DrawVisualRowRange(Vt100Rec *vt, unsigned int row, unsigned int first_column,
                   unsigned int end_column)
{
        enum
        {
                RUN_CAPACITY = 4096
        };
        unsigned int width = VtSlotWidth(vt, vt->vt.current_font);
        unsigned int height = VtSlotHeight(vt, vt->vt.current_font);
        unsigned int column = first_column;

        if (end_column > vt->vt.frame_columns)
                end_column = vt->vt.frame_columns;
        while (column < end_column) {
                const VisualCell *first =
                    &vt->vt.frame_cells[(size_t)row * vt->vt.frame_columns + column];
                XftFont *first_font =
                    vt->vt.use_xft
                        ? RoleFont(vt->vt.xft_fonts[vt->vt.current_font],
                                   vt->vt.xft_bold_fonts[vt->vt.current_font], first->bold)
                        : NULL;

                if (first->width == 0) {
                        ++column;
                } else if (first->width == 1 && first->text_length <= 1 &&
                           !XtpCairoFontIsColor(first_font)) {
                        char run[RUN_CAPACITY];
                        unsigned int start = column;
                        size_t length = 0;
                        size_t visible = 0;

                        while (column < end_column && length < sizeof(run)) {
                                const VisualCell *cell =
                                    &vt->vt
                                         .frame_cells[(size_t)row * vt->vt.frame_columns + column];

                                if (cell->width != 1 || cell->text_length > 1 ||
                                    !SameVisualStyle(cell, first))
                                        break;
                                run[length] = cell->text_length == 1 ? cell->text[0] : ' ';
                                if (cell->text_length == 1)
                                        visible = length + 1U;
                                ++length;
                                ++column;
                        }
                        {
                                XRectangle area;
                                int x = VtTerminalX(vt) + (int)start * (int)width;
                                int y = (int)vt->vt.internal_border + (int)row * (int)height;

                                area.x = (short)x;
                                area.y = (short)y;
                                area.width = (unsigned short)(length * width);
                                area.height = (unsigned short)height;
                                PaintVisualRun(vt, first, &area, x,
                                               y + VtSlotAscent(vt, vt->vt.current_font), run,
                                               visible, run, length, first_font, NULL,
                                               vt->vt.color_glyphs);
                                DrawDecorations(vt, first, &area);
                        }
                } else {
                        DrawVisualCell(vt, first, column, row);
                        column += first->width;
                }
        }
}

static void
DrawVisualRow(Vt100Rec *vt, unsigned int row)
{
        DrawVisualRowRange(vt, row, 0, vt->vt.frame_columns);
}

static void
SetCursorCell(Vt100Rec *vt, const VisualCell *cell)
{
        vt->vt.cursor_cell_seen = True;
        vt->vt.cursor_text_length = cell->text_length;
        vt->vt.cursor_width = cell->width;
        if (cell->text_length != 0)
                memcpy(vt->vt.cursor_text, cell->text, cell->text_length);
        vt->vt.cursor_fill =
            vt->vt.cursor_color != cell->background ? vt->vt.cursor_color : cell->foreground;
        vt->vt.cursor_text_color = cell->opaque_background;
        vt->vt.cursor_bold = cell->bold;
}

void
VtEraseLastCursor(Vt100Rec *vt)
{
        size_t index;

        if (!vt->vt.frame_valid || !vt->vt.last_cursor_visible ||
            vt->vt.last_cursor_column >= vt->vt.frame_columns ||
            vt->vt.last_cursor_row >= vt->vt.frame_rows)
                return;
        index = (size_t)vt->vt.last_cursor_row * vt->vt.frame_columns + vt->vt.last_cursor_column;
        DrawVisualCell(vt, &vt->vt.frame_cells[index], vt->vt.last_cursor_column,
                       vt->vt.last_cursor_row);
}

void
VtDrawCursor(Vt100Rec *vt, Boolean visible, unsigned int column, unsigned int row,
             XtpCursorShape shape)
{
        Widget widget = (Widget)vt;

        if (visible && !vt->vt.cursor_cell_seen && column < vt->vt.frame_columns &&
            row < vt->vt.frame_rows && vt->vt.frame_valid) {
                size_t index = (size_t)row * vt->vt.frame_columns + column;

                SetCursorCell(vt, &vt->vt.frame_cells[index]);
        }

        if (visible && column < vt->vt.frame_columns && row < vt->vt.frame_rows) {
                unsigned int width = VtSlotWidth(vt, vt->vt.current_font);
                unsigned int height = VtSlotHeight(vt, vt->vt.current_font);
                XRectangle area;
                int x = VtTerminalX(vt) + (int)column * (int)width;
                int y = (int)vt->vt.internal_border + (int)row * (int)height;

                area.x = (short)x;
                area.y = (short)y;
                area.width = (unsigned short)width;
                area.height = (unsigned short)height;
                if (!SetTextClip(vt, &area, NULL))
                        return;
                if (shape == XTP_CURSOR_SHAPE_BLOCK &&
                    (vt->vt.focused || vt->vt.always_highlight) && vt->vt.cursor_cell_seen) {
                        XSetForeground(XtDisplay(widget), vt->vt.gc, vt->vt.cursor_fill);
                        XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y, width,
                                       height);
                        if (vt->vt.cursor_text_length != 0) {
                                XRectangle glyph_area = area;
                                uint32_t base = 0;
                                XtpEmojiStyle style = XTP_EMOJI_STYLE_NONE;
                                XtpGlyphRun run = {0};
                                XftFont *font =
                                    vt->vt.use_xft
                                        ? SelectXftFont(vt, vt->vt.cursor_text,
                                                        vt->vt.cursor_text_length,
                                                        vt->vt.cursor_width, vt->vt.cursor_bold,
                                                        NULL, &base, &style, &run)
                                        : NULL;

                                glyph_area.width = (unsigned short)(vt->vt.cursor_width * width);
                                DrawTextClipped(
                                    vt, vt->vt.cursor_text_color, x,
                                    y + VtSlotAscent(vt, vt->vt.current_font), vt->vt.cursor_text,
                                    vt->vt.cursor_text_length, vt->vt.cursor_bold, font, &run,
                                    vt->vt.color_glyphs && style != XTP_EMOJI_STYLE_TEXT,
                                    &glyph_area, &area);
                        }
                } else if (shape == XTP_CURSOR_SHAPE_UNDERLINE || shape == XTP_CURSOR_SHAPE_BAR) {
                        unsigned int dimension =
                            shape == XTP_CURSOR_SHAPE_UNDERLINE ? height : width;
                        unsigned int thickness = dimension > 1 ? (dimension - 1U) / 8U : dimension;
                        Pixel cursor =
                            vt->vt.cursor_cell_seen ? vt->vt.cursor_fill : vt->vt.cursor_color;

                        if (thickness < 2U && dimension >= 2U)
                                thickness = 2U;
                        if (thickness > dimension)
                                thickness = dimension;
                        XSetForeground(XtDisplay(widget), vt->vt.gc, cursor);
                        if (shape == XTP_CURSOR_SHAPE_UNDERLINE)
                                XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x,
                                               y + (int)height - (int)thickness, width, thickness);
                        else
                                XFillRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y,
                                               thickness, height);
                } else {
                        Pixel outline =
                            vt->vt.cursor_cell_seen ? vt->vt.cursor_fill : vt->vt.cursor_color;

                        XSetForeground(XtDisplay(widget), vt->vt.gc, outline);
                        XDrawRectangle(XtDisplay(widget), XtWindow(widget), vt->vt.gc, x, y,
                                       width > 0 ? width - 1 : 0, height > 0 ? height - 1 : 0);
                }
                ClearTextClip(vt);
        }
        vt->vt.last_cursor_visible = visible;
        vt->vt.last_cursor_column = column;
        vt->vt.last_cursor_row = row;
        vt->vt.last_cursor_shape = shape;
}

void
VtStopCursorBlink(Vt100Rec *vt)
{
        if (vt->vt.cursor_blink_timer != (XtIntervalId)0) {
                XtRemoveTimeOut(vt->vt.cursor_blink_timer);
                vt->vt.cursor_blink_timer = (XtIntervalId)0;
        }
}

void VtScheduleCursorBlink(Vt100Rec *vt);

static void
CursorBlinkTick(XtPointer closure, XtIntervalId *timer)
{
        Vt100Rec *vt = closure;
        Boolean active;

        (void)timer;
        vt->vt.cursor_blink_timer = (XtIntervalId)0;
        if (!XtIsRealized((Widget)vt) || !vt->vt.cursor_protocol_visible ||
            !vt->vt.cursor_blinking || !vt->vt.frame_valid)
                return;

        active = vt->vt.focused || vt->vt.always_highlight;
        if (!active) {
                if (!vt->vt.last_cursor_visible) {
                        vt->vt.cursor_cell_seen = False;
                        vt->vt.cursor_text_length = 0;
                        VtDrawCursor(vt, True, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                                     vt->vt.last_cursor_shape);
                        XFlush(XtDisplay((Widget)vt));
                }
                vt->vt.cursor_blink_on = True;
        } else if (vt->vt.cursor_blink_on) {
                VtEraseLastCursor(vt);
                VtDrawCursor(vt, False, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                             vt->vt.last_cursor_shape);
                vt->vt.cursor_blink_on = False;
                XFlush(XtDisplay((Widget)vt));
        } else {
                vt->vt.cursor_cell_seen = False;
                vt->vt.cursor_text_length = 0;
                VtDrawCursor(vt, True, vt->vt.last_cursor_column, vt->vt.last_cursor_row,
                             vt->vt.last_cursor_shape);
                vt->vt.cursor_blink_on = True;
                XFlush(XtDisplay((Widget)vt));
        }
        XtpLog(XTP_LOG_DEBUG, "render", "cursor blink phase=%s column=%u row=%u",
               vt->vt.cursor_blink_on ? "on" : "off", vt->vt.last_cursor_column,
               vt->vt.last_cursor_row);
        VtScheduleCursorBlink(vt);
}

void
VtScheduleCursorBlink(Vt100Rec *vt)
{
        unsigned long delay;

        if (vt->vt.cursor_blink_timer != (XtIntervalId)0 || !vt->vt.cursor_protocol_visible ||
            !vt->vt.cursor_blinking || !XtIsRealized((Widget)vt))
                return;
        delay = (unsigned long)(vt->vt.cursor_blink_on ? vt->vt.cursor_on_time
                                                       : vt->vt.cursor_off_time);
        if (delay == 0)
                delay = 1;
        vt->vt.cursor_blink_timer =
            XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)vt), delay, CursorBlinkTick, vt);
}

void
VtRestartCursorBlink(Vt100Rec *vt)
{
        VtStopCursorBlink(vt);
        vt->vt.cursor_blink_on = vt->vt.last_cursor_visible || !vt->vt.cursor_protocol_visible;
        VtScheduleCursorBlink(vt);
}

static Boolean
SameVisualCell(const VisualCell *left, const VisualCell *right)
{
        return left->foreground == right->foreground && left->background == right->background &&
               left->opaque_background == right->opaque_background &&
               left->text_length == right->text_length && left->width == right->width &&
               left->bold == right->bold && left->underline == right->underline &&
               left->strikethrough == right->strikethrough && left->overline == right->overline &&
               memcmp(left->text, right->text, left->text_length) == 0;
}

static void
RenderBegin(const XtpRenderFrame *frame, void *closure)
{
        Vt100Rec *vt = closure;

        vt->vt.render_cursor_visible = frame->cursor_visible;
        vt->vt.render_reverse_colors = frame->reverse_colors ? True : False;
        vt->vt.render_cursor_column = frame->cursor_column;
        vt->vt.render_cursor_row = frame->cursor_row;
        vt->vt.cursor_cell_seen = False;
        vt->vt.cursor_text_length = 0;
        vt->vt.capture_full_frame =
            frame->full_repaint && EnsureFrameStorage(vt, frame->columns, frame->rows);
        if (vt->vt.capture_full_frame) {
                ResetVisualCells(vt, vt->vt.pending_cells, vt->vt.frame_capacity);
        } else {
                (void)EnsureFrameStorage(vt, frame->columns, frame->rows);
        }
}

static void
RenderCell(const XtpRenderCell *cell, void *closure)
{
        Vt100Rec *vt = closure;
        VisualCell visual = MakeVisualCell(vt, cell);
        size_t index = (size_t)cell->row * vt->vt.frame_columns + cell->column;

        if (vt->vt.render_cursor_visible && cell->column == vt->vt.render_cursor_column &&
            cell->row == vt->vt.render_cursor_row)
                SetCursorCell(vt, &visual);

        if (index < vt->vt.frame_capacity) {
                if (vt->vt.capture_full_frame) {
                        vt->vt.pending_cells[index] = visual;
                } else {
                        Boolean changed = !vt->vt.frame_valid ||
                                          !SameVisualCell(&vt->vt.frame_cells[index], &visual);
                        Boolean covered_by_cursor = vt->vt.last_cursor_visible &&
                                                    cell->column == vt->vt.last_cursor_column &&
                                                    cell->row == vt->vt.last_cursor_row;

                        vt->vt.frame_cells[index] = visual;
                        if (changed && !covered_by_cursor)
                                DrawVisualCell(vt, &visual, cell->column, cell->row);
                        vt->vt.frame_valid = True;
                }
        } else {
                DrawVisualCell(vt, &visual, cell->column, cell->row);
        }
}

static void
RenderEnd(const XtpRenderFrame *frame, void *closure)
{
        Vt100Rec *vt = closure;
        Boolean effective_blinking =
            VtEffectiveCursorBlink(vt->vt.cursor_blink_policy, frame->cursor_blinking);
        Boolean cursor_changed =
            vt->vt.cursor_protocol_visible != frame->cursor_visible ||
            (frame->cursor_visible && (vt->vt.last_cursor_column != frame->cursor_column ||
                                       vt->vt.last_cursor_row != frame->cursor_row ||
                                       vt->vt.last_cursor_shape != frame->cursor_shape));
        Boolean blinking_changed = vt->vt.cursor_blinking != effective_blinking;
        Boolean refresh_cursor = vt->vt.capture_full_frame || cursor_changed || blinking_changed ||
                                 vt->vt.cursor_cell_seen;

        if (!vt->vt.capture_full_frame)
                XtpLog(XTP_LOG_DEBUG, "render",
                       "cursor transition old=%s@%u,%u/%d blink=%s new=%s@%u,%u/%d "
                       "blink-requested=%s blink-effective=%s changed=%s cell=%s",
                       vt->vt.cursor_protocol_visible ? "visible" : "hidden",
                       vt->vt.last_cursor_column, vt->vt.last_cursor_row, vt->vt.last_cursor_shape,
                       vt->vt.cursor_blinking ? "true" : "false",
                       frame->cursor_visible ? "visible" : "hidden", frame->cursor_column,
                       frame->cursor_row, frame->cursor_shape,
                       frame->cursor_blinking ? "true" : "false",
                       effective_blinking ? "true" : "false",
                       cursor_changed || blinking_changed ? "true" : "false",
                       vt->vt.cursor_cell_seen ? "dirty" : "clean");

        if (cursor_changed || blinking_changed) {
                VtStopCursorBlink(vt);
                vt->vt.cursor_blink_on = True;
        }

        if (vt->vt.capture_full_frame) {
                unsigned int row;
                {
                        VisualCell *temporary = vt->vt.frame_cells;

                        vt->vt.frame_cells = vt->vt.pending_cells;
                        vt->vt.pending_cells = temporary;
                }
                vt->vt.frame_valid = True;
                for (row = 0; row < vt->vt.frame_rows; ++row)
                        DrawVisualRow(vt, row);
        }

        /*
         * Restore the old cursor cell after partial updates have refreshed the
         * frame cache. Erasing it in RenderBegin briefly paints the stale
         * pre-keypress cell before Xft draws the new glyph, which is visible as
         * a flash while typing. A full-frame repaint overwrites the old cursor
         * as it walks the rows, so it does not need a separate erase.
         */
        if (!vt->vt.capture_full_frame &&
            (cursor_changed || blinking_changed || vt->vt.cursor_cell_seen))
                VtEraseLastCursor(vt);

        vt->vt.cursor_protocol_visible = frame->cursor_visible;
        vt->vt.cursor_blink_requested = frame->cursor_blinking;
        vt->vt.cursor_blinking = effective_blinking;
        if (refresh_cursor) {
                Boolean draw_cursor =
                    frame->cursor_visible && (!effective_blinking || vt->vt.cursor_blink_on);

                VtDrawCursor(vt, draw_cursor, frame->cursor_column, frame->cursor_row,
                             frame->cursor_shape);
                if (cursor_changed && !vt->vt.capture_full_frame && !vt->vt.cursor_cell_seen)
                        XtpLog(XTP_LOG_DEBUG, "render", "cursor-only repaint column=%u row=%u",
                               frame->cursor_column, frame->cursor_row);
        }
        if (frame->cursor_visible && effective_blinking)
                VtScheduleCursorBlink(vt);
        else
                VtStopCursorBlink(vt);
        VtUpdateScrollbar(vt);
}

int
VtRenderTerminal(Vt100Rec *vt, Boolean force_full)
{
        static const XtpRenderer renderer = {
            RenderBegin,
            RenderCell,
            RenderEnd,
        };

        if (vt->vt.terminal == NULL)
                return -1;
        return XtpTerminalRender(vt->vt.terminal, &renderer, vt, force_full != False);
}

void
VtRepaintCached(Vt100Rec *vt, const XRectangle *damage)
{
        unsigned int cell_width = VtSlotWidth(vt, vt->vt.current_font);
        unsigned int cell_height = VtSlotHeight(vt, vt->vt.current_font);
        XRectangle grid;
        XRectangle clipped;
        unsigned int first_row;
        unsigned int end_row;
        unsigned int row;

        if (!vt->vt.frame_valid || vt->vt.frame_columns == 0 || vt->vt.frame_rows == 0)
                return;
        grid.x = (short)VtTerminalX(vt);
        grid.y = (short)vt->vt.internal_border;
        grid.width = (unsigned short)(vt->vt.frame_columns * cell_width);
        grid.height = (unsigned short)(vt->vt.frame_rows * cell_height);
        if (damage != NULL) {
                if (!IntersectRectangles(&grid, damage, &clipped))
                        return;
        } else {
                clipped = grid;
        }

        first_row = (unsigned int)(clipped.y - grid.y) / cell_height;
        end_row =
            ((unsigned int)(clipped.y - grid.y) + clipped.height + cell_height - 1U) / cell_height;
        if (end_row > vt->vt.frame_rows)
                end_row = vt->vt.frame_rows;
        vt->vt.damage_clip_active = True;
        vt->vt.damage_clip = clipped;
        for (row = first_row; row < end_row; ++row) {
                unsigned int first_column = (unsigned int)(clipped.x - grid.x) / cell_width;
                unsigned int end_column =
                    ((unsigned int)(clipped.x - grid.x) + clipped.width + cell_width - 1U) /
                    cell_width;
                VisualCell *cells = vt->vt.frame_cells + (size_t)row * vt->vt.frame_columns;

                if (end_column > vt->vt.frame_columns)
                        end_column = vt->vt.frame_columns;
                if (first_column > 0 && cells[first_column].width == 0)
                        --first_column;
                if (end_column != 0 && cells[end_column - 1U].width > 1U &&
                    end_column - 1U + cells[end_column - 1U].width > end_column)
                        end_column = end_column - 1U + cells[end_column - 1U].width;
                if (end_column > vt->vt.frame_columns)
                        end_column = vt->vt.frame_columns;
                DrawVisualRowRange(vt, row, first_column, end_column);
        }
        if (vt->vt.last_cursor_visible) {
                XRectangle cursor;
                XRectangle intersection;

                cursor.x = (short)(grid.x + (int)vt->vt.last_cursor_column * (int)cell_width);
                cursor.y = (short)(grid.y + (int)vt->vt.last_cursor_row * (int)cell_height);
                cursor.width = (unsigned short)cell_width;
                cursor.height = (unsigned short)cell_height;
                if (IntersectRectangles(&cursor, &clipped, &intersection)) {
                        unsigned int column = vt->vt.last_cursor_column;
                        unsigned int cursor_row = vt->vt.last_cursor_row;

                        vt->vt.cursor_cell_seen = False;
                        vt->vt.cursor_text_length = 0;
                        VtDrawCursor(vt, True, column, cursor_row, vt->vt.last_cursor_shape);
                }
        }
        vt->vt.damage_clip_active = False;
}

void
VtPlaceholder(Vt100Rec *vt)
{
        static const char *const lines[] = {
            "xterm+",
            "UI-only build: configure with -Dlibghostty=enabled",
        };
        int x = VtTerminalX(vt);
        int y = (int)vt->vt.internal_border + VtSlotAscent(vt, vt->vt.current_font);
        size_t line;

        XSetForeground(XtDisplay((Widget)vt), vt->vt.gc, vt->core.background_pixel);
        XFillRectangle(XtDisplay((Widget)vt), XtWindow((Widget)vt), vt->vt.gc, 0, 0, vt->core.width,
                       vt->core.height);
        for (line = 0; line < XtNumber(lines); ++line) {
                size_t length = strlen(lines[line]);

                DrawText(vt, vt->vt.foreground, x, y, lines[line], length, False, NULL);
                y += (int)VtSlotHeight(vt, vt->vt.current_font);
        }
}

void
VtRedisplay(Widget widget, XEvent *event, Region region)
{
        Vt100Rec *vt = VtAsRecord(widget);
        XRectangle damage;
        Boolean have_damage = False;

        XtpLog(XTP_LOG_DEBUG, "render", "expose pixels=%ux%u grid=%dx%d font-slot=%d cell=%ux%u",
               vt->core.width, vt->core.height, vt->vt.columns, vt->vt.rows, vt->vt.current_font,
               XtpVtCellWidth(widget), XtpVtCellHeight(widget));
        if (!vt->vt.frame_valid) {
                if (VtRenderTerminal(vt, True) != 0)
                        VtPlaceholder(vt);
                return;
        }
        if (region != NULL) {
                XClipBox(region, &damage);
                have_damage = damage.width != 0 && damage.height != 0;
        } else if (event != NULL && event->type == Expose) {
                damage.x = (short)event->xexpose.x;
                damage.y = (short)event->xexpose.y;
                damage.width = (unsigned short)event->xexpose.width;
                damage.height = (unsigned short)event->xexpose.height;
                have_damage = damage.width != 0 && damage.height != 0;
        }
        if (have_damage)
                VtRepaintCached(vt, &damage);
        else
                VtRepaintCached(vt, NULL);
}
