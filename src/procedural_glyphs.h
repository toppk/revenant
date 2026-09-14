#ifndef XTERM_PLUS_PROCEDURAL_GLYPHS_H
#define XTERM_PLUS_PROCEDURAL_GLYPHS_H

#include <stdbool.h>
#include <stdint.h>

bool XtpProceduralExtendedCodepoint(uint32_t codepoint);
bool XtpProceduralExtendedRasterize(uint32_t codepoint, unsigned int width, unsigned int height,
                                    unsigned int thickness, uint8_t *mask);

#endif
