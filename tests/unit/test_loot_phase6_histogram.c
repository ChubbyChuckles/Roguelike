#include "core/loot/loot_console.h"
#include "core/loot/loot_rarity.h"
#include "core/loot/loot_stats.h"
#include <stdio.h>
#include <string.h>

/* Simple unit test for histogram formatting (6.4) */
int main()
{
    /* seed some counts by simulating adds */
    for (int i = 0; i < 5; i++)
        rogue_loot_stats_record_rarity(0); /* common */
    for (int i = 0; i < 3; i++)
        rogue_loot_stats_record_rarity(1); /* uncommon */
    rogue_loot_stats_record_rarity(4);     /* legendary */
    char buf[256];
    int n = rogue_loot_histogram_format(buf, sizeof(buf));
    if (n < 0)
    {
        puts("HISTOGRAM_FAIL format error");
        return 1;
    }
    if (!strstr(buf, "COMMON:5") || !strstr(buf, "UNCOMMON:3") || !strstr(buf, "LEGENDARY:1") ||
        !strstr(buf, "TOTAL:9"))
    {
        puts("HISTOGRAM_FAIL wrong counts");
        puts(buf);
        return 1;
    }
    puts("HISTOGRAM_OK");
    return 0;
}
