#include "device_attributes.h"

#include <ctype.h>

static const uint16_t features[] = {
    XTP_DA1_FEATURE_SELECTIVE_ERASE,
    XTP_DA1_FEATURE_HORIZONTAL_SCROLLING,
    XTP_DA1_FEATURE_ANSI_COLOR,
};

const uint16_t *
XtpDeviceAttributesFeatures(size_t *count)
{
        *count = sizeof(features) / sizeof(features[0]);
        return features;
}

unsigned int
XtpDeviceAttributesFirmware(const char *version)
{
        unsigned int components[3] = {0, 0, 0};
        unsigned int index;
        unsigned int firmware;

        for (index = 0; index < 3; ++index) {
                if (version == NULL || !isdigit((unsigned char)*version))
                        break;
                while (isdigit((unsigned char)*version)) {
                        unsigned int digit = (unsigned int)(*version - '0');

                        /* Any component this large already saturates the field. */
                        if (components[index] > 9999U)
                                components[index] = 9999U;
                        else
                                components[index] = components[index] * 10U + digit;
                        ++version;
                }
                if (*version != '.')
                        break;
                ++version;
        }
        if (components[1] > 99U)
                components[1] = 99U;
        if (components[2] > 99U)
                components[2] = 99U;
        firmware = components[0] * 10000U + components[1] * 100U + components[2];
        return firmware > 65535U ? 65535U : firmware;
}
