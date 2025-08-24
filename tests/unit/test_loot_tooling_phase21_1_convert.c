#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_item_defs_convert.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* tsv = "temp_items.tsv";
    const char* csv = "temp_items_converted.csv";
    FILE* f = fopen(tsv, "wb");
    if (!f)
    {
        fprintf(stderr, "FAIL open tsv\n");
        return 1;
    }
    fprintf(f, "gold_coin\tGold Coin\t0\t0\t100\t1\t0\t0\t0\t../assets/sheet.png\t1\t1\t1\t1\t0\n");
    fprintf(f, "bandage\tBandage\t1\t0\t10\t5\t0\t0\t0\t../assets/sheet.png\t2\t1\t1\t1\n");
    fprintf(f, "# ignore this line\n\n");
    fclose(f);
    int c = rogue_item_defs_convert_tsv_to_csv(tsv, csv);
    if (c != 2)
    {
        fprintf(stderr, "FAIL expected 2 converted got %d\n", c);
        return 2;
    }
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg(csv);
    if (added != 2)
    {
        fprintf(stderr, "FAIL expected 2 added got %d\n", added);
        return 3;
    }
    const RogueItemDef* def0 = rogue_item_def_by_id("gold_coin");
    const RogueItemDef* def1 = rogue_item_def_by_id("bandage");
    if (!def0 || !def1)
    {
        fprintf(stderr, "FAIL defs missing after load\n");
        return 4;
    }
    if (def0->rarity != 0)
    {
        fprintf(stderr, "FAIL rarity mismatch %d\n", def0->rarity);
        return 5;
    }
    remove(tsv);
    remove(csv);
    printf("loot_tooling_phase21_1_convert_ok converted=%d added=%d\n", c, added);
    return 0;
}
