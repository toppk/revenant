#include "working_directory.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool
HasScheme(const uint8_t *bytes, size_t length, size_t *scheme_length)
{
        size_t index;

        if (length == 0 || !isalpha(bytes[0]))
                return false;
        for (index = 1; index < length; ++index) {
                uint8_t byte = bytes[index];

                if (byte == ':') {
                        *scheme_length = index;
                        return true;
                }
                if (!isalnum(byte) && byte != '+' && byte != '-' && byte != '.')
                        return false;
        }
        return false;
}

static bool
LabelMatches(const char *reported, size_t reported_length, const char *local)
{
        size_t local_length = strlen(local);
        const char *dot;

        if (reported_length == local_length && strncasecmp(reported, local, local_length) == 0)
                return true;
        /* Accept the short name against a fully qualified one in either direction. */
        dot = memchr(reported, '.', reported_length);
        if (dot != NULL && (size_t)(dot - reported) == local_length &&
            strncasecmp(reported, local, local_length) == 0)
                return true;
        dot = strchr(local, '.');
        return dot != NULL && (size_t)(dot - local) == reported_length &&
               strncasecmp(reported, local, reported_length) == 0;
}

/* Only a plain registered name is accepted: no userinfo, port, escapes, or
 * delimiters, so a decoy such as local@remote cannot pass the prefix match. */
static bool
AuthorityIsWellFormed(const uint8_t *host, size_t length)
{
        size_t index;

        for (index = 0; index < length; ++index) {
                uint8_t byte = host[index];

                if (!isalnum(byte) && byte != '-' && byte != '.' && byte != '_')
                        return false;
        }
        return true;
}

static bool
HostIsLocal(const uint8_t *host, size_t length, const char *hostname)
{
        if (length == 0 || (length == 9 && strncasecmp((const char *)host, "localhost", 9) == 0))
                return true;
        return hostname != NULL && *hostname != '\0' &&
               LabelMatches((const char *)host, length, hostname);
}

static int
HexValue(uint8_t byte)
{
        if (byte >= '0' && byte <= '9')
                return byte - '0';
        if (byte >= 'a' && byte <= 'f')
                return byte - 'a' + 10;
        if (byte >= 'A' && byte <= 'F')
                return byte - 'A' + 10;
        return -1;
}

/* Copies `bytes` into a new string, decoding %XX when `decode` is set. Fails
 * on malformed escapes and on NUL or other control bytes, which cannot name a
 * directory safely. */
static char *
CopyPath(const uint8_t *bytes, size_t length, bool decode)
{
        char *path = malloc(length + 1U);
        size_t in;
        size_t out = 0;

        if (path == NULL)
                return NULL;
        for (in = 0; in < length; ++in) {
                uint8_t byte = bytes[in];

                if (decode && byte == '%') {
                        int high;
                        int low;

                        if (in + 2 >= length)
                                goto fail;
                        high = HexValue(bytes[in + 1]);
                        low = HexValue(bytes[in + 2]);
                        if (high < 0 || low < 0)
                                goto fail;
                        byte = (uint8_t)(high * 16 + low);
                        in += 2;
                }
                if (byte < 0x20 || byte == 0x7f)
                        goto fail;
                path[out++] = (char)byte;
        }
        path[out] = '\0';
        return path;
fail:
        free(path);
        return NULL;
}

XtpWorkingDirectoryStatus
XtpWorkingDirectoryDecode(const uint8_t *bytes, size_t length, const char *hostname, char **path)
{
        size_t scheme_length;
        const uint8_t *host;
        const uint8_t *slash;
        size_t host_length;
        size_t path_length;
        size_t index;

        *path = NULL;
        if (length == 0)
                return XTP_WORKING_DIRECTORY_CLEARED;
        if (bytes[0] == '/') {
                *path = CopyPath(bytes, length, false);
                return *path != NULL ? XTP_WORKING_DIRECTORY_SET : XTP_WORKING_DIRECTORY_INVALID;
        }
        if (!HasScheme(bytes, length, &scheme_length) || scheme_length != 4 ||
            strncasecmp((const char *)bytes, "file", 4) != 0)
                return XTP_WORKING_DIRECTORY_INVALID;
        if (length < 7 || bytes[5] != '/' || bytes[6] != '/')
                return XTP_WORKING_DIRECTORY_INVALID;
        host = bytes + 7;
        slash = memchr(host, '/', length - 7);
        if (slash == NULL)
                return XTP_WORKING_DIRECTORY_INVALID;
        host_length = (size_t)(slash - host);
        if (!AuthorityIsWellFormed(host, host_length))
                return XTP_WORKING_DIRECTORY_INVALID;
        if (!HostIsLocal(host, host_length, hostname))
                return XTP_WORKING_DIRECTORY_REMOTE;
        /* A query or fragment is not part of the path. */
        path_length = length - (size_t)(slash - bytes);
        for (index = 0; index < path_length; ++index) {
                if (slash[index] == '?' || slash[index] == '#') {
                        path_length = index;
                        break;
                }
        }
        if (path_length == 0)
                return XTP_WORKING_DIRECTORY_INVALID;
        *path = CopyPath(slash, path_length, true);
        return *path != NULL ? XTP_WORKING_DIRECTORY_SET : XTP_WORKING_DIRECTORY_INVALID;
}
