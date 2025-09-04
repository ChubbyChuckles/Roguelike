#include "world_gen.h" /* brings in RogueBiomeId and includes taxonomy header */

const char* rogue_dungeon_archetype_name(RogueDungeonArchetype a)
{
    switch (a)
    {
    case ROGUE_DUNGEON_ARCH_LINEAR:
        return "Linear";
    case ROGUE_DUNGEON_ARCH_BRANCHING:
        return "Branching";
    case ROGUE_DUNGEON_ARCH_LOOPING:
        return "Looping";
    case ROGUE_DUNGEON_ARCH_HUB:
        return "Hub";
    case ROGUE_DUNGEON_ARCH_GAUNTLET:
        return "Gauntlet";
    case ROGUE_DUNGEON_ARCH_PUZZLE:
        return "Puzzle";
    case ROGUE_DUNGEON_ARCH_ARENA:
        return "Arena";
    default:
        return "UNKNOWN";
    }
}

int rogue_dungeon_is_valid_template_id(int id)
{
    return (id >= ROGUE_DUNGEON_TEMPLATE_ID_BASE && id <= ROGUE_DUNGEON_TEMPLATE_ID_MAX);
}

int rogue_dungeon_is_valid_event_id(int id)
{
    return (id >= ROGUE_DUNGEON_EVENT_ID_BASE && id <= ROGUE_DUNGEON_EVENT_ID_MAX);
}

int rogue_dungeon_is_valid_objective_id(int id)
{
    return (id >= ROGUE_DUNGEON_OBJECTIVE_ID_BASE && id <= ROGUE_DUNGEON_OBJECTIVE_ID_MAX);
}

/* Simple monotonic, saturating mapping for early phases: every 3 depths adds +1 until +8. */
int rogue_dungeon_target_level_delta(int depth)
{
    if (depth <= 0)
        return 0;
    int delta = depth / 3; /* 0,0,0,1,1,1,2,2,2, ... */
    if (delta > 8)
        delta = 8;
    return delta;
}

unsigned int rogue_dungeon_biome_theme_tags(enum RogueBiomeId biome)
{
    switch (biome)
    {
    case ROGUE_BIOME_OCEAN:
        return ROGUE_DUNGEON_THEME_DAMP;
    case ROGUE_BIOME_PLAINS:
        return ROGUE_DUNGEON_THEME_RUIN;
    case ROGUE_BIOME_FOREST_BIOME:
        return ROGUE_DUNGEON_THEME_FORESTED;
    case ROGUE_BIOME_MOUNTAIN_BIOME:
        return ROGUE_DUNGEON_THEME_MOUNTAIN;
    case ROGUE_BIOME_SNOW_BIOME:
        return ROGUE_DUNGEON_THEME_ICY;
    case ROGUE_BIOME_SWAMP_BIOME:
        return ROGUE_DUNGEON_THEME_SWAMP | ROGUE_DUNGEON_THEME_DAMP;
    default:
        return ROGUE_DUNGEON_THEME_DEFAULT;
    }
}
