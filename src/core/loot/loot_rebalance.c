#include "core/loot/loot_rebalance.h"
#include <stdio.h>

int rogue_rarity_rebalance_scales(const int current[5], const int target[5], float out_scale[5])
{
    if (!current || !target || !out_scale)
        return -1;
    for (int i = 0; i < 5; i++)
    {
        if (current[i] <= 0)
        {
            out_scale[i] = (target[i] <= 0) ? 0.0f : 1.0f;
        }
        else
        {
            out_scale[i] = (float) target[i] / (float) current[i];
        }
    }
    return 0;
}

int rogue_rarity_rebalance_export_json(const float scales[5], char* buf, int cap)
{
    if (!scales || !buf || cap <= 0)
        return -1;
    int w = 0;
    buf[0] = '\0';
    int n = snprintf(buf + w, (w < cap ? cap - w : 0), "{\"scales\":[%.3g,%.3g,%.3g,%.3g,%.3g]}",
                     scales[0], scales[1], scales[2], scales[3], scales[4]);
    if (n > 0)
        w += n;
    if (w >= cap)
        buf[cap - 1] = '\0';
    return w;
}
