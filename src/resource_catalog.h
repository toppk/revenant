#ifndef XTERM_PLUS_RESOURCE_CATALOG_H
#define XTERM_PLUS_RESOURCE_CATALOG_H

#include <stddef.h>

typedef enum
{
        XTP_RESOURCE_APPLICATION,
        XTP_RESOURCE_VT100,
        XTP_RESOURCE_TEK4014,
        XTP_RESOURCE_FONT_SUBRESOURCE,
        XTP_RESOURCE_APPLICATION_CONDITIONAL,
        XTP_RESOURCE_VT100_CONDITIONAL,
        XTP_RESOURCE_TEK4014_CONDITIONAL,
} XtpResourceScope;

typedef struct
{
        XtpResourceScope scope;
        const char *name;
        const char *class_name;
        const char *default_value;
} XtpResourceCatalogEntry;

extern const XtpResourceCatalogEntry xtp_xterm_411_resources[];
extern const size_t xtp_xterm_411_resource_count;
extern const char *const xtp_xterm_411_actions[];
extern const size_t xtp_xterm_411_action_count;
extern const char *const xtp_xterm_411_app_defaults[];
extern const size_t xtp_xterm_411_app_default_count;

#endif
