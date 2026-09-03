#include "welcome.h"

#include "font_chain.h"
#include "terminal.h"
#include "version.h"
#include "vt_widget.h"

#include <X11/Xft/Xft.h>
#include <X11/Xresource.h>

#include <ctype.h>
#include <fontconfig/fontconfig.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#define XTP_OS_RELEASE_LIMIT (64U * 1024U)

typedef struct
{
        bool found;
        char family[160];
} FontMatch;

typedef struct
{
        const char *install_command;
        const char *xrdb;
        const char *mono;
        const char *emoji;
        const char *cjk;
} PackageHints;

static void
CopyClean(char *destination, size_t capacity, const char *source, size_t length)
{
        size_t used = 0;
        size_t index;

        if (capacity == 0)
                return;
        for (index = 0; index < length && used + 1U < capacity; ++index) {
                unsigned char byte = (unsigned char)source[index];

                if (byte < 0x20U || byte == 0x7fU)
                        continue;
                destination[used++] = (char)byte;
        }
        destination[used] = '\0';
}

static void
ParseValue(const char *begin, const char *end, char *destination, size_t capacity)
{
        char quote = '\0';
        size_t used = 0;

        while (begin < end && isspace((unsigned char)*begin))
                ++begin;
        while (end > begin && isspace((unsigned char)end[-1]))
                --end;
        if (begin < end && (*begin == '\'' || *begin == '"')) {
                quote = *begin++;
                if (end > begin && end[-1] == quote)
                        --end;
        }
        while (begin < end && used + 1U < capacity) {
                unsigned char byte = (unsigned char)*begin++;

                if (byte == '\\' && begin < end && quote != '\'' &&
                    (quote == '"' || *begin == '\\' || *begin == '\''))
                        byte = (unsigned char)*begin++;
                if (byte >= 0x20U && byte != 0x7fU)
                        destination[used++] = (char)byte;
        }
        if (capacity != 0)
                destination[used] = '\0';
}

static bool
KeyEquals(const char *begin, const char *end, const char *expected)
{
        return (size_t)(end - begin) == strlen(expected) &&
               strncmp(begin, expected, (size_t)(end - begin)) == 0;
}

int
XtpWelcomeParseOsRelease(const char *contents, XtpOsRelease *release)
{
        const char *line;

        if (contents == NULL || release == NULL)
                return -1;
        memset(release, 0, sizeof(*release));
        line = contents;
        while (*line != '\0') {
                const char *end = strchr(line, '\n');
                const char *equals;

                if (end == NULL)
                        end = line + strlen(line);
                equals = memchr(line, '=', (size_t)(end - line));
                if (line < end && *line != '#' && equals != NULL && equals > line) {
                        char *destination = NULL;
                        size_t capacity = 0;

                        if (KeyEquals(line, equals, "ID")) {
                                destination = release->id;
                                capacity = sizeof(release->id);
                        } else if (KeyEquals(line, equals, "ID_LIKE")) {
                                destination = release->id_like;
                                capacity = sizeof(release->id_like);
                        } else if (KeyEquals(line, equals, "PRETTY_NAME")) {
                                destination = release->name;
                                capacity = sizeof(release->name);
                        } else if (KeyEquals(line, equals, "VERSION_ID")) {
                                destination = release->version;
                                capacity = sizeof(release->version);
                        }
                        if (destination != NULL)
                                ParseValue(equals + 1, end, destination, capacity);
                }
                line = *end == '\0' ? end : end + 1;
        }
        return release->id[0] != '\0' || release->name[0] != '\0' ? 0 : -1;
}

static int
ReadOsReleaseFile(const char *path, XtpOsRelease *release)
{
        FILE *file;
        char *contents;
        size_t length;
        int result;

        file = fopen(path, "r");
        if (file == NULL)
                return -1;
        contents = malloc(XTP_OS_RELEASE_LIMIT + 1U);
        if (contents == NULL) {
                fclose(file);
                return -1;
        }
        length = fread(contents, 1, XTP_OS_RELEASE_LIMIT, file);
        if (ferror(file) || (!feof(file) && length == XTP_OS_RELEASE_LIMIT)) {
                free(contents);
                fclose(file);
                return -1;
        }
        contents[length] = '\0';
        result = XtpWelcomeParseOsRelease(contents, release);
        free(contents);
        fclose(file);
        return result;
}

