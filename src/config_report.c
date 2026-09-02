#include "config_report.h"

#include "ansi_palette.h"
#include "command_options.h"
#include "diagnostics.h"
#include "resource_catalog.h"
#include "sme_slider.h"
#include "vt_widget.h"

#include <fontconfig/fontconfig.h>

#include <X11/StringDefs.h>
#include <X11/Xft/Xft.h>
#include <X11/Shell.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum
{
        ORIGIN_COMMAND_LINE,
        ORIGIN_X_RESOURCES,
        ORIGIN_COMPILED_DEFAULT,
        ORIGIN_UNSET,
} Origin;

typedef struct
{
        char *value;
        Origin origin;
} Resolved;

typedef struct
{
        const char *resource;
        const char *class_name;
        const char *default_value;
} ResourceSpec;

typedef struct
{
        XrmDatabase merged;
        XrmDatabase server;
        XrmDatabase command;
        const char *application_name;
        const char *application_class;
} ReportContext;

typedef struct
{
        const char *label;
        const char *resource;
        XtpFontSlotInfo loaded;
        double point_size;
        unsigned long area;
        int order;
} FontChoice;

static const char *const reset = "\033[0m";
static bool use_color;

typedef struct
{
        const char *name;
        const char *class_name;
} ResourceProbe;

static const ResourceProbe resource_probes[] = {
    {"xterm.geometry", "XTerm.Geometry"},
    {"xterm.menuLocale", "XTerm.MenuLocale"},
    {"xterm.logLevel", "XTerm.LogLevel"},
    {"xterm.debug", "XTerm.Debug"},
    {"xterm.vt100.background", "XTerm.VT100.Background"},
    {"xterm.vt100.backgroundOpacity", "XTerm.VT100.BackgroundOpacity"},
    {"xterm.vt100.foreground", "XTerm.VT100.Foreground"},
    {"xterm.vt100.font", "XTerm.VT100.Font"},
    {"xterm.vt100.font1", "XTerm.VT100.Font1"},
    {"xterm.vt100.font2", "XTerm.VT100.Font2"},
    {"xterm.vt100.font3", "XTerm.VT100.Font3"},
    {"xterm.vt100.font4", "XTerm.VT100.Font4"},
    {"xterm.vt100.font5", "XTerm.VT100.Font5"},
    {"xterm.vt100.font6", "XTerm.VT100.Font6"},
    {"xterm.vt100.font7", "XTerm.VT100.Font7"},
    {"xterm.vt100.faceName", "XTerm.VT100.FaceName"},
    {"xterm.vt100.faceNameDoublesize", "XTerm.VT100.FaceNameDoublesize"},
    {"xterm.vt100.faceNameEmoji", "XTerm.VT100.FaceNameEmoji"},
    {"xterm.vt100.faceNameHan", "XTerm.VT100.FaceNameHan"},
    {"xterm.vt100.boldFont", "XTerm.VT100.BoldFont"},
    {"xterm.vt100.wideBoldFont", "XTerm.VT100.WideBoldFont"},
    {"xterm.vt100.emojiPresentation", "XTerm.VT100.EmojiPresentation"},
    {"xterm.vt100.graphemeWidth", "XTerm.VT100.GraphemeWidth"},
    {"xterm.vt100.colorGlyphs", "XTerm.VT100.ColorGlyphs"},
    {"xterm.vt100.reportFontRouting", "XTerm.VT100.ReportFontRouting"},
    {"xterm.vt100.faceSize", "XTerm.VT100.FaceSize"},
    {"xterm.vt100.renderFont", "XTerm.VT100.RenderFont"},
    {"xterm.vt100.internalBorder", "XTerm.VT100.BorderWidth"},
    {"xterm.vt100.saveLines", "XTerm.VT100.SaveLines"},
    {"xterm.vt100.scrollBar", "XTerm.VT100.ScrollBar"},
    {"xterm.vt100.rightScrollBar", "XTerm.VT100.RightScrollBar"},
    {"xterm.vt100.cursorColor", "XTerm.VT100.CursorColor"},
    {"xterm.vt100.alwaysHighlight", "XTerm.VT100.AlwaysHighlight"},
    {"xterm.vt100.cursorBlink", "XTerm.VT100.CursorBlink"},
    {"xterm.vt100.cursorBlinkXOR", "XTerm.VT100.CursorBlinkXOR"},
    {"xterm.vt100.cursorOnTime", "XTerm.VT100.CursorOnTime"},
    {"xterm.vt100.cursorOffTime", "XTerm.VT100.CursorOffTime"},
    {"xterm.vt100.pointerColor", "XTerm.VT100.PointerColor"},
    {"xterm.vt100.pointerShape", "XTerm.VT100.PointerShape"},
    {"xterm.vt100.color0", "XTerm.VT100.Color0"},
    {"xterm.vt100.color1", "XTerm.VT100.Color1"},
    {"xterm.vt100.color2", "XTerm.VT100.Color2"},
    {"xterm.vt100.color3", "XTerm.VT100.Color3"},
    {"xterm.vt100.color4", "XTerm.VT100.Color4"},
    {"xterm.vt100.color5", "XTerm.VT100.Color5"},
    {"xterm.vt100.color6", "XTerm.VT100.Color6"},
    {"xterm.vt100.color7", "XTerm.VT100.Color7"},
    {"xterm.vt100.color8", "XTerm.VT100.Color8"},
    {"xterm.vt100.color9", "XTerm.VT100.Color9"},
    {"xterm.vt100.color10", "XTerm.VT100.Color10"},
    {"xterm.vt100.color11", "XTerm.VT100.Color11"},
    {"xterm.vt100.color12", "XTerm.VT100.Color12"},
    {"xterm.vt100.color13", "XTerm.VT100.Color13"},
    {"xterm.vt100.color14", "XTerm.VT100.Color14"},
    {"xterm.vt100.color15", "XTerm.VT100.Color15"},
};

static char *QualifiedResource(const char *path, const char *default_root, const char *actual_root);

static bool
RelevantServerResource(const char *line, const char *application_name,
                       const char *application_class)
{
        return strncmp(line, application_name, strlen(application_name)) == 0 ||
               strncmp(line, application_class, strlen(application_class)) == 0 ||
               strncmp(line, "Xft.", 4) == 0 || strncmp(line, "Xcursor.", 8) == 0;
}

void
XtpLogResourceDatabases(Display *display, const char *application_name,
                        const char *application_class)
{
        const char *manager = XResourceManagerString(display);
        char *copy = manager != NULL ? strdup(manager) : NULL;
        char *line;
        char *state = NULL;
        size_t probe;
        XrmDatabase database = XtDatabase(display);

        XtpLog(XTP_LOG_DEBUG, "xresource", "RESOURCE_MANAGER present=%s",
               manager != NULL ? "true" : "false");
        for (line = copy != NULL ? strtok_r(copy, "\n", &state) : NULL; line != NULL;
             line = strtok_r(NULL, "\n", &state)) {
                if (RelevantServerResource(line, application_name, application_class))
                        XtpLog(XTP_LOG_DEBUG, "xresource", "server %s", line);
        }
        free(copy);

        for (probe = 0; probe < XtNumber(resource_probes); ++probe) {
                XrmValue value;
                String type = NULL;
                char *resource_name = QualifiedResource(resource_probes[probe].name,
                                                        XTP_APPLICATION_NAME, application_name);
                char *class_name = QualifiedResource(resource_probes[probe].class_name,
                                                     XTP_APPLICATION_CLASS, application_class);

                if (resource_name != NULL && class_name != NULL &&
                    XrmGetResource(database, resource_name, class_name, &type, &value)) {
                        int length = (int)value.size;

                        if (length > 0 && ((const char *)value.addr)[length - 1] == '\0')
                                --length;
                        XtpLog(XTP_LOG_DEBUG, "xresource",
                               "merged name=%s class=%s type=%s value=%.*s", resource_name,
                               class_name, type != NULL ? type : "(null)", length,
                               (const char *)value.addr);
                }
                free(resource_name);
                free(class_name);
        }
        XtpLog(XTP_LOG_INFO, "config",
               "resource precedence effective=command-line > server RESOURCE_MANAGER > "
               "app-defaults/fallbacks > compiled resource defaults");
        XtpLog(XTP_LOG_INFO, "compat",
               "renderer is resolved by the VT100 widget; cursorColor and color0..color15 are "
               "applied; pointerColor and pointerShape are merged but not applied yet");
        XtpLog(XTP_LOG_INFO, "scrollback",
               "saveLines, scrollbar visibility/side, wheel navigation, thumb dragging, and "
               "scroll-back/scroll-forw actions are active");
}

