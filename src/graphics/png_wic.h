/* Minimal Windows WIC based PNG loader returning 32-bit RGBA pixel buffer. */
#ifndef ROGUE_PNG_WIC_H
#define ROGUE_PNG_WIC_H

#ifdef _WIN32
#include <stdbool.h>
#include <stddef.h>
bool rogue_png_load_rgba(const char* path, unsigned char** out_pixels, int* w, int* h);
#endif

#endif /* ROGUE_PNG_WIC_H */