static int
ReadOsRelease(XtpOsRelease *release)
{
        if (ReadOsReleaseFile("/etc/os-release", release) == 0)
                return 0;
        return ReadOsReleaseFile("/usr/lib/os-release", release);
}

static bool
HasWord(const char *words, const char *wanted)
{
        const char *word = words;
        size_t wanted_length = strlen(wanted);

        while (word != NULL && *word != '\0') {
                const char *end;

                while (*word != '\0' && isspace((unsigned char)*word))
                        ++word;
                end = word;
                while (*end != '\0' && !isspace((unsigned char)*end))
                        ++end;
                if ((size_t)(end - word) == wanted_length &&
                    strncmp(word, wanted, wanted_length) == 0)
                        return true;
                word = end;
        }
        return false;
}

static bool
OsIs(const XtpOsRelease *release, const char *id)
{
        return strcmp(release->id, id) == 0 || HasWord(release->id_like, id);
}

static PackageHints
PackagesFor(const XtpOsRelease *release)
{
        const char *family = XtpWelcomePackageFamily(release);

        if (family != NULL && strcmp(family, "apt") == 0)
                return (PackageHints){"sudo apt install", "x11-xserver-utils", "fonts-dejavu-mono",
                                      "fonts-noto-color-emoji", "fonts-noto-cjk"};
        if (family != NULL && strcmp(family, "dnf") == 0)
                return (PackageHints){"sudo dnf install", "xrdb", "dejavu-sans-mono-fonts",
                                      "google-noto-color-emoji-fonts",
                                      "google-noto-sans-cjk-fonts"};
        if (family != NULL && strcmp(family, "pacman") == 0)
                return (PackageHints){"sudo pacman -S", "xorg-xrdb", "ttf-dejavu",
                                      "noto-fonts-emoji", "noto-fonts-cjk"};
        return (PackageHints){0};
}

const char *
XtpWelcomePackageFamily(const XtpOsRelease *release)
{
        if (release == NULL)
                return NULL;
        if (OsIs(release, "debian") || OsIs(release, "ubuntu"))
                return "apt";
        if (OsIs(release, "fedora") || OsIs(release, "rhel") || OsIs(release, "centos"))
                return "dnf";
        if (OsIs(release, "arch") || OsIs(release, "manjaro"))
                return "pacman";
        return NULL;
}

bool
XtpWelcomeNeedsReadableFont(bool configured_font, bool using_xft, unsigned int cell_height,
                            double dpi)
{
        return !configured_font && !using_xft && (cell_height <= 16U || dpi >= 144.0);
}

static bool
ExecutableInPath(const char *program)
{
        const char *path = getenv("PATH");
        const char *part;
        size_t program_length = strlen(program);

        if (path == NULL)
                return false;
        part = path;
        for (;;) {
                const char *end = strchr(part, ':');
                size_t length = end != NULL ? (size_t)(end - part) : strlen(part);
                char *candidate = malloc((length != 0 ? length : 1U) + program_length + 2U);
                struct stat status;
                bool found;

                if (candidate == NULL)
                        return false;
                if (length == 0) {
                        candidate[0] = '.';
                        length = 1;
                } else {
                        memcpy(candidate, part, length);
                }
                candidate[length] = '/';
                memcpy(candidate + length + 1U, program, program_length + 1U);
                found = access(candidate, X_OK) == 0 && stat(candidate, &status) == 0 &&
                        !S_ISDIR(status.st_mode);
                free(candidate);
                if (found)
                        return true;
                if (end == NULL)
                        return false;
                part = end + 1;
        }
}

static bool
DatabaseValue(XrmDatabase database, const char *name, const char *class_name, char *value,
              size_t capacity)
{
        XrmValue found = {0};
        char *type = NULL;
        size_t length;

        if (database == NULL)
                return false;
        if (!XrmGetResource(database, name, class_name, &type, &found) || found.addr == NULL)
                return false;
        length = found.size;
        if (length != 0 && ((const char *)found.addr)[length - 1U] == '\0')
                --length;
        CopyClean(value, capacity, found.addr, length);
        return true;
}

static bool
ResourceValue(XrmDatabase database, const char *application_name, const char *application_class,
              const char *resource, const char *resource_class, char *value, size_t capacity)
{
        char name[256];
        char class_name[256];

        (void)snprintf(name, sizeof(name), "%s.vt100.%s", application_name, resource);
        (void)snprintf(class_name, sizeof(class_name), "%s.VT100.%s", application_class,
                       resource_class);
        return DatabaseValue(database, name, class_name, value, capacity);
}

