#pragma once
#include "../../overlay_theme.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY
static inline OverlayColor shade_color(OverlayColor c, int delta)
{
    int r = (int) c.r + delta;
    int g = (int) c.g + delta;
    int b = (int) c.b + delta;
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;
    OverlayColor out = {(unsigned char) r, (unsigned char) g, (unsigned char) b, c.a};
    return out;
}
#endif