static const char *
OriginName(Origin origin)
{
        switch (origin) {
        case ORIGIN_COMMAND_LINE:
                return "command line";
        case ORIGIN_X_RESOURCES:
                return "X resources";
        case ORIGIN_COMPILED_DEFAULT:
                return "compiled default";
        case ORIGIN_UNSET:
                return "unset";
        }
        return "unset";
}

static const char *
OriginColor(Origin origin)
{
        switch (origin) {
        case ORIGIN_COMMAND_LINE:
                return "\033[96m";
        case ORIGIN_X_RESOURCES:
                return "\033[92m";
        case ORIGIN_COMPILED_DEFAULT:
                return "\033[95m";
        case ORIGIN_UNSET:
                return "\033[90m";
        }
        return "";
}

static char *
QualifiedResource(const char *path, const char *default_root, const char *actual_root)
{
        size_t default_length = strlen(default_root);
        size_t actual_length;
        size_t path_length;
        char *qualified;

        if (strncmp(path, default_root, default_length) != 0 ||
            (path[default_length] != '.' && path[default_length] != '\0'))
                return strdup(path);
        actual_length = strlen(actual_root);
        path_length = strlen(path);
        qualified = malloc(actual_length + path_length - default_length + 1U);
        if (qualified == NULL)
                return NULL;
        memcpy(qualified, actual_root, actual_length);
        memcpy(qualified + actual_length, path + default_length, path_length - default_length + 1U);
        return qualified;
}

static char *
DatabaseValue(XrmDatabase database, const ResourceSpec *spec, const ReportContext *context)
{
        XrmValue value;
        char *type = NULL;
        size_t length;
        char *copy;
        char *resource;
        char *class_name;

        if (database == NULL)
                return NULL;
        resource =
            QualifiedResource(spec->resource, XTP_APPLICATION_NAME, context->application_name);
        class_name =
            QualifiedResource(spec->class_name, XTP_APPLICATION_CLASS, context->application_class);
        if (resource == NULL || class_name == NULL ||
            !XrmGetResource(database, resource, class_name, &type, &value) || value.addr == NULL) {
                free(resource);
                free(class_name);
                return NULL;
        }
        free(resource);
        free(class_name);
        length = value.size;
        if (length != 0 && ((const char *)value.addr)[length - 1] == '\0')
                --length;
        copy = malloc(length + 1U);
        if (copy == NULL)
                return NULL;
        memcpy(copy, value.addr, length);
        copy[length] = '\0';
        return copy;
}

static Resolved
ResolveResource(const ReportContext *context, const ResourceSpec *spec)
{
        Resolved result = {NULL, ORIGIN_UNSET};
        char *probe = DatabaseValue(context->command, spec, context);

        result.value = DatabaseValue(context->merged, spec, context);
        if (probe != NULL) {
                result.origin = ORIGIN_COMMAND_LINE;
                if (result.value == NULL)
                        result.value = probe;
                else
                        free(probe);
        } else {
                probe = DatabaseValue(context->server, spec, context);
                if (probe != NULL) {
                        result.origin = ORIGIN_X_RESOURCES;
                        if (result.value == NULL)
                                result.value = probe;
                        else
                                free(probe);
                } else if (result.value != NULL) {
                        result.origin = ORIGIN_X_RESOURCES;
                } else if (spec->default_value != NULL) {
                        result.value = strdup(spec->default_value);
                        result.origin = ORIGIN_COMPILED_DEFAULT;
                }
        }
        return result;
}

static void
PrintComment(Origin origin, const char *support, const char *text)
{
        if (use_color)
                printf("%s! [%s] [%s] %s%s\n", OriginColor(origin), OriginName(origin), support,
                       text, reset);
        else
                printf("! [%s] [%s] %s\n", OriginName(origin), support, text);
}

static void
PrintResolved(const char *display_name, const char *support, const char *explanation,
              const Resolved *resolved)
{
        PrintComment(resolved->origin, support, explanation);
        if (use_color)
                fputs(OriginColor(resolved->origin), stdout);
        if (resolved->origin == ORIGIN_UNSET || resolved->value == NULL)
                printf("! %s:\t<unset>", display_name);
        else
                printf("%s:\t%s", display_name, resolved->value);
        if (use_color)
                fputs(reset, stdout);
        fputc('\n', stdout);
}

XrmDatabase
XtpConfigCommandDatabase(int argc, char **argv, const char *application_name)
{
        XrmDatabase database = NULL;
        XrmOptionDescRec *options;
        char **arguments;
        int option_count = XtpCommandOptionCount + 1;
        int parse_argc = argc;

        options = malloc((size_t)option_count * sizeof(*options));
        arguments = malloc(((size_t)parse_argc + 1U) * sizeof(*arguments));
        if (options == NULL || arguments == NULL) {
                free(options);
                free(arguments);
                return NULL;
        }
        memcpy(options, XtpCommandOptions, (size_t)XtpCommandOptionCount * sizeof(*options));
        options[XtpCommandOptionCount] = (XrmOptionDescRec){"-xrm", NULL, XrmoptionResArg, NULL};
        memcpy(arguments, argv, (size_t)parse_argc * sizeof(*arguments));
        arguments[parse_argc] = NULL;

        XrmInitialize();
        XrmParseCommand(&database, options, option_count, application_name, &parse_argc, arguments);
        free(arguments);
        free(options);
        return database;
}

static double
NumberValue(const Resolved *resolved, double fallback)
{
        char *end = NULL;
        double value;

        if (resolved->value == NULL)
                return fallback;
        value = strtod(resolved->value, &end);
        return end != resolved->value && value > 0.0 ? value : fallback;
}

static char *
PrimaryFontconfigFace(const char *configured)
{
        char *copy;
        char *item;
        char *state = NULL;

        if (configured == NULL)
                return NULL;
        copy = strdup(configured);
        if (copy == NULL)
                return NULL;
        for (item = strtok_r(copy, ",", &state); item != NULL; item = strtok_r(NULL, ",", &state)) {
                char *end;

                while (isspace((unsigned char)*item))
                        ++item;
                end = item + strlen(item);
                while (end > item && isspace((unsigned char)end[-1]))
                        *--end = '\0';
                if (strncmp(item, "x:", 2) == 0 || strncmp(item, "x11:", 4) == 0)
                        continue;
                if (strncmp(item, "xft:", 4) == 0)
                        item += 4;
                item = strdup(item);
                free(copy);
                return item;
        }
        free(copy);
        return NULL;
}

