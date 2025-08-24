#define SDL_MAIN_HANDLED 1
#include "../../src/core/loot/loot_affixes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    int added = rogue_affixes_load_from_cfg("assets/affixes.cfg");
    assert(added > 0);
    int idx_fire = rogue_affix_index("embered");
    int idx_frost = rogue_affix_index("glacial");
    int idx_arc = rogue_affix_index("eldritch");
    int idx_rec = rogue_affix_index("of_resurgence");
    int idx_thp = rogue_affix_index("of_spines");
    int idx_thc = rogue_affix_index("of_rebuke");
    assert(idx_fire >= 0 && idx_frost >= 0 && idx_arc >= 0 && idx_rec >= 0 && idx_thp >= 0 &&
           idx_thc >= 0);
    const RogueAffixDef* a = rogue_affix_at(idx_fire);
    assert(a && a->min_value >= 5 && a->max_value <= 15);
    printf("equipment_phase7_affix_parsing_newstats_ok\n");
    return 0;
}
