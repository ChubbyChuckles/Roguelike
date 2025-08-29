/**
 * @file local_avoidance.c
 * @brief Simple local obstacle avoidance for AI pathfinding.
 *
 * This module provides basic obstacle avoidance functionality for AI agents.
 * When an agent's desired movement is blocked, it attempts to find alternative
 * paths by trying perpendicular detours (left/right relative to the desired direction).
 * As a last resort, it allows the agent to stay in place to prevent getting stuck.
 */

#include "local_avoidance.h"
#include "../../game/navigation.h"

/**
 * @brief Adjusts movement direction to avoid obstacles using perpendicular detours.
 *
 * This function implements a simple local obstacle avoidance algorithm. When the
 * desired movement direction is blocked, it attempts to find an alternative path
 * by trying perpendicular detours (left/right relative to the desired direction).
 * As a last resort, it allows the agent to stay in place to prevent getting stuck.
 *
 * The algorithm works by:
 * 1. Checking if the original movement direction is clear
 * 2. If blocked, trying perpendicular alternatives (left/right)
 * 3. If no alternatives work, staying put (no movement)
 *
 * @param x Current X position of the agent.
 * @param y Current Y position of the agent.
 * @param dx Pointer to desired X movement delta (input/output).
 * @param dy Pointer to desired Y movement delta (input/output).
 * @return 0 if original direction is clear, 1 if detour found, -1 if blocked (staying put).
 */
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