static void
PrintFontconfigMatch(Display *display, int screen, const char *face, double point_size)
{
        FcPattern *pattern;
        FcPattern *match;
        FcResult result;
        FcChar8 *family = NULL;
        FcChar8 *style = NULL;
        FcChar8 *file = NULL;

        if (face == NULL)
                return;
        pattern = FcNameParse((const FcChar8 *)face);
        if (pattern == NULL)
                return;
        FcPatternAddDouble(pattern, FC_SIZE, point_size);
        FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(display, screen, pattern);
        match = FcFontMatch(NULL, pattern, &result);
        if (match != NULL) {
                (void)FcPatternGetString(match, FC_FAMILY, 0, &family);
                (void)FcPatternGetString(match, FC_STYLE, 0, &style);
                (void)FcPatternGetString(match, FC_FILE, 0, &file);
                printf("! fontconfig match: %s / %s\n",
                       family != NULL ? (const char *)family : "unknown family",
                       style != NULL ? (const char *)style : "unknown style");
                printf("! font file: %s\n", file != NULL ? (const char *)file : "unknown");
                FcPatternDestroy(match);
        }
        FcPatternDestroy(pattern);
}

static int
CompareFontChoice(const void *left, const void *right)
{
        const FontChoice *a = left;
        const FontChoice *b = right;

        if (a->point_size > 0.0 && b->point_size > 0.0) {
                if (a->point_size < b->point_size)
                        return -1;
                if (a->point_size > b->point_size)
                        return 1;
        }
        if (a->area < b->area)
                return -1;
        if (a->area > b->area)
                return 1;
        return strcmp(a->label, b->label);
}

static void
ReportFonts(const ReportContext *context, Widget vt)
{
        static const char *const labels[8] = {
            "Default", "Unreadable", "Tiny", "Small", "Medium", "Large", "Huge", "Enormous",
        };
        static const char *const resource_names[8] = {
            "font", "font1", "font2", "font3", "font4", "font5", "font6", "font7",
        };
        static const char *const defaults[8] = {
            "fixed", "nil2", "5x7", "6x10", "7x13", "9x15", "10x20", "12x24",
        };
        ResourceSpec render_spec = {
            "xterm.vt100.renderFont",
            "XTerm.VT100.RenderFont",
            "default",
        };
        ResourceSpec face_spec = {
            "xterm.vt100.faceName",
            "XTerm.VT100.FaceName",
            NULL,
        };
        ResourceSpec double_spec = {
            "xterm.vt100.faceNameDoublesize",
            "XTerm.VT100.FaceNameDoublesize",
            NULL,
        };
        ResourceSpec size_spec = {
            "xterm.vt100.faceSize",
            "XTerm.VT100.FaceSize",
            "8.0",
        };
        ResourceSpec emoji_spec = {
            "xterm.vt100.faceNameEmoji",
            "XTerm.VT100.FaceNameEmoji",
            NULL,
        };
        ResourceSpec han_spec = {
            "xterm.vt100.faceNameHan",
            "XTerm.VT100.FaceNameHan",
            NULL,
        };
        ResourceSpec bold_spec = {
            "xterm.vt100.boldFont",
            "XTerm.VT100.BoldFont",
            NULL,
        };
        ResourceSpec wide_bold_spec = {
            "xterm.vt100.wideBoldFont",
            "XTerm.VT100.WideBoldFont",
            NULL,
        };
        ResourceSpec presentation_spec = {
            "xterm.vt100.emojiPresentation",
            "XTerm.VT100.EmojiPresentation",
            "unicode",
        };
        ResourceSpec grapheme_width_spec = {
            "xterm.vt100.graphemeWidth",
            "XTerm.VT100.GraphemeWidth",
            "legacy",
        };
        ResourceSpec color_glyphs_spec = {
            "xterm.vt100.colorGlyphs",
            "XTerm.VT100.ColorGlyphs",
            "true",
        };
        ResourceSpec system_fallback_spec = {
            "xterm.vt100.systemFallback",
            "XTerm.VT100.SystemFallback",
            "true",
        };
        ResourceSpec report_routing_spec = {
            "xterm.vt100.reportFontRouting",
            "XTerm.VT100.ReportFontRouting",
            "false",
        };
        Resolved render = ResolveResource(context, &render_spec);
        Resolved face = ResolveResource(context, &face_spec);
        Resolved double_face = ResolveResource(context, &double_spec);
        Resolved base_size = ResolveResource(context, &size_spec);
        Resolved emoji_face = ResolveResource(context, &emoji_spec);
        Resolved han_face = ResolveResource(context, &han_spec);
        Resolved bold_face = ResolveResource(context, &bold_spec);
        Resolved wide_bold_face = ResolveResource(context, &wide_bold_spec);
        Resolved presentation = ResolveResource(context, &presentation_spec);
        Resolved grapheme_width = ResolveResource(context, &grapheme_width_spec);
        Resolved color_glyphs = ResolveResource(context, &color_glyphs_spec);
        Resolved system_fallback = ResolveResource(context, &system_fallback_spec);
        Resolved report_routing = ResolveResource(context, &report_routing_spec);
        FontChoice choices[8];
        FontChoice ordered[8];
        double base_points = NumberValue(&base_size, 8.0);
        unsigned long base_area;
        char *primary;
        int slot;

        puts("\n! ----------------------------------------------------------------------");
        puts("! Renderer and modern font selection");
        puts("! renderFont=true asks xterm to use Xft/fontconfig.  faceName chooses the");
        puts("! scalable family and faceSize chooses its Default-menu point size.");
        PrintResolved("XTerm*renderFont", "supported", "TrueType/Xft renderer requested state.",
                      &render);
        PrintResolved("XTerm*faceName", "supported (primary face)",
                      "Primary two-entry chain; entry 1 is the metrics authority.", &face);
        PrintResolved("XTerm*faceSize", "supported", "Point size for the Default font-menu entry.",
                      &base_size);
        PrintResolved("XTerm*faceNameDoublesize", "supported",
                      "Preferred face for wide text and the compatibility emoji fallback.",
                      &double_face);
        PrintResolved("XTerm*faceNameEmoji", "supported",
                      "Preferred face for Unicode emoji presentation.", &emoji_face);
        PrintResolved("XTerm*faceNameHan", "supported",
                      "Script=Han text-presentation role; excludes Script_Extensions.", &han_face);
        PrintResolved("XTerm*boldFont", "supported",
                      "Explicit xft: entries supply same-family primary bold instances.",
                      &bold_face);
        PrintResolved("XTerm*wideBoldFont", "supported",
                      "Explicit xft: entries supply same-family doublesize bold instances.",
                      &wide_bold_face);
        PrintResolved("XTerm*emojiPresentation", "supported",
                      "Selector-less policy: unicode, text, or emoji.", &presentation);
        PrintResolved("XTerm*graphemeWidth", "supported",
                      "Initial/RIS width contract: legacy or unicode (mode 2027 on).",
                      &grapheme_width);
        PrintResolved("XTerm*colorGlyphs", "supported",
                      "Color-glyph policy; false uses real outline bases and rejects empty ones.",
                      &color_glyphs);
        PrintResolved("XTerm*systemFallback", "supported",
                      "Whether unnamed fontconfig candidates follow explicitly named fonts.",
                      &system_fallback);
        PrintResolved("XTerm*reportFontRouting", "supported",
                      "Collect bounded route records for report-font-routing() snapshots.",
                      &report_routing);
        for (slot = 0; slot < 16; ++slot) {
                char resource_name[64];
                char class_name[64];
                char label[32];
                ResourceSpec fallback_spec;
                Resolved fallback;

                snprintf(resource_name, sizeof(resource_name), "xterm.vt100.fallbackFace%d",
                         slot + 1);
                snprintf(class_name, sizeof(class_name), "XTerm.VT100.FallbackFace%d", slot + 1);
                snprintf(label, sizeof(label), "XTerm*fallbackFace%d", slot + 1);
                fallback_spec.resource = resource_name;
                fallback_spec.class_name = class_name;
                fallback_spec.default_value = NULL;
                fallback = ResolveResource(context, &fallback_spec);
                if (fallback.origin != ORIGIN_UNSET)
                        PrintResolved(label, "supported",
                                      "Ordered user fallback before unnamed system candidates.",
                                      &fallback);
                free(fallback.value);
        }
        primary = PrimaryFontconfigFace(face.value);
        PrintFontconfigMatch(XtDisplay(vt), XScreenNumberOfScreen(XtScreen(vt)), primary,
                             base_points);
        free(primary);
        printf("! active xterm+ renderer: %s\n", XtpVtRendererName(vt));
        puts("! Xft routes wide text and emoji presentation through their configured faces,");
        puts("! falling back on cmap misses without changing libghostty's committed width.");

        puts("\n! Eight configured font-menu sizes");
        puts("! xterm has ten menu choices: these eight configured choices plus two");
        puts("! runtime-only choices named Escape Sequence and Selection.");
        memset(choices, 0, sizeof(choices));
        for (slot = 0; slot < 8; ++slot) {
                char resource[64];
                char class_name[64];
                char display_name[64];
                ResourceSpec bitmap_spec;
                Resolved bitmap;

                (void)snprintf(resource, sizeof(resource), "xterm.vt100.%s", resource_names[slot]);
                (void)snprintf(class_name, sizeof(class_name), "XTerm.VT100.Font%s",
                               slot == 0 ? "" : resource_names[slot] + 4);
                if (slot == 0)
                        (void)snprintf(class_name, sizeof(class_name), "XTerm.VT100.Font");
                bitmap_spec.resource = resource;
                bitmap_spec.class_name = class_name;
                bitmap_spec.default_value = defaults[slot];
                bitmap = ResolveResource(context, &bitmap_spec);
                (void)XtpVtFontSlotInfo(vt, slot, &choices[slot].loaded);
                choices[slot].label = labels[slot];
                choices[slot].resource = resource_names[slot];
                choices[slot].area = (unsigned long)choices[slot].loaded.cell_width *
                                     choices[slot].loaded.cell_height;
                (void)snprintf(display_name, sizeof(display_name), "XTerm*%s",
                               resource_names[slot]);
                {
                        char explanation[192];

                        (void)snprintf(explanation, sizeof(explanation),
                                       "%s menu entry; active renderer cell %ux%u.", labels[slot],
                                       choices[slot].loaded.cell_width,
                                       choices[slot].loaded.cell_height);
                        PrintResolved(display_name, "supported", explanation, &bitmap);
                }
                free(bitmap.value);
        }
        base_area = choices[0].area != 0 ? choices[0].area : 1;
        for (slot = 0; slot < 8; ++slot) {
                ResourceSpec alternative_spec;
                Resolved alternative;
                char resource[64];
                char class_name[64];

                if (slot == 0) {
                        choices[slot].point_size = base_points;
                } else {
                        (void)snprintf(resource, sizeof(resource), "xterm.vt100.faceSize%d", slot);
                        (void)snprintf(class_name, sizeof(class_name), "XTerm.VT100.FaceSize%d",
                                       slot);
                        alternative_spec.resource = resource;
                        alternative_spec.class_name = class_name;
                        alternative_spec.default_value = "0.0";
                        alternative = ResolveResource(context, &alternative_spec);
                        {
                                char display_name[64];
                                char explanation[192];

                                (void)snprintf(display_name, sizeof(display_name),
                                               "XTerm*faceSize%d", slot);
                                (void)snprintf(
                                    explanation, sizeof(explanation),
                                    "%s Xft point size; 0.0 derives it from bitmap cell area.",
                                    labels[slot]);
                                PrintResolved(display_name, "supported", explanation, &alternative);
                        }
                        choices[slot].point_size =
                            choices[slot].loaded.point_size > 0.0
                                ? choices[slot].loaded.point_size
                                : NumberValue(&alternative,
                                              base_points * sqrt((double)choices[slot].area /
                                                                 (double)base_area));
                        free(alternative.value);
                }
        }
        memcpy(ordered, choices, sizeof(ordered));
        qsort(ordered, XtNumber(ordered), sizeof(ordered[0]), CompareFontChoice);
        for (slot = 0; slot < 8; ++slot) {
                ordered[slot].order = slot + 1;
                printf("! size-order %d: %-10s active-cell=%ux%u xft-points=%.2f%s\n",
                       ordered[slot].order, ordered[slot].label, ordered[slot].loaded.cell_width,
                       ordered[slot].loaded.cell_height, ordered[slot].point_size,
                       strcmp(ordered[slot].label, "Default") == 0 ? " (startup slot)" : "");
        }
        puts("! [unset] [unsupported] font-menu Escape Sequence: runtime control sequence");
        puts("! [unset] [unsupported] font-menu Selection: PRIMARY selection value");

        free(render.value);
        free(face.value);
        free(double_face.value);
        free(base_size.value);
        free(emoji_face.value);
        free(han_face.value);
        free(bold_face.value);
        free(wide_bold_face.value);
        free(presentation.value);
        free(grapheme_width.value);
        free(color_glyphs.value);
        free(system_fallback.value);
        free(report_routing.value);
}

