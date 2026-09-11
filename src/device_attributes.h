#ifndef XTERM_PLUS_DEVICE_ATTRIBUTES_H
#define XTERM_PLUS_DEVICE_ATTRIBUTES_H

#include <stddef.h>
#include <stdint.h>

/* Claimed identity; every DA1 code needs rendered-cell evidence (see the drift ledger). */
#define XTP_DA1_CONFORMANCE_LEVEL 62U
#define XTP_DA1_FEATURE_SELECTIVE_ERASE 6U
#define XTP_DA1_FEATURE_HORIZONTAL_SCROLLING 21U
#define XTP_DA1_FEATURE_ANSI_COLOR 22U
#define XTP_DA2_DEVICE_TYPE 1U
#define XTP_DA3_UNIT_ID 0U

#define XTP_DA1_REPLY "\033[?62;6;21;22c"
#define XTP_DA3_REPLY "\033P!|00000000\033\\"

/* Ordered DA1 feature codes; `count` receives the number of entries. */
const uint16_t *XtpDeviceAttributesFeatures(size_t *count);

/* major * 10000 + minor * 100 + patch of "M.m.p[-suffix]", clamped to 16 bits. */
unsigned int XtpDeviceAttributesFirmware(const char *version);

#endif
