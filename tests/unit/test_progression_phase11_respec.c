/* Phase 11.5: Ensure no stale cache after passive re-spec (reload + different DSL) */
#include "../../src/core/progression/progression_maze.h"
#include "../../src/core/progression/progression_passives.h"
#include "../../src/core/progression/progression_stats.h"
#include "../../src/entities/player.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void build_maze(struct RogueProgressionMaze* m)
{
    memset(m, 0, sizeof(*m));
    m->base.node_count = 2;
}
static RoguePlayer make_player(void)
{
    RoguePlayer p;
    memset(&p, 0, sizeof(p));
    p.strength = 10;
    p.dexterity = 5;
    p.vitality = 4;
    p.intelligence = 3;
    p.crit_rating = 50;
    p.haste_rating = 25;
    p.avoidance_rating = 10;
    p.crit_chance = 5;
    p.crit_damage = 150;
    p.max_health = 100;
    return p;
}

int main(void)
{
    struct RogueProgressionMaze maze;
    build_maze(&maze);
    assert(rogue_progression_passives_init(&maze) == 0);
    const char* dslA = "0 STR+5\n1 DEX+3\n";
    const char* dslB = "0 STR+2\n1 DEX+8\n";
    assert(rogue_progression_passives_load_dsl(dslA) == 0);
    rogue_progression_passive_unlock(0, 1, 1, 0, 0, 0, 0);
    rogue_progression_passive_unlock(1, 2, 1, 0, 0, 0, 0);
    RoguePlayer p = make_player();
    rogue_stat_cache_mark_passive_dirty();
    rogue_stat_cache_update(&p);
    unsigned long long fpA = rogue_stat_cache_fingerprint();
    /* Lookup stat IDs dynamically */
    size_t def_count = 0;
    const RogueStatDef* defs = rogue_stat_def_all(&def_count);
    int str_id = -1, dex_id = -1;
    for (size_t i = 0; i < def_count; i++)
    {
        if (strcmp(defs[i].code, "STR") == 0)
            str_id = defs[i].id;
        if (strcmp(defs[i].code, "DEX") == 0)
            dex_id = defs[i].id;
    }
    assert(str_id >= 0 && dex_id >= 0);
    int strA = rogue_progression_passives_stat_total(str_id);
    int dexA = rogue_progression_passives_stat_total(dex_id);
    assert(rogue_progression_passives_reload(&maze, dslB, 1, 0, 0, 0, 0) == 0);
    rogue_stat_cache_mark_passive_dirty();
    rogue_stat_cache_update(&p);
    unsigned long long fpB = rogue_stat_cache_fingerprint();
    int strB = rogue_progression_passives_stat_total(str_id);
    int dexB = rogue_progression_passives_stat_total(dex_id);
    assert(fpA != fpB);
    assert((strA != strB) || (dexA != dexB));
    printf("progression_phase11_respec: OK\n");
    return 0;
}