static void
ReportResourceGroup(const ReportContext *context, const char *heading, const ResourceSpec *specs,
                    const char *const *display_names, const char *const *support,
                    const char *const *explanations, size_t count)
{
        size_t item;

        printf("\n! ----------------------------------------------------------------------\n");
        printf("! %s\n", heading);
        for (item = 0; item < count; ++item) {
                Resolved resolved = ResolveResource(context, &specs[item]);
                PrintResolved(display_names[item], support[item], explanations[item], &resolved);
                free(resolved.value);
        }
}

static void
ReportPaletteAndPointer(const ReportContext *context)
{
        static const char *const defaults[XTP_ANSI_PALETTE_SIZE] = {XTP_ANSI_PALETTE_DEFAULT_LIST};
        unsigned int color;

        puts("\n! ----------------------------------------------------------------------");
        puts("! ANSI palette and pointer");
        puts("! ANSI palette values are applied; pointer values are not applied yet.");
        for (color = 0; color < XTP_ANSI_PALETTE_SIZE; ++color) {
                char resource[64];
                char class_name[64];
                char display_name[64];
                char explanation[96];
                ResourceSpec spec;
                Resolved resolved;

                (void)snprintf(resource, sizeof(resource), "xterm.vt100.color%u", color);
                (void)snprintf(class_name, sizeof(class_name), "XTerm.VT100.Color%u", color);
                (void)snprintf(display_name, sizeof(display_name), "XTerm*color%u", color);
                (void)snprintf(explanation, sizeof(explanation), "ANSI palette index %u.", color);
                spec.resource = resource;
                spec.class_name = class_name;
                spec.default_value = defaults[color];
                resolved = ResolveResource(context, &spec);
                PrintResolved(display_name, "supported", explanation, &resolved);
                free(resolved.value);
        }
        {
                static const ResourceSpec pointer_specs[] = {
                    {"xterm.vt100.pointerColor", "XTerm.VT100.PointerColor", NULL},
                    {"xterm.vt100.pointerShape", "XTerm.VT100.PointerShape", NULL},
                };
                static const char *const pointer_names[] = {
                    "XTerm*pointerColor",
                    "XTerm*pointerShape",
                };
                static const char *const pointer_support[] = {
                    "accepted but ignored",
                    "accepted but ignored",
                };
                static const char *const pointer_help[] = {
                    "Pointer foreground color.",
                    "Pointer cursor shape.",
                };

                ReportResourceGroup(context, "Pointer details", pointer_specs, pointer_names,
                                    pointer_support, pointer_help, XtNumber(pointer_specs));
        }
}

