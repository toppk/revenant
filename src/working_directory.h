#ifndef XTERM_PLUS_WORKING_DIRECTORY_H
#define XTERM_PLUS_WORKING_DIRECTORY_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
        XTP_WORKING_DIRECTORY_SET,
        XTP_WORKING_DIRECTORY_CLEARED,
        XTP_WORKING_DIRECTORY_REMOTE,
        XTP_WORKING_DIRECTORY_INVALID,
} XtpWorkingDirectoryStatus;

/* Decodes an OSC 7 file URI or a bare absolute path (OSC 9/1337 form) into a
 * local path. `hostname` is this machine's name; NULL accepts only an empty
 * host or localhost. On SET the caller frees *path. */
XtpWorkingDirectoryStatus XtpWorkingDirectoryDecode(const uint8_t *bytes, size_t length,
                                                    const char *hostname, char **path);

#endif
