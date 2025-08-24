#include "../../src/entities/player.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static RoguePlayer make_player(void)
{
    RoguePlayer p;
    memset(&p, 0, sizeof(p));
    p.strength = 12;
    p.dexterity = 8;
    p.vitality = 10;
    p.intelligence = 4;
    p.crit_rating = 120;
    p.haste_rating = 60;
    p.avoidance_rating = 30;
    p.crit_chance = 12;
    p.crit_damage = 160;
    p.max_health = 120;
    return p;
}

int main(void)
{
    RoguePlayer p = make_player();
    rogue_stat_cache_mark_attr_dirty();
    rogue_stat_cache_update(&p);
    unsigned int base_passive = rogue_stat_cache_heavy_passive_recompute_count();
    /* Buff dirty should not force passive recompute */
    rogue_stat_cache_mark_buff_dirty();
    rogue_stat_cache_update(&p);
    unsigned int after_buff = rogue_stat_cache_heavy_passive_recompute_count();
    assert(after_buff == base_passive); /* unchanged */
    /* Passive dirty should increment */
    rogue_stat_cache_mark_passive_dirty();
    rogue_stat_cache_update(&p);
    unsigned int after_passive = rogue_stat_cache_heavy_passive_recompute_count();
    assert(after_passive == base_passive + 1);
    size_t sz = rogue_stat_cache_sizeof();
    assert(sz < 2048); /* relaxed threshold for future fields */
    printf("progression_phase11_cache: OK\n");
    return 0;
}