static bool
KnownTranslationAction(const char *action)
{
        return strcmp(action, "larger-vt-font") == 0 || strcmp(action, "smaller-vt-font") == 0 ||
               strcmp(action, "set-render-font") == 0 || strcmp(action, "set-select") == 0 ||
               strcmp(action, "report-font-routing") == 0 || strcmp(action, "popup-menu") == 0 ||
               strcmp(action, "scroll-back") == 0 || strcmp(action, "scroll-forw") == 0 ||
               strcmp(action, "select-start") == 0 || strcmp(action, "select-extend") == 0 ||
               strcmp(action, "select-end") == 0 || strcmp(action, "start-extend") == 0 ||
               strcmp(action, "insert-selection") == 0 || strcmp(action, "mouse-press") == 0 ||
               strcmp(action, "mouse-motion") == 0;
}

static bool
ActionWasSeen(char actions[][64], size_t count, const char *action)
{
        size_t item;

        for (item = 0; item < count; ++item) {
                if (strcmp(actions[item], action) == 0)
                        return true;
        }
        return false;
}

static size_t
TranslationActions(const char *value, char actions[][64], size_t capacity)
{
        size_t count = 0;
        const char *cursor = value;

        while (cursor != NULL && *cursor != '\0') {
                const char *open = strchr(cursor, '(');
                const char *begin;
                size_t length;

                if (open == NULL)
                        break;
                begin = open;
                while (begin > cursor &&
                       (isalnum((unsigned char)begin[-1]) || begin[-1] == '-' || begin[-1] == '_'))
                        --begin;
                length = (size_t)(open - begin);
                if (length != 0 && length < sizeof(actions[0])) {
                        char action[64];

                        memcpy(action, begin, length);
                        action[length] = '\0';
                        if (!ActionWasSeen(actions, count, action) && count < capacity) {
                                memcpy(actions[count], action, length + 1U);
                                ++count;
                        }
                }
                cursor = open + 1;
        }
        return count;
}

static void
PrintTranslationResource(const Resolved *resolved, const char *support)
{
        const char *cursor;

        PrintComment(resolved->origin, support,
                     "VT widget event bindings; #override changes matching class defaults.");
        if (use_color)
                fputs(OriginColor(resolved->origin), stdout);
        fputs("XTerm*VT100.translations:\t", stdout);
        for (cursor = resolved->value; cursor != NULL && *cursor != '\0'; ++cursor) {
                if (*cursor == '\n')
                        fputs("\\n\\\n    ", stdout);
                else
                        fputc(*cursor, stdout);
        }
        if (use_color)
                fputs(reset, stdout);
        fputc('\n', stdout);
}

static void
ReportTranslations(const ReportContext *context)
{
        ResourceSpec spec = {
            "xterm.vt100.translations",
            "XTerm.VT100.Translations",
            "<class defaults>",
        };
        Resolved resolved = ResolveResource(context, &spec);
        char actions[64][64] = {{0}};
        size_t action_count = 0;
        size_t unsupported = 0;
        size_t action;
        const char *support;

        if (resolved.origin == ORIGIN_COMPILED_DEFAULT) {
                static const char *const defaults[] = {
                    "larger-vt-font", "smaller-vt-font", "insert-selection", "scroll-back",
                    "scroll-forw",    "popup-menu",      "select-start",     "mouse-press",
                    "start-extend",   "select-end",      "mouse-motion",     "select-extend",
                };

                for (action = 0; action < XtNumber(defaults); ++action)
                        (void)snprintf(actions[action], sizeof(actions[action]), "%s",
                                       defaults[action]);
                action_count = XtNumber(defaults);
        } else {
                action_count = TranslationActions(resolved.value, actions, XtNumber(actions));
        }
        for (action = 0; action < action_count; ++action) {
                if (!KnownTranslationAction(actions[action]))
                        ++unsupported;
        }
        if (unsupported == 0)
                support = "supported";
        else if (unsupported == action_count)
                support = "unsupported";
        else
                support = "partially supported";

        puts("\n! ----------------------------------------------------------------------");
        puts("! VT100 translations and action audit");
        puts("! The translation-table syntax is handled by Xt. Each invoked action must");
        puts("! also be registered by xterm+; upstream xterm has many more actions.");
        PrintTranslationResource(&resolved, support);
        for (action = 0; action < action_count; ++action) {
                bool known = KnownTranslationAction(actions[action]);

                printf("! action %s(): %s", actions[action], known ? "supported" : "unsupported");
                fputc('\n', stdout);
        }
        if (action_count == 0)
                puts("! action audit: no action calls found in the resolved value");
        printf("! patch-411 registered action ledger: %zu unique actions\n",
               xtp_xterm_411_action_count);
        for (action = 0; action < xtp_xterm_411_action_count; ++action) {
                const char *name = xtp_xterm_411_actions[action];

                printf("! [%s] action %s()\n",
                       KnownTranslationAction(name) ? "supported" : "unsupported", name);
        }
        free(resolved.value);
}

static const char *
CatalogSupport(const XtpResourceCatalogEntry *entry)
{
        const char *name = entry->name;
        bool ansi_palette =
            strncmp(name, "color", 5) == 0 &&
            (((name[5] >= '0' && name[5] <= '9') && name[6] == '\0') ||
             (name[5] == '1' && name[6] >= '0' && name[6] <= '5' && name[7] == '\0'));

        if (entry->scope == XTP_RESOURCE_APPLICATION_CONDITIONAL ||
            entry->scope == XTP_RESOURCE_VT100_CONDITIONAL ||
            entry->scope == XTP_RESOURCE_TEK4014_CONDITIONAL)
                return "unsupported (reference build option disabled)";
        if (entry->scope == XTP_RESOURCE_TEK4014 || entry->scope == XTP_RESOURCE_FONT_SUBRESOURCE)
                return "unsupported";
        if (entry->scope == XTP_RESOURCE_APPLICATION) {
                if (strcmp(name, "title") == 0 || strcmp(name, "iconName") == 0 ||
                    strcmp(name, "iconGeometry") == 0 || strcmp(name, "menuLocale") == 0)
                        return "supported";
                return "unsupported";
        }
        if (strcmp(name, "foreground") == 0 || strcmp(name, "background") == 0 ||
            strcmp(name, "cursorColor") == 0 || strcmp(name, "font") == 0 ||
            strcmp(name, "font1") == 0 || strcmp(name, "font2") == 0 ||
            strcmp(name, "font3") == 0 || strcmp(name, "font4") == 0 ||
            strcmp(name, "font5") == 0 || strcmp(name, "font6") == 0 ||
            strcmp(name, "font7") == 0 || strcmp(name, "geometry") == 0 ||
            strcmp(name, "internalBorder") == 0 || strcmp(name, "alwaysHighlight") == 0 ||
            strcmp(name, "cursorBlink") == 0 || strcmp(name, "cursorOnTime") == 0 ||
            strcmp(name, "cursorOffTime") == 0 || strcmp(name, "cursorBlinkXOR") == 0 ||
            strcmp(name, "saveLines") == 0 || strcmp(name, "scrollBar") == 0 ||
            strcmp(name, "scrollBarBorder") == 0 || strcmp(name, "rightScrollBar") == 0 ||
            strcmp(name, "scrollKey") == 0 || strcmp(name, "scrollTtyOutput") == 0 ||
            strcmp(name, "selectToClipboard") == 0 || strcmp(name, "multiClickTime") == 0 ||
            strcmp(name, "charClass") == 0 || strcmp(name, "renderFont") == 0 ||
            strcmp(name, "faceName") == 0 || strcmp(name, "faceNameDoublesize") == 0 ||
            strcmp(name, "faceNameEmoji") == 0 || strcmp(name, "faceNameHan") == 0 ||
            strcmp(name, "boldFont") == 0 || strcmp(name, "wideBoldFont") == 0 ||
            strcmp(name, "emojiPresentation") == 0 || strcmp(name, "graphemeWidth") == 0 ||
            strcmp(name, "colorGlyphs") == 0 || strcmp(name, "limitFontsets") == 0 ||
            strcmp(name, "limitFontHeight") == 0 || strcmp(name, "limitFontWidth") == 0 ||
            strncmp(name, "faceSize", 8) == 0 || ansi_palette)
                return "supported";
        if (strncmp(name, "color", 5) == 0 || strcmp(name, "pointerColor") == 0 ||
            strcmp(name, "pointerColorBackground") == 0 || strcmp(name, "pointerShape") == 0)
                return "accepted but ignored";
        return "unsupported";
}

