#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_dungeon_taxonomy.h"
#include <stdio.h>

static int expect(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "%s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    /* Archetype enumeration and names */
    for (int a = 0; a < ROGUE_DUNGEON_ARCHETYPE_MAX; ++a)
    {
        const char* n = rogue_dungeon_archetype_name((RogueDungeonArchetype) a);
        if (!expect(n && n[0] != '\0', "archetype name empty"))
            return 1;
    }

    /* ID ranges should accept in-range and reject out-of-range */
    if (!expect(rogue_dungeon_is_valid_template_id(ROGUE_DUNGEON_TEMPLATE_ID_BASE),
                "template base invalid"))
        return 1;
    if (!expect(!rogue_dungeon_is_valid_template_id(ROGUE_DUNGEON_TEMPLATE_ID_BASE - 1),
                "template below base valid"))
        return 1;
    if (!expect(rogue_dungeon_is_valid_event_id(ROGUE_DUNGEON_EVENT_ID_BASE), "event base invalid"))
        return 1;
    if (!expect(!rogue_dungeon_is_valid_event_id(ROGUE_DUNGEON_EVENT_ID_MAX + 1),
                "event above max valid"))
        return 1;
    if (!expect(rogue_dungeon_is_valid_objective_id(ROGUE_DUNGEON_OBJECTIVE_ID_MAX),
                "objective max invalid"))
        return 1;

    /* Depth->delta should be monotonic and bounded */
    int prev = -1;
    for (int d = 0; d <= 50; ++d)
    {
        int delta = rogue_dungeon_target_level_delta(d);
        if (!expect(delta >= 0 && delta <= 8, "delta out of bounds"))
            return 1;
        if (!expect(delta >= prev, "delta not monotonic"))
            return 1;
        prev = delta;
    }

    /* Biome theme tags must always return some tag (non-zero) */
    for (int b = 0; b < ROGUE_BIOME_MAX; ++b)
    {
        unsigned int t = rogue_dungeon_biome_theme_tags((enum RogueBiomeId) b);
        if (!expect(t != 0u, "biome theme tag zero"))
            return 1;
    }

    printf("phase0 dungeon taxonomy tests passed\n");
    return 0;
}
