#include <stdio.h>
#include <assert.h>
#include "core/stat_cache.h"
#include "entities/player.h"

/* Minimal stub player for cache tests */
static RoguePlayer make_player(void){ RoguePlayer p; memset(&p,0,sizeof(p)); p.strength=10; p.dexterity=5; p.vitality=7; p.intelligence=3; p.crit_rating=100; p.haste_rating=50; p.avoidance_rating=25; p.crit_chance=10; p.crit_damage=150; p.max_health=100; return p; }

int main(void){
    RoguePlayer p = make_player();
    /* Mark only buff dirty -> passive heavy recompute should increment once when passives first needed */
    unsigned int before_passive = rogue_stat_cache_heavy_passive_recompute_count();
    rogue_stat_cache_mark_buff_dirty();
    rogue_stat_cache_update(&p);
    unsigned int after_passive = rogue_stat_cache_heavy_passive_recompute_count();
    assert(after_passive == before_passive + 1); /* first compute counts */
    /* Now mark only buff dirty again: passive layer should NOT recompute */
    rogue_stat_cache_mark_buff_dirty();
    rogue_stat_cache_update(&p);
    unsigned int after2 = rogue_stat_cache_heavy_passive_recompute_count();
    assert(after2 == after_passive); /* unchanged */
    /* Mark passives dirty explicitly */
    rogue_stat_cache_mark_passive_dirty();
    rogue_stat_cache_update(&p);
    unsigned int after3 = rogue_stat_cache_heavy_passive_recompute_count();
    assert(after3 == after2 + 1);
    /* Size constraint sanity (< 1KB baseline; adjustable threshold) */
    size_t sz = rogue_stat_cache_sizeof();
    assert(sz < 1024);
    printf("progression_phase11_cache: OK\n");
    return 0;
}