static const char *
CatalogScopeName(XtpResourceScope scope)
{
        switch (scope) {
        case XTP_RESOURCE_APPLICATION:
                return "application";
        case XTP_RESOURCE_VT100:
                return "VT100";
        case XTP_RESOURCE_TEK4014:
                return "Tek4014";
        case XTP_RESOURCE_FONT_SUBRESOURCE:
                return "VT font subresource";
        case XTP_RESOURCE_APPLICATION_CONDITIONAL:
                return "conditional application";
        case XTP_RESOURCE_VT100_CONDITIONAL:
                return "conditional VT100";
        case XTP_RESOURCE_TEK4014_CONDITIONAL:
                return "conditional Tek4014";
        }
        return "unknown";
}

static void
CatalogResourceSpec(const XtpResourceCatalogEntry *entry, char *resource, size_t resource_size,
                    char *class_name, size_t class_size, char *display_name, size_t display_size)
{
        switch (entry->scope) {
        case XTP_RESOURCE_APPLICATION:
        case XTP_RESOURCE_APPLICATION_CONDITIONAL:
                (void)snprintf(resource, resource_size, "xterm.%s", entry->name);
                (void)snprintf(class_name, class_size, "XTerm.%s", entry->class_name);
                (void)snprintf(display_name, display_size, "XTerm*%s", entry->name);
                break;
        case XTP_RESOURCE_VT100:
        case XTP_RESOURCE_VT100_CONDITIONAL:
                (void)snprintf(resource, resource_size, "xterm.vt100.%s", entry->name);
                (void)snprintf(class_name, class_size, "XTerm.VT100.%s", entry->class_name);
                (void)snprintf(display_name, display_size, "XTerm*%s", entry->name);
                break;
        case XTP_RESOURCE_TEK4014:
        case XTP_RESOURCE_TEK4014_CONDITIONAL:
                (void)snprintf(resource, resource_size, "xterm.tektronix.%s", entry->name);
                (void)snprintf(class_name, class_size, "XTerm.Tektronix.%s", entry->class_name);
                (void)snprintf(display_name, display_size, "XTerm*tek4014*%s", entry->name);
                break;
        case XTP_RESOURCE_FONT_SUBRESOURCE:
                (void)snprintf(resource, resource_size, "xterm.vt100.%s", entry->name);
                (void)snprintf(class_name, class_size, "XTerm.VT100.%s", entry->class_name);
                (void)snprintf(display_name, display_size, "XTerm*VT100.<font-subresource>*%s",
                               entry->name);
                break;
        }
}

static void
ReportUpstreamCatalog(const ReportContext *context)
{
        size_t item;
        size_t conditional = 0;

        for (item = 0; item < xtp_xterm_411_resource_count; ++item) {
                XtpResourceScope scope = xtp_xterm_411_resources[item].scope;

                if (scope == XTP_RESOURCE_APPLICATION_CONDITIONAL ||
                    scope == XTP_RESOURCE_VT100_CONDITIONAL ||
                    scope == XTP_RESOURCE_TEK4014_CONDITIONAL)
                        ++conditional;
        }

        puts("\n! ======================================================================");
        puts("! Exhaustive upstream X resource compatibility ledger");
        printf("! Authority: xterm patch 411 (%zu active, %zu conditional entries).\n",
               xtp_xterm_411_resource_count - conditional, conditional);
        puts("! This includes application, VT100, Tek4014, and named VT font tables,");
        puts("! plus resources hidden by disabled patch-411 compile-time options.");
        puts("! Unsupported entries are intentional open decisions,");
        puts("! not omissions from this report. Xt/Xaw inherited classes follow below.");
        for (item = 0; item < xtp_xterm_411_resource_count; ++item) {
                const XtpResourceCatalogEntry *entry = &xtp_xterm_411_resources[item];
                char resource[160];
                char class_name[160];
                char display_name[192];
                char explanation[160];
                ResourceSpec spec;
                Resolved resolved;

                CatalogResourceSpec(entry, resource, sizeof(resource), class_name,
                                    sizeof(class_name), display_name, sizeof(display_name));
                spec.resource = resource;
                spec.class_name = class_name;
                spec.default_value = entry->default_value;
                resolved = ResolveResource(context, &spec);
                (void)snprintf(explanation, sizeof(explanation),
                               "patch-411 %s resource (class %s).", CatalogScopeName(entry->scope),
                               entry->class_name);
                PrintResolved(display_name, CatalogSupport(entry), explanation, &resolved);
                free(resolved.value);
        }
}

static const char *
AppDefaultSupport(const char *record)
{
        if (strstr(record, "*saveLines:") != NULL)
                return "supported";
        if (strstr(record, "*tekMenu") != NULL || strstr(record, "*tek4014") != NULL ||
            strstr(record, "*VT100.utf8Fonts") != NULL || strstr(record, "*IconFont:") != NULL ||
            strstr(record, "*form.") != NULL || strstr(record, "*menubar.") != NULL ||
            strstr(record, "*MenuButton*") != NULL || strstr(record, "*vtMenu*cursesemul*") != NULL)
                return "unsupported";
        if (strstr(record, "*SimpleMenu") != NULL || strstr(record, "*mainMenu") != NULL ||
            strstr(record, "*vtMenu") != NULL || strstr(record, "*fontMenu") != NULL ||
            strstr(record, "*VT100.font") != NULL)
                return "supported";
        return "unsupported";
}

static void
ReportUpstreamAppDefaults(void)
{
        size_t item;

        puts("\n! ======================================================================");
        puts("! Upstream patch-411 XTerm app-default records");
        printf("! %zu active resource patterns from XTerm.ad. These preserve widget\n",
               xtp_xterm_411_app_default_count);
        puts("! instance names as well as resource names; the live app-defaults path");
        puts("! reported above may contain distribution or local overrides.");
        puts("! 'supported' here means the pattern reaches a matching widget/resource;");
        puts("! it does not imply that an insensitive menu item's command works yet.");
        for (item = 0; item < xtp_xterm_411_app_default_count; ++item) {
                const char *record = xtp_xterm_411_app_defaults[item];

                printf("! [%s] patch-411 XTerm.ad record\n", AppDefaultSupport(record));
                puts(record);
        }
}

typedef struct
{
        const char *label;
        const char *display_prefix;
        const char *resource_prefix;
        const char *class_prefix;
        WidgetClass widget_class;
        const char *support;
} ToolkitClassSpec;

