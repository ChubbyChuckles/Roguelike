/* Tests for advanced rarity features 5.5-5.8 */
#include <stdio.h>
#include "core/loot/loot_rarity_adv.h"
#include "core/loot/loot_tables.h"
#include "../../src/core/loot/loot_dynamic_weights.h"

static int fail(const char* m){ printf("FAIL:%s\n", m); return 1; }

int main(){
    rogue_rarity_adv_reset();
    /* 5.5 spawn sound mapping */
    if(rogue_rarity_set_spawn_sound(2, "rare_spawn")!=0) return fail("set_spawn_sound");
    if(!rogue_rarity_get_spawn_sound(2)) return fail("get_spawn_sound_null");
    if(rogue_rarity_get_spawn_sound(2)[0] != 'r') return fail("spawn_sound_value");
    /* 5.6 despawn override */
    if(rogue_rarity_set_despawn_ms(4, 12345)!=0) return fail("set_despawn");
    if(rogue_rarity_get_despawn_ms(4)!=12345) return fail("get_despawn");
    /* 5.7 floor application */
    rogue_rarity_set_min_floor(2);
    unsigned int rng=123u; int ups=0; for(int i=0;i<100;i++){ int r=rogue_loot_rarity_sample(&rng,0,4); if(r<2) return fail("floor_not_applied"); if(r==2) ups++; }
    if(ups==0) return fail("floor_never_hit");
    /* 5.8 pity thresholds: force upgrade after N commons */
    rogue_rarity_adv_reset();
    rogue_rarity_pity_set_thresholds(3,0); /* after 3 sub-epic (rarity<3) should upgrade to epic */
    rng=5u; int saw_epic=0; for(int i=0;i<20;i++){ int r=rogue_loot_rarity_sample(&rng,0,4); if(r==3){ saw_epic=1; break; } }
    if(!saw_epic) return fail("pity_never_triggered");
    printf("RARITY_ADV_OK\n");
    return 0;
}
