#ifndef XTERM_PLUS_COMMAND_OPTIONS_H
#define XTERM_PLUS_COMMAND_OPTIONS_H

#include <X11/Xresource.h>

#include <stdbool.h>
#include <stdio.h>

#define XTP_APPLICATION_NAME "xterm"
#define XTP_APPLICATION_CLASS "XTerm"

typedef enum
{
        XTP_COMMAND_ERROR_NONE,
        XTP_COMMAND_ERROR_UNKNOWN,
        XTP_COMMAND_ERROR_MISSING_VALUE,
        XTP_COMMAND_ERROR_MISSING_COMMAND,
        XTP_COMMAND_ERROR_CONFLICT,
        XTP_COMMAND_ERROR_MEMORY,
} XtpCommandError;

typedef struct
{
        int xt_argc;
        char **xt_argv;
        char **command;
        const char *application_name;
        const char *application_class;
        const char *early_log_level;
        const char *face_name;
        const char *wide_face_name;
        const char *emoji_face_name;
        const char *error_option;
        XtpCommandError error;
        bool report_config;
        bool welcome;
        bool print_help;
        bool print_version;
        bool self_test;
} XtpCommandLine;

extern XrmOptionDescRec XtpCommandOptions[];
extern const int XtpCommandOptionCount;

int XtpScanCommandLine(int argc, char **argv, XtpCommandLine *result);
void XtpCommandLineDestroy(XtpCommandLine *command_line);
void XtpCommandPrintError(FILE *stream, const char *program, const XtpCommandLine *command_line);
void XtpCommandPrintHelp(FILE *stream, const char *program);
void XtpCommandPrintVersion(FILE *stream);

#endif