static char *
ToolkitDefault(const XtResource *resource)
{
        char buffer[96];
        uintptr_t immediate;

        if (resource->default_type == NULL)
                return NULL;
        if (strcmp(resource->default_type, XtRString) == 0) {
                if (resource->default_addr == NULL)
                        return NULL;
                return strdup((const char *)resource->default_addr);
        }
        if (strcmp(resource->default_type, XtRImmediate) == 0) {
                immediate = (uintptr_t)resource->default_addr;
                if (strcmp(resource->resource_type, XtRBoolean) == 0)
                        return strdup(immediate != 0 ? "true" : "false");
                (void)snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)immediate);
                return strdup(buffer);
        }
        (void)snprintf(buffer, sizeof(buffer), "<computed by %s>", resource->default_type);
        return strdup(buffer);
}

static void
ReportToolkitClass(const ReportContext *context, const ToolkitClassSpec *class_spec)
{
        XtResourceList resources = NULL;
        Cardinal count = 0;
        Cardinal item;

        XtInitializeWidgetClass(class_spec->widget_class);
        XtGetResourceList(class_spec->widget_class, &resources, &count);
        printf("\n! %s: %u merged class resources\n", class_spec->label, (unsigned int)count);
        for (item = 0; item < count; ++item) {
                XtResource *resource = &resources[item];
                char resource_name[192];
                char class_name[192];
                char display_name[192];
                char explanation[192];
                char *default_value = ToolkitDefault(resource);
                const char *support = class_spec->support;
                ResourceSpec spec;
                Resolved resolved;

                (void)snprintf(resource_name, sizeof(resource_name), "%s.%s",
                               class_spec->resource_prefix, resource->resource_name);
                (void)snprintf(class_name, sizeof(class_name), "%s.%s", class_spec->class_prefix,
                               resource->resource_class);
                (void)snprintf(display_name, sizeof(display_name), "%s%s",
                               class_spec->display_prefix, resource->resource_name);
                (void)snprintf(explanation, sizeof(explanation),
                               "%s class resource; value type %s.", class_spec->label,
                               resource->resource_type);
                spec.resource = resource_name;
                spec.class_name = class_name;
                spec.default_value = default_value;
                resolved = ResolveResource(context, &spec);
                if (class_spec->widget_class == vt100WidgetClass &&
                    strcmp(resource->resource_name, XtNtranslations) == 0)
                        support = "partially supported";
                PrintResolved(display_name, support, explanation, &resolved);
                free(resolved.value);
                free(default_value);
        }
        if (resources != NULL)
                XtFree((char *)resources);
}

static void
ReportToolkitConstraints(const ReportContext *context, const ToolkitClassSpec *class_spec)
{
        XtResourceList resources = NULL;
        Cardinal count = 0;
        Cardinal item;

        XtInitializeWidgetClass(class_spec->widget_class);
        XtGetConstraintResourceList(class_spec->widget_class, &resources, &count);
        printf("\n! %s constraints: %u merged resources\n", class_spec->label, (unsigned int)count);
        for (item = 0; item < count; ++item) {
                XtResource *resource = &resources[item];
                char resource_name[192];
                char class_name[192];
                char display_name[192];
                char explanation[192];
                char *default_value = ToolkitDefault(resource);
                ResourceSpec spec;
                Resolved resolved;

                (void)snprintf(resource_name, sizeof(resource_name), "%s.%s",
                               class_spec->resource_prefix, resource->resource_name);
                (void)snprintf(class_name, sizeof(class_name), "%s.%s", class_spec->class_prefix,
                               resource->resource_class);
                (void)snprintf(display_name, sizeof(display_name), "%s%s",
                               class_spec->display_prefix, resource->resource_name);
                (void)snprintf(explanation, sizeof(explanation),
                               "%s child constraint; value type %s.", class_spec->label,
                               resource->resource_type);
                spec.resource = resource_name;
                spec.class_name = class_name;
                spec.default_value = default_value;
                resolved = ResolveResource(context, &spec);
                PrintResolved(display_name, class_spec->support, explanation, &resolved);
                free(resolved.value);
                free(default_value);
        }
        if (resources != NULL)
                XtFree((char *)resources);
}

static void
ReportToolkitCatalog(const ReportContext *context)
{
        const ToolkitClassSpec classes[] = {
            {"ApplicationShell (including inherited Xt classes)", "XTerm*", "xterm", "XTerm",
             applicationShellWidgetClass, "supported by Xt"},
            {"xterm+ VT100 (including inherited Core/Composite)", "XTerm*VT100*", "xterm.vt100",
             "XTerm.VT100", vt100WidgetClass, "supported by Xt/xterm+"},
            {"Athena SimpleMenu (including inherited shell classes)", "XTerm*SimpleMenu*",
             "xterm.mainMenu", "XTerm.SimpleMenu", simpleMenuWidgetClass, "supported by Xaw"},
            {"Athena SmeBSB menu entry (including inherited object classes)",
             "XTerm*SimpleMenu*SmeBSB*", "xterm.mainMenu.toolbar", "XTerm.SimpleMenu.SmeBSB",
             smeBSBObjectClass, "supported by Xaw"},
            {"Athena SmeLine separator (including inherited object classes)",
             "XTerm*SimpleMenu*SmeLine*", "xterm.mainMenu.line1", "XTerm.SimpleMenu.SmeLine",
             smeLineObjectClass, "supported by Xaw"},
            {"xterm+ Athena opacity slider (including inherited object classes)",
             "XTerm*mainMenu*SmeSlider*", "xterm.mainMenu.backgroundOpacity",
             "XTerm.SimpleMenu.SmeSlider", xtpSmeSliderObjectClass, "supported by Xt/xterm+"},
            {"Athena Scrollbar", "XTerm*VT100*scrollbar*", "xterm.vt100.scrollbar",
             "XTerm.VT100.Scrollbar", scrollbarWidgetClass, "supported by Xaw/xterm+"},
            {"Athena Form (conditional upstream toolbar component)", "XTerm*form*", "xterm.form",
             "XTerm.Form", formWidgetClass, "unsupported (toolbar option disabled)"},
            {"Athena Box (conditional upstream toolbar component)", "XTerm*form*menubar*",
             "xterm.form.menubar", "XTerm.Form.Box", boxWidgetClass,
             "unsupported (toolbar option disabled)"},
            {"Athena MenuButton (conditional upstream toolbar component)", "XTerm*MenuButton*",
             "xterm.form.menubar.mainMenuButton", "XTerm.Form.Box.MenuButton",
             menuButtonWidgetClass, "unsupported (toolbar option disabled)"},
        };
        const ToolkitClassSpec form_constraints = {
            "Athena Form (conditional upstream toolbar component)",
            "XTerm*form*<child>*",
            "xterm.form.menubar",
            "XTerm.Form.Box",
            formWidgetClass,
            "unsupported (toolbar option disabled)",
        };
        size_t item;

        puts("\n! ======================================================================");
        puts("! Xt and Athena component resource classes");
        puts("! Xt merges inherited resources after class initialization. These lists");
        puts("! therefore include Core/Shell/Object resources which xterm does not repeat");
        puts("! in its own tables, plus the Xaw menu and scrollbar component surfaces.");
        for (item = 0; item < XtNumber(classes); ++item)
                ReportToolkitClass(context, &classes[item]);
        ReportToolkitConstraints(context, &form_constraints);
}