static bool
ServerHasRelevantSettings(const char *manager, const char *application_name,
                          const char *application_class)
{
        static const struct
        {
                const char *name;
                const char *class_name;
        } probes[] = {
            {"font", "Font"},
            {"faceName", "FaceName"},
            {"faceSize", "FaceSize"},
            {"renderFont", "RenderFont"},
            {"background", "Background"},
            {"foreground", "Foreground"},
            {"saveLines", "SaveLines"},
            {"scrollBar", "ScrollBar"},
        };
        XrmDatabase database;
        char value[256];
        size_t index;
        bool found = false;

        if (manager == NULL || *manager == '\0')
                return false;
        database = XrmGetStringDatabase(manager);
        if (database == NULL)
                return false;
        for (index = 0; index < sizeof(probes) / sizeof(probes[0]); ++index) {
                if (ResourceValue(database, application_name, application_class, probes[index].name,
                                  probes[index].class_name, value, sizeof(value))) {
                        found = true;
                        break;
                }
        }
        XrmDestroyDatabase(database);
        return found;
}

static void
PrimaryPattern(const char *configured, const char *fallback, char *pattern, size_t capacity)
{
        XtpFontChain chain = {0};
        const char *selected = fallback;

        if (XtpFontChainParse(configured, &chain) == 0 && chain.count != 0)
                selected = chain.entries[0];
        CopyClean(pattern, capacity, selected, strlen(selected));
        XtpFontChainClear(&chain);
}

static double
PositiveNumber(const char *text, double fallback)
{
        char *end = NULL;
        double value;

        if (text == NULL || text[0] == '\0')
                return fallback;
        value = strtod(text, &end);
        return end != text && value > 0.0 ? value : fallback;
}

static FcPattern *
FontRequest(Display *display, const char *requested, double point_size)
{
        FcPattern *pattern = FcNameParse((const FcChar8 *)requested);

        if (pattern == NULL)
                return NULL;
        FcPatternDel(pattern, FC_SIZE);
        FcPatternDel(pattern, FC_PIXEL_SIZE);
        (void)FcPatternAddDouble(pattern, FC_SIZE, point_size);
        (void)FcConfigSubstitute(NULL, pattern, FcMatchPattern);
        XftDefaultSubstitute(display, DefaultScreen(display), pattern);
        return pattern;
}

static FontMatch
MatchFont(Display *display, const char *requested, const FcChar32 *characters, size_t count,
          bool require_color, bool require_scalable, double point_size)
{
        FontMatch result = {0};
        FcPattern *pattern;
        FcCharSet *wanted = NULL;
        FcFontSet *matches;
        FcResult match_result;
        int index;

        pattern = FontRequest(display, requested, point_size);
        if (pattern == NULL)
                return result;
        if (count != 0) {
                size_t item;

                wanted = FcCharSetCreate();
                if (wanted == NULL) {
                        FcPatternDestroy(pattern);
                        return result;
                }
                for (item = 0; item < count; ++item)
                        (void)FcCharSetAddChar(wanted, characters[item]);
                (void)FcPatternAddCharSet(pattern, FC_CHARSET, wanted);
        }
        matches = FcFontSort(NULL, pattern, FcTrue, NULL, &match_result);
        for (index = 0; matches != NULL && index < matches->nfont; ++index) {
                FcPattern *candidate = matches->fonts[index];
                FcCharSet *charset = NULL;
                FcBool color = FcFalse;
                FcBool scalable = FcFalse;
                FcChar8 *family = NULL;
                bool usable = true;
                size_t item;

                if (count != 0 &&
                    FcPatternGetCharSet(candidate, FC_CHARSET, 0, &charset) != FcResultMatch)
                        usable = false;
                for (item = 0; usable && item < count; ++item) {
                        if (!FcCharSetHasChar(charset, characters[item]))
                                usable = false;
                }
                (void)FcPatternGetBool(candidate, FC_COLOR, 0, &color);
                (void)FcPatternGetBool(candidate, FC_SCALABLE, 0, &scalable);
                if (require_color && !color)
                        usable = false;
                if (require_scalable && !scalable)
                        usable = false;
                if (!usable)
                        continue;
                (void)FcPatternGetString(candidate, FC_FAMILY, 0, &family);
                if (family != NULL)
                        CopyClean(result.family, sizeof(result.family), (const char *)family,
                                  strlen((const char *)family));
                result.found = family != NULL;
                break;
        }
        if (matches != NULL)
                FcFontSetDestroy(matches);
        if (wanted != NULL)
                FcCharSetDestroy(wanted);
        FcPatternDestroy(pattern);
        return result;
}

