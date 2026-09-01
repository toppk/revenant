#include "font_role.h"

#include <string.h>

unsigned int
XtpFontStyleIndex(Boolean bold, Boolean italic)
{
        return (bold ? 1U : 0U) | (italic ? 2U : 0U);
}

const char *
XtpFontStyleName(Boolean bold, Boolean italic)
{
        if (bold && italic)
                return "bold-italic";
        if (bold)
                return "bold";
        if (italic)
                return "italic";
        return "normal";
}

const char *
XtpFontPatternFamily(const FcPattern *pattern)
{
        FcChar8 *family = NULL;

        if (pattern == NULL || FcPatternGetString(pattern, FC_FAMILY, 0, &family) != FcResultMatch)
                return "(unknown)";
        return (const char *)family;
}

Boolean
XtpFontSameFamily(const FcPattern *left, const FcPattern *right)
{
        const char *left_family = XtpFontPatternFamily(left);
        const char *right_family = XtpFontPatternFamily(right);

        return strcmp(left_family, "(unknown)") != 0 && strcmp(right_family, "(unknown)") != 0 &&
               FcStrCmpIgnoreCase((const FcChar8 *)left_family, (const FcChar8 *)right_family) == 0;
}

Boolean
XtpFontStyleIsReal(XftFont *normal, XftFont *font, Boolean bold, Boolean italic)
{
        FcBool embolden = FcFalse;
        FcMatrix *matrix = NULL;
        int weight = FC_WEIGHT_REGULAR;
        int slant = FC_SLANT_ROMAN;

        if (normal == NULL || font == NULL || normal->pattern == NULL || font->pattern == NULL ||
            !XtpFontSameFamily(normal->pattern, font->pattern))
                return False;
        if (FcPatternGetBool(font->pattern, FC_EMBOLDEN, 0, &embolden) == FcResultMatch && embolden)
                return False;
        if (FcPatternGetMatrix(font->pattern, FC_MATRIX, 0, &matrix) == FcResultMatch &&
            matrix != NULL &&
            (matrix->xx != 1.0 || matrix->xy != 0.0 || matrix->yx != 0.0 || matrix->yy != 1.0))
                return False;
        if (bold && (FcPatternGetInteger(font->pattern, FC_WEIGHT, 0, &weight) != FcResultMatch ||
                     weight < FC_WEIGHT_DEMIBOLD))
                return False;
        if (italic && (FcPatternGetInteger(font->pattern, FC_SLANT, 0, &slant) != FcResultMatch ||
                       slant == FC_SLANT_ROMAN))
                return False;
        return True;
}

XftFont *
XtpFontRoleSelect(XftFont *normal, XftFont *bold_font, XftFont *italic_font,
                  XftFont *bold_italic_font, Boolean bold, Boolean italic)
{
        if (bold && italic && bold_italic_font != NULL)
                return bold_italic_font;
        if (italic && italic_font != NULL)
                return italic_font;
        if (bold && bold_font != NULL)
                return bold_font;
        return normal;
}

const char *
XtpFontSlantName(XftFont *font)
{
        int slant = FC_SLANT_ROMAN;

        if (font == NULL || font->pattern == NULL ||
            FcPatternGetInteger(font->pattern, FC_SLANT, 0, &slant) != FcResultMatch)
                return "unknown";
        if (slant >= FC_SLANT_OBLIQUE)
                return "oblique";
        if (slant >= FC_SLANT_ITALIC)
                return "italic";
        return "roman";
}

const char *
XtpFontFileName(XftFont *font)
{
        FcChar8 *file = NULL;

        if (font == NULL || font->pattern == NULL ||
            FcPatternGetString(font->pattern, FC_FILE, 0, &file) != FcResultMatch)
                return "(unknown)";
        return (const char *)file;
}

int
XtpFontCollectionIndex(XftFont *font)
{
        int index = 0;

        if (font != NULL && font->pattern != NULL)
                (void)FcPatternGetInteger(font->pattern, FC_INDEX, 0, &index);
        return index;
}

Boolean
XtpGlyphRunIsPositioned(const XtpGlyphRun *run)
{
        unsigned int index;

        if (run == NULL)
                return False;
        for (index = 0; index < run->count; ++index) {
                if (run->glyphs[index].x_offset != 0 || run->glyphs[index].y_offset != 0 ||
                    run->glyphs[index].y_advance != 0 || run->glyphs[index].x_advance == 0)
                        return True;
        }
        return False;
}