static void
ReportResourceSources(Display *display, const char *manager, const char *application_class)
{
        static const char *const environment_names[] = {
            "XENVIRONMENT",
            "XFILESEARCHPATH",
            "XUSERFILESEARCHPATH",
            "XAPPLRESDIR",
        };
        char *app_defaults;
        size_t item;

        puts("\n! ----------------------------------------------------------------------");
        puts("! X resource input sources");
        puts("! Xt constructs one merged database from application defaults (or the");
        puts("! compiled fallbacks when no app-defaults file is found), the X server");
        puts("! RESOURCE_MANAGER database or legacy ~/.Xdefaults, XENVIRONMENT (or its");
        puts("! host-specific default), and command-line options/-xrm.");
        app_defaults = XtResolvePathname(display, "app-defaults", application_class, NULL, NULL,
                                         NULL, 0, NULL);
        printf("! resolved %s app-defaults: %s\n", application_class,
               app_defaults != NULL ? app_defaults : "<not found; fallbacks apply>");
        if (app_defaults != NULL)
                XtFree(app_defaults);
        printf("! server RESOURCE_MANAGER: %s", manager != NULL ? "present" : "absent");
        if (manager != NULL)
                printf(" (%zu bytes)", strlen(manager));
        fputc('\n', stdout);
        for (item = 0; item < XtNumber(environment_names); ++item) {
                const char *value = getenv(environment_names[item]);

                printf("! environment %s: %s\n", environment_names[item],
                       value != NULL ? value : "<unset>");
        }
        puts("! xterm+ fallback records: SimpleMenu spacing/margins/cursor, the");
        puts("! mainMenu/vtMenu/fontMenu labels, and the xterm+ shell title.");
        puts("! Provenance below distinguishes command-line values and reports server");
        puts("! database presence; other values merged by Xt are labeled 'X resources'.");
}

void
XtpReportConfig(Display *display, Widget vt, XrmDatabase command_database,
                const char *application_name, const char *application_class)
{
        static const ResourceSpec appearance[] = {
            {"xterm.geometry", "XTerm.Geometry", NULL},
            {"xterm.vt100.columns", "XTerm.VT100.Columns", "80"},
            {"xterm.vt100.rows", "XTerm.VT100.Rows", "24"},
            {"xterm.vt100.foreground", "XTerm.VT100.Foreground", "black"},
            {"xterm.vt100.background", "XTerm.VT100.Background", "white"},
            {"xterm.vt100.backgroundOpacity", "XTerm.VT100.BackgroundOpacity", "1.0"},
            {"xterm.vt100.cursorColor", "XTerm.VT100.CursorColor", "foreground"},
            {"xterm.vt100.alwaysHighlight", "XTerm.VT100.AlwaysHighlight", "false"},
            {"xterm.vt100.cursorBlink", "XTerm.VT100.CursorBlink", "false"},
            {"xterm.vt100.cursorBlinkXOR", "XTerm.VT100.CursorBlinkXOR", "true"},
            {"xterm.vt100.cursorOnTime", "XTerm.VT100.CursorOnTime", "600"},
            {"xterm.vt100.cursorOffTime", "XTerm.VT100.CursorOffTime", "300"},
            {"xterm.vt100.internalBorder", "XTerm.VT100.BorderWidth", "2"},
        };
        static const char *const appearance_names[] = {
            "XTerm*geometry",       "XTerm*columns",         "XTerm*rows",
            "XTerm*foreground",     "XTerm*background",      "XTerm*backgroundOpacity",
            "XTerm*cursorColor",    "XTerm*alwaysHighlight", "XTerm*cursorBlink",
            "XTerm*cursorBlinkXOR", "XTerm*cursorOnTime",    "XTerm*cursorOffTime",
            "XTerm*internalBorder",
        };
        static const char *const appearance_help[] = {
            "Initial columns/rows and optional window position.",
            "Default terminal columns when geometry does not override it.",
            "Default terminal rows when geometry does not override it.",
            "Default text color.",
            "Terminal background color.",
            "Default-background opacity from 0.0 (transparent) to 1.0 (opaque).",
            "Focused block and unfocused outline cursor color.",
            "Keep a filled cursor while unfocused.",
            "Default/application cursor blink policy: false, true, always, or never.",
            "Combine configured and application cursor blink state using XOR instead of OR.",
            "Milliseconds a blinking cursor remains visible.",
            "Milliseconds a blinking cursor remains hidden.",
            "Pixels between the terminal grid and window edge.",
        };
        static const char *const appearance_support[] = {
            "supported", "supported", "supported", "supported", "supported",
            "supported", "supported", "supported", "supported", "supported",
            "supported", "supported", "supported",
        };
        static const ResourceSpec behavior[] = {
            {"xterm.vt100.saveLines", "XTerm.VT100.SaveLines", "1024"},
            {"xterm.vt100.scrollBar", "XTerm.VT100.ScrollBar", "false"},
            {"xterm.vt100.rightScrollBar", "XTerm.VT100.RightScrollBar", "false"},
            {"xterm.vt100.scrollKey", "XTerm.VT100.ScrollCond", "false"},
            {"xterm.vt100.scrollTtyOutput", "XTerm.VT100.ScrollCond", "true"},
            {"xterm.vt100.selectToClipboard", "XTerm.VT100.SelectToClipboard", "false"},
            {"xterm.vt100.multiClickTime", "XTerm.VT100.MultiClickTime", "250"},
            {"xterm.vt100.charClass", "XTerm.VT100.CharClass", ""},
            {"xterm.logLevel", "XTerm.LogLevel", "warning"},
            {"xterm.debug", "XTerm.Debug", "false"},
        };
        static const char *const behavior_names[] = {
            "XTerm*saveLines",      "XTerm*scrollBar",       "XTerm*rightScrollBar",
            "XTerm*scrollKey",      "XTerm*scrollTtyOutput", "XTerm*selectToClipboard",
            "XTerm*multiClickTime", "XTerm*charClass",       "XTerm*logLevel",
            "XTerm*debug",
        };
        static const char *const behavior_help[] = {
            "Maximum saved-history lines retained by libghostty.",
            "Create and display the Athena scrollbar.",
            "Place the scrollbar on the right rather than the traditional left.",
            "Scroll to the active screen after an encoded keypress.",
            "Scroll to the active screen when PTY output arrives.",
            "Resolve the SELECT action token to CLIPBOARD rather than PRIMARY.",
            "Milliseconds allowed between clicks in a multi-click selection gesture.",
            "Override character classes used for double-click word selection.",
            "Minimum diagnostic severity: debug, info, warning, or error.",
            "Legacy Boolean alias used only when logLevel is unset.",
        };
        static const char *const behavior_support[] = {
            "supported", "supported", "supported", "supported", "supported",
            "supported", "supported", "supported", "supported", "supported",
        };
        XrmDatabase merged = XtDatabase(display);
        XrmDatabase server = NULL;
        const char *manager = XResourceManagerString(display);
        ReportContext context;

        use_color = isatty(STDOUT_FILENO) != 0 && getenv("NO_COLOR") == NULL;
        if (manager != NULL)
                server = XrmGetStringDatabase(manager);
        context.merged = merged;
        context.server = server;
        context.command = command_database;
        context.application_name = application_name;
        context.application_class = application_class;
        puts("! xterm+ consolidated configuration");
        puts("! Syntax is intentionally close to .Xresources. Redirect stdout for");
        puts("! plain, reusable text; terminal output uses colors unless NO_COLOR is set.");
        puts("! Origins: command line, merged X resources, compiled default, unset.");
        puts("! Support: supported, partially supported, accepted but ignored, or unsupported.");
        puts("! Active renderer: Xlib bitmap (transitional xterm+ implementation)");
        ReportResourceSources(display, manager, application_class);
        ReportFonts(&context, vt);
        ReportTranslations(&context);
        ReportPaletteAndPointer(&context);
        ReportResourceGroup(&context, "Window and appearance", appearance, appearance_names,
                            appearance_support, appearance_help, XtNumber(appearance));
        ReportResourceGroup(&context, "Behavior and transitional features", behavior,
                            behavior_names, behavior_support, behavior_help, XtNumber(behavior));
        ReportUpstreamCatalog(&context);
        ReportUpstreamAppDefaults();
        ReportToolkitCatalog(&context);
        if (server != NULL)
                XrmDestroyDatabase(server);
}