static double
DisplayDpi(Display *display, const char *requested, double point_size)
{
        FcPattern *pattern;
        int screen = DefaultScreen(display);
        int millimeters = DisplayHeightMM(display, screen);
        double dpi = 0.0;

        pattern = FontRequest(display, requested, point_size);
        if (pattern != NULL) {
                (void)FcPatternGetDouble(pattern, FC_DPI, 0, &dpi);
                FcPatternDestroy(pattern);
        }
        if (dpi > 0.0)
                return dpi;
        if (millimeters <= 0)
                return 0.0;
        return (double)DisplayHeight(display, screen) * 25.4 / (double)millimeters;
}

static void
PrintPackageSuggestion(FILE *stream, const PackageHints *hints, bool need_xrdb, bool need_mono,
                       bool need_emoji, bool need_cjk)
{
        if (!need_xrdb && !need_mono && !need_emoji && !need_cjk)
                return;
        if (hints->install_command == NULL) {
                fputs("  [recommend] Ask your package manager for", stream);
                if (need_xrdb)
                        fputs(" xrdb", stream);
                if (need_mono)
                        fputs(" a scalable monospace font", stream);
                if (need_emoji)
                        fputs(" a color emoji font", stream);
                if (need_cjk)
                        fputs(" a CJK font", stream);
                fputs(".\n", stream);
                return;
        }
        if (need_xrdb || need_mono) {
                fprintf(stream, "  [recommend] Install setup essentials with: %s",
                        hints->install_command);
                if (need_xrdb)
                        fprintf(stream, " %s", hints->xrdb);
                if (need_mono)
                        fprintf(stream, " %s", hints->mono);
                fputc('\n', stream);
        }
        if (need_emoji || need_cjk) {
                fprintf(stream, "  [optional] For broader Unicode coverage: %s",
                        hints->install_command);
                if (need_emoji)
                        fprintf(stream, " %s", hints->emoji);
                if (need_cjk)
                        fprintf(stream, " %s", hints->cjk);
                fputc('\n', stream);
        }
}

