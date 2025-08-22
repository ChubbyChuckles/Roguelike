#include "core/progression/progression_maze.h"
#include "core/progression/progression_passives.h"
#include <stdio.h>
#include <string.h>

/* Minimal DSL mapping node 0 -> +STR+1, node 1 -> +DEX+1 to ensure effects present */
static const char* k_passive_dsl = "0 STR+1\n1 DEX+1\n";

int main(void)
{
    RogueProgressionMaze mz;
    memset(&mz, 0, sizeof mz);
    /* Build progression maze from default config (search helper inside generator). */
    if (!rogue_progression_maze_build("assets/skill_maze_config.json", &mz))
    {
        printf("PH3_6_MAZE_UNLOCKS_ERR no_maze\n");
        return 1;
    }
    if (rogue_progression_passives_init(&mz) != 0)
    {
        printf("PH3_6_MAZE_UNLOCKS_ERR init\n");
        rogue_progression_maze_free(&mz);
        return 1;
    }
    if (rogue_progression_passives_load_dsl(k_passive_dsl) != 0)
    {
        printf("PH3_6_MAZE_UNLOCKS_ERR dsl\n");
        rogue_progression_passives_shutdown();
        rogue_progression_maze_free(&mz);
        return 1;
    }
    /* Pick two nodes: node 0 (typically inner ring -> low req) and a farther node (last index) */
    int inner = 0;
    int outer = mz.base.node_count > 1 ? (mz.base.node_count - 1) : 0;
    int lvl = 1, str = 1, dex = 1, intel = 1, vit = 1;

    int ok_inner_gate =
        rogue_progression_maze_node_unlockable(&mz, inner, lvl, str, dex, intel, vit);
    int ok_outer_gate =
        rogue_progression_maze_node_unlockable(&mz, outer, lvl, str, dex, intel, vit);

    /* Attempt to unlock both with low stats. Expect inner possibly ok, outer likely gated. */
    int r1 = rogue_progression_passive_unlock(inner, 123u, lvl, str, dex, intel, vit);
    int r2 = rogue_progression_passive_unlock(outer, 124u, lvl, str, dex, intel, vit);

    /* Now boost requirements to ensure outer becomes unlockable. */
    int need_level = mz.meta[outer].level_req;
    int need_str = mz.meta[outer].str_req;
    int need_dex = mz.meta[outer].dex_req;
    int need_int = mz.meta[outer].int_req;
    int need_vit = mz.meta[outer].vit_req;

    lvl = need_level;
    str = need_str;
    dex = need_dex;
    intel = need_int;
    vit = need_vit;
    int ok_outer_after =
        rogue_progression_maze_node_unlockable(&mz, outer, lvl, str, dex, intel, vit);
    int r2b = rogue_progression_passive_unlock(outer, 125u, lvl, str, dex, intel, vit);

    printf("PH3_6_MAZE_UNLOCKS_OK inner_g=%d outer_g=%d r1=%d r2=%d after_g=%d r2b=%d\n",
           ok_inner_gate, ok_outer_gate, r1, r2, ok_outer_after, r2b);

    rogue_progression_passives_shutdown();
    rogue_progression_maze_free(&mz);
    return 0;
}
