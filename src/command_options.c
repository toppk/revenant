#include "command_options.h"

#include "version.h"

#include <X11/Intrinsic.h>

#include <stdlib.h>
#include <string.h>

XrmOptionDescRec XtpCommandOptions[] = {
    {"-background", "*background", XrmoptionSepArg, NULL},
    {"-bg", "*background", XrmoptionSepArg, NULL},
    {"-bordercolor", ".borderColor", XrmoptionSepArg, NULL},
    {"-bd", ".borderColor", XrmoptionSepArg, NULL},
    {"-borderwidth", ".borderWidth", XrmoptionSepArg, NULL},
    {"-bw", ".borderWidth", XrmoptionSepArg, NULL},
    {"-foreground", "*foreground", XrmoptionSepArg, NULL},
    {"-fg", "*foreground", XrmoptionSepArg, NULL},
    {"-geometry", "*geometry", XrmoptionSepArg, NULL},
    {"-font", "*vt100.font", XrmoptionSepArg, NULL},
    {"-fn", "*vt100.font", XrmoptionSepArg, NULL},
    {"-fb", "*vt100.boldFont", XrmoptionSepArg, NULL},
    {"-fwb", "*vt100.wideBoldFont", XrmoptionSepArg, NULL},
    {"-fa", "*vt100.faceName", XrmoptionSepArg, NULL},
    {"-fd", "*vt100.faceNameDoublesize", XrmoptionSepArg, NULL},
    {"-fe", "*vt100.faceNameEmoji", XrmoptionSepArg, NULL},
    {"-fs", "*vt100.faceSize", XrmoptionSepArg, NULL},
    {"-b", "*vt100.internalBorder", XrmoptionSepArg, NULL},
    {"-sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"+sb", "*vt100.scrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sl", "*vt100.saveLines", XrmoptionSepArg, NULL},
    {"-cc", "*vt100.charClass", XrmoptionSepArg, NULL},
    {"-mc", "*vt100.multiClickTime", XrmoptionSepArg, NULL},
    {"-rightbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "true"},
    {"-leftbar", "*vt100.rightScrollBar", XrmoptionNoArg, (XPointer) "false"},
    {"-sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "true"},
    {"+sk", "*vt100.scrollKey", XrmoptionNoArg, (XPointer) "false"},
    {"-si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "false"},
    {"+si", "*vt100.scrollTtyOutput", XrmoptionNoArg, (XPointer) "true"},
    {"-ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "true"},
    {"+ah", "*vt100.alwaysHighlight", XrmoptionNoArg, (XPointer) "false"},
    {"-cr", "*vt100.cursorColor", XrmoptionSepArg, NULL},
    {"-pc", "*vt100.boldColors", XrmoptionNoArg, (XPointer) "true"},
    {"+pc", "*vt100.boldColors", XrmoptionNoArg, (XPointer) "false"},
    {"-iconic", ".iconic", XrmoptionNoArg, (XPointer) "true"},
    {"-r", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "true"},
    {"+r", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "false"},
    {"-reverse", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "true"},
    {"-rv", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "true"},
    {"+rv", "*vt100.reverseVideo", XrmoptionNoArg, (XPointer) "false"},
    {"-bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "true"},
    {"+bc", "*vt100.cursorBlink", XrmoptionNoArg, (XPointer) "false"},
    {"-bcf", "*vt100.cursorOffTime", XrmoptionSepArg, NULL},
    {"-bcn", "*vt100.cursorOnTime", XrmoptionSepArg, NULL},
    {"-log", "*logLevel", XrmoptionSepArg, NULL},
    {"-debug", "*logLevel", XrmoptionNoArg, (XPointer) "debug"},
    {"+debug", "*logLevel", XrmoptionNoArg, (XPointer) "warning"},
    {"-report-config", "*reportConfig", XrmoptionNoArg, (XPointer) "true"},
    {"-report-font-routing", "*vt100.reportFontRouting", XrmoptionNoArg, (XPointer) "true"},
    {"-T", ".title", XrmoptionSepArg, NULL},
    {"-n", "*iconName", XrmoptionSepArg, NULL},
    {"-title", ".title", XrmoptionSepArg, NULL},
    {"-w", ".borderWidth", XrmoptionSepArg, NULL},
    {"#", ".iconGeometry", XrmoptionStickyArg, NULL},
};

const int XtpCommandOptionCount = XtNumber(XtpCommandOptions);

typedef struct
{
        const char *option;
        XrmOptionKind kind;
} ToolkitOption;

typedef struct
{
        const char *option;
        const char *description;
} OptionHelp;

typedef enum
{
        OPTION_NONE,
        OPTION_RESOURCE,
        OPTION_TOOLKIT,
        OPTION_COMMAND,
        OPTION_HELP,
        OPTION_VERSION,
        OPTION_NAME,
        OPTION_CLASS,
} OptionKind;

typedef struct
{
        OptionKind kind;
        const char *option;
        const XrmOptionDescRec *resource;
        const ToolkitOption *toolkit;
} OptionMatch;

typedef struct
{
        const char *option;
        OptionKind kind;
} SpecialOption;

static const ToolkitOption toolkit_options[] = {
    {"-display", XrmoptionSepArg},     {"-selectionTimeout", XrmoptionSepArg},
    {"-synchronous", XrmoptionNoArg},  {"+synchronous", XrmoptionNoArg},
    {"-xnllanguage", XrmoptionSepArg}, {"-xrm", XrmoptionResArg},
};

static const SpecialOption special_options[] = {
    {"-e", OPTION_COMMAND}, {"-help", OPTION_HELP},   {"-version", OPTION_VERSION},
    {"-name", OPTION_NAME}, {"-class", OPTION_CLASS},
};

static const OptionHelp option_help[] = {
    {"-version", "print the version number"},
    {"--version", "print the version number"},
    {"-help", "print out this message"},
    {"--help", "print out this message"},
    {"-display displayname", "X server to contact"},
    {"-geometry geom", "size (in characters) and position"},
    {"-/+rv", "turn on/off reverse video"},
    {"-/+r", "historical reverse-video alias"},
    {"-reverse", "turn on reverse video"},
    {"-bg color", "background color"},
    {"-background color", "background color"},
    {"-fg color", "foreground color"},
    {"-foreground color", "foreground color"},
    {"-bd color", "border color"},
    {"-bordercolor color", "border color"},
    {"-bw number", "border width in pixels"},
    {"-borderwidth number", "border width in pixels"},
    {"-w number", "historical border-width alias"},
    {"-fn fontname", "normal text font"},
    {"-font fontname", "normal text font"},
    {"-fb fontname", "bold Xft fallback font"},
    {"-fwb fontname", "doublewidth bold Xft fallback font"},
    {"-fa pattern", "FreeType font-selection pattern"},
    {"-fd pattern", "FreeType doublesize font-selection pattern"},
    {"-fe pattern", "FreeType emoji font-selection pattern"},
    {"-fs size", "FreeType font size"},
    {"-iconic", "start iconic"},
    {"-name string", "client instance, icon, and title strings"},
    {"-class string", "application class string (XTerm)"},
    {"-title string", "title string"},
    {"-xrm resourcestring", "additional resource specification"},
    {"-selectionTimeout milliseconds", "Xt selection timeout"},
    {"-/+synchronous", "turn synchronous X requests on/off"},
    {"-xnllanguage string", "Xt language resource"},
    {"-b number", "internal border in pixels"},
    {"-/+ah", "turn on/off always highlight"},
    {"-/+bc", "turn on/off text cursor blinking"},
    {"-bcf milliseconds", "time text cursor is off when blinking"},
    {"-bcn milliseconds", "time text cursor is on when blinking"},
    {"-cc classrange", "specify additional character classes"},
    {"-cr color", "text cursor color"},
    {"-/+pc", "turn on/off PC-style bright bold colors"},
    {"-mc milliseconds", "multiclick time in milliseconds"},
    {"-/+sb", "turn on/off scrollbar"},
    {"-rightbar", "force scrollbar right (default left)"},
    {"-leftbar", "force scrollbar left"},
    {"-/+si", "turn on/off scroll-on-tty-output inhibit"},
    {"-/+sk", "turn on/off scroll-on-keypress"},
    {"-sl number", "number of scrolled lines to save"},
    {"-T string", "title name for window"},
    {"-n string", "icon name for window"},
    {"#geom", "icon window geometry"},
    {"-log level", "set diagnostic threshold"},
    {"-/+debug", "turn debug diagnostics on/off"},
    {"-report-config", "print resolved configuration and exit"},
    {"-report-font-routing", "report font-routing decisions"},
    {"-e command args ...", "command to execute"},
    {"--self-test", "run the installed package diagnostic"},
};

static const char *
ProgramBaseName(const char *program)
{
        const char *slash;

        if (program == NULL || *program == '\0')
                return XTP_PROGRAM_NAME;
        slash = strrchr(program, '/');
        return slash != NULL ? slash + 1 : program;
}

static bool
IsOptionPrefix(const char *argument, const char *option)
{
        size_t length = strlen(argument);

        return length >= 2 && length < strlen(option) && strncmp(argument, option, length) == 0;
}

static OptionMatch
FindOption(const char *argument)
{
        OptionMatch match = {OPTION_NONE, NULL, NULL, NULL};
        size_t matches = 0;
        size_t special;
        int index;

        for (special = 0; special < XtNumber(special_options); ++special) {
                if (strcmp(argument, special_options[special].option) == 0) {
                        match.kind = special_options[special].kind;
                        match.option = special_options[special].option;
                        return match;
                }
        }
        for (index = 0; index < XtpCommandOptionCount; ++index) {
                const XrmOptionDescRec *option = &XtpCommandOptions[index];

                if (option->argKind == XrmoptionStickyArg) {
                        size_t length = strlen(option->option);

                        if (strncmp(argument, option->option, length) == 0 &&
                            argument[length] != '\0') {
                                match.kind = OPTION_RESOURCE;
                                match.option = option->option;
                                match.resource = option;
                                return match;
                        }
                } else if (strcmp(argument, option->option) == 0) {
                        match.kind = OPTION_RESOURCE;
                        match.option = option->option;
                        match.resource = option;
                        return match;
                }
        }
        for (special = 0; special < XtNumber(toolkit_options); ++special) {
                if (strcmp(argument, toolkit_options[special].option) == 0) {
                        match.kind = OPTION_TOOLKIT;
                        match.option = toolkit_options[special].option;
                        match.toolkit = &toolkit_options[special];
                        return match;
                }
        }

        for (special = 0; special < XtNumber(special_options); ++special) {
                if (IsOptionPrefix(argument, special_options[special].option)) {
                        ++matches;
                        match.kind = special_options[special].kind;
                        match.option = special_options[special].option;
                }
        }
        for (index = 0; index < XtpCommandOptionCount; ++index) {
                const XrmOptionDescRec *option = &XtpCommandOptions[index];

                if (option->argKind != XrmoptionStickyArg &&
                    IsOptionPrefix(argument, option->option)) {
                        ++matches;
                        match.kind = OPTION_RESOURCE;
                        match.option = option->option;
                        match.resource = option;
                        match.toolkit = NULL;
                }
        }
        for (special = 0; special < XtNumber(toolkit_options); ++special) {
                if (IsOptionPrefix(argument, toolkit_options[special].option)) {
                        ++matches;
                        match.kind = OPTION_TOOLKIT;
                        match.option = toolkit_options[special].option;
                        match.resource = NULL;
                        match.toolkit = &toolkit_options[special];
                }
        }
        if (matches != 1)
                match.kind = OPTION_NONE;
        return match;
}

static int
CopyOption(XtpCommandLine *result, char **argv, int *argument, XrmOptionKind kind)
{
        result->xt_argv[result->xt_argc++] = argv[*argument];
        if (kind == XrmoptionSepArg || kind == XrmoptionResArg) {
                if (argv[*argument + 1] == NULL) {
                        result->error = XTP_COMMAND_ERROR_MISSING_VALUE;
                        result->error_option = argv[*argument];
                        return -1;
                }
                result->xt_argv[result->xt_argc++] = argv[++*argument];
        }
        return 0;
}

int
XtpScanCommandLine(int argc, char **argv, XtpCommandLine *result)
{
        int argument;

        memset(result, 0, sizeof(*result));
        result->application_name = XTP_APPLICATION_NAME;
        result->application_class = XTP_APPLICATION_CLASS;
        result->xt_argv = calloc((size_t)argc + 1U, sizeof(*result->xt_argv));
        if (result->xt_argv == NULL) {
                result->error = XTP_COMMAND_ERROR_MEMORY;
                return -1;
        }
        result->xt_argv[result->xt_argc++] = argv[0];
        for (argument = 1; argument < argc; ++argument) {
                OptionMatch match;

                if (strcmp(argv[argument], "--help") == 0) {
                        result->print_help = true;
                        continue;
                }
                if (strcmp(argv[argument], "--version") == 0) {
                        result->print_version = true;
                        continue;
                }
                if (strcmp(argv[argument], "--self-test") == 0) {
                        if (argc != 2) {
                                result->error = XTP_COMMAND_ERROR_UNKNOWN;
                                result->error_option = argv[argument];
                                return -1;
                        }
                        result->self_test = true;
                        continue;
                }

                match = FindOption(argv[argument]);
                if (match.kind == OPTION_COMMAND) {
                        if (argument + 1 >= argc) {
                                result->error = XTP_COMMAND_ERROR_MISSING_COMMAND;
                                result->error_option = argv[argument];
                                return -1;
                        }
                        result->command = &argv[argument + 1];
                        break;
                }
                if (match.kind == OPTION_HELP) {
                        result->print_help = true;
                        continue;
                }
                if (match.kind == OPTION_VERSION) {
                        result->print_version = true;
                        continue;
                }
                if (match.kind == OPTION_CLASS || match.kind == OPTION_NAME) {
                        if (argument + 1 >= argc) {
                                result->error = XTP_COMMAND_ERROR_MISSING_VALUE;
                                result->error_option = argv[argument];
                                return -1;
                        }
                        if (match.kind == OPTION_CLASS)
                                result->application_class = argv[++argument];
                        else
                                result->application_name = argv[++argument];
                        continue;
                }
                if (match.kind == OPTION_RESOURCE) {
                        if (strcmp(match.option, "-report-config") == 0)
                                result->report_config = true;
                        if (strcmp(match.option, "-debug") == 0)
                                result->early_log_level = "debug";
                        else if (strcmp(match.option, "+debug") == 0)
                                result->early_log_level = "warning";
                        else if (strcmp(match.option, "-log") == 0 && argument + 1 < argc)
                                result->early_log_level = argv[argument + 1];
                        if (strcmp(match.option, "-fa") == 0 && argument + 1 < argc)
                                result->face_name = argv[argument + 1];
                        else if (strcmp(match.option, "-fd") == 0 && argument + 1 < argc)
                                result->wide_face_name = argv[argument + 1];
                        else if (strcmp(match.option, "-fe") == 0 && argument + 1 < argc)
                                result->emoji_face_name = argv[argument + 1];
                        if (CopyOption(result, argv, &argument, match.resource->argKind) != 0)
                                return -1;
                        continue;
                }
                if (match.kind == OPTION_TOOLKIT) {
                        if (CopyOption(result, argv, &argument, match.toolkit->kind) != 0)
                                return -1;
                        continue;
                }
                result->error = XTP_COMMAND_ERROR_UNKNOWN;
                result->error_option = argv[argument];
                return -1;
        }
        result->xt_argv[result->xt_argc] = NULL;
        return 0;
}

void
XtpCommandLineDestroy(XtpCommandLine *command_line)
{
        free(command_line->xt_argv);
        command_line->xt_argv = NULL;
        command_line->xt_argc = 0;
}

static void
PrintUsage(FILE *stream, const char *program)
{
        size_t index;
        int column;

        program = ProgramBaseName(program);
        fprintf(stream, "usage:  %s", program);
        column = 8 + (int)strlen(program);
        for (index = 0; index < XtNumber(option_help); ++index) {
                int length = 3 + (int)strlen(option_help[index].option);

                if (column + length > 79) {
                        fputs("\n   ", stream);
                        column = 3;
                }
                fprintf(stream, " [%s]", option_help[index].option);
                column += length;
        }
        fputc('\n', stream);
}

void
XtpCommandPrintError(FILE *stream, const char *program, const XtpCommandLine *command_line)
{
        program = ProgramBaseName(program);
        if (command_line->error == XTP_COMMAND_ERROR_MEMORY) {
                fprintf(stream, "%s: unable to allocate command-line state\n", program);
                return;
        }
        if (command_line->error == XTP_COMMAND_ERROR_MISSING_VALUE) {
                fprintf(stream, "%s: option %s requires a value\n", program,
                        command_line->error_option);
                return;
        }
        fprintf(stream, "%s: bad command line option \"%s\"\n\n", program,
                command_line->error_option);
        PrintUsage(stream, program);
        fprintf(stream, "\nType %s -help for a full description.\n\n", program);
}

void
XtpCommandPrintHelp(FILE *stream, const char *program)
{
        size_t index;

        program = ProgramBaseName(program);
        fprintf(stream, XTP_PROGRAM_NAME " " XTP_VERSION " usage:\n");
        fprintf(stream, "    %s [-options ...] [-e command args]\n\n", program);
        fputs("where options include:\n", stream);
        for (index = 0; index < XtNumber(option_help); ++index)
                fprintf(stream, "    %-28s %s\n", option_help[index].option,
                        option_help[index].description);
        fputs("\nThe -e option, if given, must appear at the end of the command line.\n"
              "Options beginning with plus (+) restore the corresponding default.\n\n",
              stream);
}

void
XtpCommandPrintVersion(FILE *stream)
{
        fprintf(stream, XTP_PROGRAM_NAME " " XTP_VERSION "\n");
}
