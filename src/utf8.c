#include "utf8.h"

bool
XtpUtf8Decode(const char *text, size_t length, uint32_t *codepoint, size_t *consumed)
{
        const unsigned char *bytes = (const unsigned char *)text;
        uint32_t value;
        size_t need;
        size_t index;

        if (text == NULL || length == 0 || codepoint == NULL || consumed == NULL)
                return false;
        if (bytes[0] < 0x80U) {
                *codepoint = bytes[0];
                *consumed = 1;
                return true;
        }
        if ((bytes[0] & 0xe0U) == 0xc0U) {
                value = bytes[0] & 0x1fU;
                need = 2;
        } else if ((bytes[0] & 0xf0U) == 0xe0U) {
                value = bytes[0] & 0x0fU;
                need = 3;
        } else if ((bytes[0] & 0xf8U) == 0xf0U) {
                value = bytes[0] & 0x07U;
                need = 4;
        } else {
                return false;
        }
        if (need > length)
                return false;
        for (index = 1; index < need; ++index) {
                if ((bytes[index] & 0xc0U) != 0x80U)
                        return false;
                value = (value << 6) | (bytes[index] & 0x3fU);
        }
        if ((need == 2 && value < 0x80U) || (need == 3 && value < 0x800U) ||
            (need == 4 && value < 0x10000U) || value > 0x10ffffU ||
            (value >= 0xd800U && value <= 0xdfffU))
                return false;
        *codepoint = value;
        *consumed = need;
        return true;
}
