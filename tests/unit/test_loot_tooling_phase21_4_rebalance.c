#include "../../src/core/loot/loot_rebalance.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    int current[5] = {100, 50, 25, 10, 5};
    int target[5] = {80, 60, 30, 15, 5};
    float scales[5];
    if (rogue_rarity_rebalance_scales(current, target, scales) != 0)
    {
        fprintf(stderr, "FAIL scales compute\n");
        return 1;
    }
    if (scales[0] < 0.79f || scales[0] > 0.81f)
        return 2;
    if (scales[1] < 1.19f || scales[1] > 1.21f)
        return 3;
    if (scales[3] < 1.49f || scales[3] > 1.51f)
        return 4;
    char json[128];
    int len = rogue_rarity_rebalance_export_json(scales, json, sizeof json);
    if (len <= 0)
    {
        fprintf(stderr, "FAIL export json\n");
        return 5;
    }
    if (strstr(json, "0.8") == NULL || strstr(json, "1.5") == NULL)
    {
        fprintf(stderr, "FAIL json content %s\n", json);
        return 6;
    }
    printf("loot_tooling_phase21_4_rebalance_ok %s\n", json);
    return 0;
}