void
XtpWelcomeReport(FILE *stream, Display *display, Widget vt, const char *application_name,
                 const char *application_class)
{
        static const FcChar32 emoji_characters[] = {0x1f600U, 0x1f3fDU, 0x1f4bbU};
        static const FcChar32 cjk_characters[] = {0x4e2dU, 0x65e5U, 0x8a9eU};
        XtpOsRelease release = {0};
        struct utsname system_name;
        PackageHints hints;
        XrmDatabase database = XtDatabase(display);
        char resource[256];
        char primary_pattern[256];
        char emoji_pattern[256];
        char clean_application_name[160];
        char clean_application_class[160];
        char clean_host[160];
        char clean_host_version[160];
        char *app_defaults;
        String face_name = NULL;
        String emoji_name = NULL;
        String face_size = NULL;
        FontMatch primary;
        FontMatch emoji;
        FontMatch cjk;
        const char *host = getenv("TERM_PROGRAM");
        const char *host_version = getenv("TERM_PROGRAM_VERSION");
        const char *manager = XResourceManagerString(display);
        bool hosted_here = host != NULL && strcmp(host, XTP_PROGRAM_NAME) == 0;
        bool xrdb_available = ExecutableInPath("xrdb");
        bool relevant_server_settings =
            ServerHasRelevantSettings(manager, application_name, application_class);
        bool configured_font = false;
        bool configured_face_size;
        bool using_xft = XtpVtUsingXft(vt);
        bool needs_readable_font;
        bool output_is_terminal = isatty(fileno(stream)) != 0;
        double point_size;
        double dpi;
        unsigned int cell_width = XtpVtCellWidth(vt);
        unsigned int cell_height = XtpVtCellHeight(vt);

        (void)ReadOsRelease(&release);
        if (uname(&system_name) != 0)
                memset(&system_name, 0, sizeof(system_name));
        hints = PackagesFor(&release);
        XtVaGetValues(vt, "faceName", &face_name, "faceNameEmoji", &emoji_name, "faceSize",
                      &face_size, NULL);
        configured_face_size = ResourceValue(database, application_name, application_class,
                                             "faceSize", "FaceSize", resource, sizeof(resource));
        configured_font = ResourceValue(database, application_name, application_class, "font",
                                        "Font", resource, sizeof(resource)) ||
                          ResourceValue(database, application_name, application_class, "faceName",
                                        "FaceName", resource, sizeof(resource)) ||
                          ResourceValue(database, application_name, application_class, "renderFont",
                                        "RenderFont", resource, sizeof(resource)) ||
                          configured_face_size;
        PrimaryPattern(face_name, "monospace", primary_pattern, sizeof(primary_pattern));
        PrimaryPattern(emoji_name, "emoji", emoji_pattern, sizeof(emoji_pattern));
        point_size = PositiveNumber(face_size, 8.0);
        dpi = DisplayDpi(display, primary_pattern, point_size);
        primary = MatchFont(display, primary_pattern, NULL, 0, false, true, point_size);
        emoji = MatchFont(display, emoji_pattern, emoji_characters,
                          sizeof(emoji_characters) / sizeof(emoji_characters[0]), true, true,
                          point_size);
        cjk =
            MatchFont(display, "sans-serif", cjk_characters,
                      sizeof(cjk_characters) / sizeof(cjk_characters[0]), false, true, point_size);
        needs_readable_font =
            XtpWelcomeNeedsReadableFont(configured_font, using_xft, cell_height, dpi);
        app_defaults = XtResolvePathname(display, "app-defaults", application_class, NULL, NULL,
                                         NULL, 0, NULL);
        CopyClean(clean_application_name, sizeof(clean_application_name), application_name,
                  strlen(application_name));
        CopyClean(clean_application_class, sizeof(clean_application_class), application_class,
                  strlen(application_class));
        CopyClean(clean_host, sizeof(clean_host), host != NULL ? host : "unknown",
                  strlen(host != NULL ? host : "unknown"));
        CopyClean(clean_host_version, sizeof(clean_host_version),
                  host_version != NULL ? host_version : "",
                  strlen(host_version != NULL ? host_version : ""));

        fprintf(stream, "%s welcome\n", XTP_PROGRAM_NAME);
        fputs("===============\n\n", stream);
        fputs("Setup health\n", stream);
        fprintf(stream, "  [%s] Renderer: %s; startup cell: %ux%u; display DPI: ",
                needs_readable_font ? "recommend" : "ok", XtpVtRendererName(vt), cell_width,
                cell_height);
        if (dpi > 0.0)
                fprintf(stream, "%.0f\n", dpi);
        else
                fputs("unknown\n", stream);
        fprintf(stream, "  [info] %s app-defaults: %s\n", clean_application_class,
                app_defaults != NULL ? "found" : "not found; built-in fallbacks active");
        fprintf(stream, "  [info] X server resource database: %s\n",
                manager != NULL && *manager != '\0' ? "present" : "absent");
        fprintf(stream, "  [%s] Relevant %s/%s server settings: %s\n",
                relevant_server_settings ? "ok" : "recommend", clean_application_name,
                clean_application_class, relevant_server_settings ? "found" : "not found");
        fprintf(stream, "  [%s] xrdb command: %s\n", xrdb_available ? "ok" : "recommend",
                xrdb_available ? "available" : "not found");
        if (using_xft)
                fprintf(stream, "  [%s] Primary scalable font: %s%s%s\n",
                        primary.found ? "ok" : "recommend",
                        primary.found ? primary.family : "no match for ",
                        primary.found ? "" : primary_pattern,
                        configured_face_size ? " (faceSize applies)" : "");
        else
                fprintf(stream, "  [info] Primary bitmap font: fixed (%ux%u cell)\n", cell_width,
                        cell_height);
        fprintf(stream, "  [%s] Color emoji coverage: %s\n", emoji.found ? "ok" : "recommend",
                emoji.found ? emoji.family : "not found");
        fprintf(stream, "  [%s] CJK coverage: %s\n", cjk.found ? "ok" : "recommend",
                cjk.found ? cjk.family : "not found");

        if (needs_readable_font) {
                fprintf(stream,
                        "\nReadable starter resources\n"
                        "  The unconfigured bitmap default is intentionally xterm-compatible, but\n"
                        "  it is often too small on modern high-density displays. Add this to\n"
                        "  ~/.Xresources, then run `xrdb -merge ~/.Xresources`:\n\n"
                        "    %s*renderFont: true\n"
                        "    %s*faceName: monospace\n"
                        "    %s*faceSize: 13\n"
                        "    %s*saveLines: 10000\n",
                        clean_application_class, clean_application_class, clean_application_class,
                        clean_application_class);
                if (emoji.found)
                        fprintf(stream, "    %s*faceNameEmoji: %s\n", clean_application_class,
                                emoji.family);
                if (cjk.found)
                        fprintf(stream, "    %s*faceNameHan: %s\n", clean_application_class,
                                cjk.family);
        }
        if (manager == NULL || *manager == '\0')
                fputs("\n  No RESOURCE_MANAGER property is loaded. ~/.Xdefaults may be read as a\n"
                      "  legacy fallback, but ~/.Xresources plus xrdb is the reliable workflow.\n",
                      stream);
        PrintPackageSuggestion(stream, &hints, !xrdb_available, !primary.found, !emoji.found,
                               !cjk.found);

        fputs("\nColor palettes\n"
              "  [optional] Browse palettes and their live demo at https://terminal.love/.\n"
              "  Its export is ready for Xresources by default. Review the generated\n"
              "  foreground, background, and color0 through color15 entries before adding\n"
              "  them to ~/.Xresources.\n",
              stream);

        fputs("\nVisual sample\n", stream);
        if (output_is_terminal && hosted_here) {
                fprintf(stream,
                        "  %s host detected. These should remain aligned and intact:\n"
                        "  styles: \033[1mbold\033[0m \033[3mitalic\033[0m "
                        "\033[4munderline\033[0m\n"
                        "  emoji:  👩🏽‍💻  👨‍👩‍👧‍👦  🇺🇸  ❤️  "
                        "☺︎ "
                        " "
                        "☺️\n"
                        "  text:   é  क्  العربية  中文  日本語\n",
                        XTP_PROGRAM_NAME);
        } else {
                fprintf(stream,
                        "  Advanced rendering sample withheld: output=%s, host=%s.\n"
                        "  Run `%s -welcome` without redirection directly inside %s to inspect "
                        "its rendering.\n",
                        output_is_terminal ? "terminal" : "redirected", clean_host,
                        XTP_PROGRAM_NAME, XTP_PROGRAM_NAME);
        }

        fputs("\nSupport\n", stream);
        fprintf(stream, "  os: %s%s%s", release.id[0] != '\0' ? release.id : "unknown",
                release.version[0] != '\0' ? " " : "", release.version);
        if (release.name[0] != '\0')
                fprintf(stream, " (%s)", release.name);
        fputc('\n', stream);
        fprintf(stream, "  architecture: %s\n",
                system_name.machine[0] != '\0' ? system_name.machine : "unknown");
        fprintf(stream, "  %s: %s\n", XTP_PROGRAM_NAME, XTP_VERSION);
        fprintf(stream, "  backend: %s\n", XtpTerminalBackend());
        if (!XtpTerminalBackendIsStub())
                fprintf(stream, "  backend-revision: %s\n", XTP_GHOSTTY_REVISION);
        fprintf(stream, "  renderer: %s\n", XtpVtRendererName(vt));
        fprintf(stream, "  identity: %s / %s\n", clean_application_name, clean_application_class);
        fprintf(stream, "  primary-font: %s\n",
                using_xft ? (primary.found ? primary.family : "unresolved") : "fixed (bitmap)");
        fprintf(stream, "  emoji-font: %s\n", emoji.found ? emoji.family : "not found");
        fprintf(stream, "  app-defaults: %s\n", app_defaults != NULL ? "found" : "not found");
        fprintf(stream, "  host-terminal: %s%s%s\n", clean_host,
                clean_host_version[0] != '\0' ? " " : "", clean_host_version);
        fprintf(stream, "\nMore detail: %s -report-config\nManual:      man %s\n", XTP_PROGRAM_NAME,
                XTP_PROGRAM_NAME);
        fprintf(stream, "Help:        %s\n", XTP_SUPPORT_URL);

        if (app_defaults != NULL)
                XtFree(app_defaults);
}
