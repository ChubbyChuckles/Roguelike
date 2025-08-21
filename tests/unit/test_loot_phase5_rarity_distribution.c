#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_tables.h"
#include "core/path_utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct DistSpec
{
    const char* table_id;
    const char* item_id;
    int allow_min;
    int allow_max;
} DistSpec;

int main(void)
{
    rogue_drop_rates_reset();
    rogue_loot_dyn_reset();
    rogue_item_defs_reset();
    char pitems[256];
    assert(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems));
    int items = rogue_item_defs_load_from_cfg(pitems);
    assert(items > 0);
    rogue_loot_tables_reset();
    char ptables[256];
    assert(rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables));
    int tables = rogue_loot_tables_load_from_cfg(ptables);
    assert(tables > 0);

    DistSpec specs[] = {
        {"ORC_BASE", "long_sword", 0, 2},
        {"ORC_WARRIOR", "long_sword", 1, 2},
        {"SKELETON_BASE", "magic_staff", 2, 2},
        {"SKELETON_WARRIOR", "epic_axe", 3, 3},
    };
    const int spec_count = (int) (sizeof specs / sizeof specs[0]);

    for (int si = 0; si < spec_count; ++si)
    {
        DistSpec* s = &specs[si];
        int tbl = rogue_loot_table_index(s->table_id);
        assert(tbl >= 0);
        int item_index = rogue_item_def_index(s->item_id);
        assert(item_index >= 0);
        unsigned int seed = 12345u + (unsigned) si * 777u;
        int observed_mask = 0; /* bit i set if rarity (allow_min + i) seen */
        int iterations =
            3000; /* chosen to make rare item + rarity band coverage extremely likely */
        for (int it = 0; it < iterations; ++it)
        {
            unsigned int local = seed + (unsigned) it * 17u;
            int idef[8];
            int qty[8];
            int rar[8];
            int drops = rogue_loot_roll_ex(tbl, &local, 8, idef, qty, rar);
            for (int d = 0; d < drops; ++d)
            {
                if (idef[d] == item_index && rar[d] != -1)
                {
                    assert(rar[d] >= s->allow_min && rar[d] <= s->allow_max);
                    int bit = rar[d] - s->allow_min;
                    if (bit >= 0 && bit < 8)
                        observed_mask |= (1 << bit);
                }
            }
        }
        /* Ensure every rarity in the allowed band appeared at least once. */
        int needed_bits = (s->allow_max - s->allow_min + 1);
        int have = 0;
        for (int b = 0; b < needed_bits; b++)
        {
            if (observed_mask & (1 << b))
                have++;
        }
        assert(have == needed_bits);
        printf("RARITY_DIST_OK table=%s item=%s band=%d-%d\n", s->table_id, s->item_id,
               s->allow_min, s->allow_max);
    }
    return 0;
}
