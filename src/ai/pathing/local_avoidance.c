#include "ai/pathing/local_avoidance.h"
#include "core/navigation.h"

int rogue_local_avoid_adjust(int x, int y, int* dx, int* dy)
{
    if (!dx || !dy)
        return -1;
    int ndx = *dx, ndy = *dy;
    int tx = x + ndx, ty = y + ndy;
    if (!rogue_nav_is_blocked(tx, ty))
        return 0; /* original is fine */
    /* Try perpendicular detours (left/right relative to desired) */
    int detours[2][2];
    if (ndx != 0)
    {
        detours[0][0] = 0;
        detours[0][1] = 1;
        detours[1][0] = 0;
        detours[1][1] = -1;
    }
    else if (ndy != 0)
    {
        detours[0][0] = 1;
        detours[0][1] = 0;
        detours[1][0] = -1;
        detours[1][1] = 0;
    }
    else
    {
        return -1;
    }
    for (int i = 0; i < 2; ++i)
    {
        int ddx = detours[i][0], ddy = detours[i][1];
        if (!rogue_nav_is_blocked(x + ddx, y + ddy))
        {
            *dx = ddx;
            *dy = ddy;
            return 1;
        }
    }
    /* As last resort, try staying put (no move) to avoid stuck */
    *dx = 0;
    *dy = 0;
    return -1;
}
