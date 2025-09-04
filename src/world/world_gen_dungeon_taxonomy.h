#ifndef ROGUE_WORLD_GEN_DUNGEON_TAXONOMY_H
#define ROGUE_WORLD_GEN_DUNGEON_TAXONOMY_H

/* Forward decls to avoid header cycles; consumers may include world_gen.h separately. */
enum RogueBiomeId; /* defined in world_gen.h */

/* Phase 0: Core Dungeon Taxonomy & ID Ranges */

/* Dungeon archetypes enumerate high-level layout intents. */
typedef enum RogueDungeonArchetype
{
    ROGUE_DUNGEON_ARCH_LINEAR = 0,
    ROGUE_DUNGEON_ARCH_BRANCHING,
    ROGUE_DUNGEON_ARCH_LOOPING,
    ROGUE_DUNGEON_ARCH_HUB,
    ROGUE_DUNGEON_ARCH_GAUNTLET,
    ROGUE_DUNGEON_ARCH_PUZZLE,
    ROGUE_DUNGEON_ARCH_ARENA,
    ROGUE_DUNGEON_ARCHETYPE_MAX
} RogueDungeonArchetype;

/* Human-readable name for an archetype. Returns "UNKNOWN" if out of range. */
const char* rogue_dungeon_archetype_name(RogueDungeonArchetype a);

/* Reserved ID ranges for templates & events to prevent collisions. */
#define ROGUE_DUNGEON_TEMPLATE_ID_BASE 1000
#define ROGUE_DUNGEON_TEMPLATE_ID_MAX 1999
#define ROGUE_DUNGEON_EVENT_ID_BASE 2000
#define ROGUE_DUNGEON_EVENT_ID_MAX 2199
#define ROGUE_DUNGEON_OBJECTIVE_ID_BASE 3000
#define ROGUE_DUNGEON_OBJECTIVE_ID_MAX 3099

/* ID validation helpers (1=valid, 0=invalid) */
int rogue_dungeon_is_valid_template_id(int id);
int rogue_dungeon_is_valid_event_id(int id);
int rogue_dungeon_is_valid_objective_id(int id);

/* Difficulty target: map dungeon depth (0..N) to desired enemy relative level delta. */
int rogue_dungeon_target_level_delta(int depth);

/* Thematic tag bitmask derived from surrounding biome (coarse mapping). */
#define ROGUE_DUNGEON_THEME_DAMP 0x00000001u
#define ROGUE_DUNGEON_THEME_ICY 0x00000002u
#define ROGUE_DUNGEON_THEME_VOLCANIC 0x00000004u
#define ROGUE_DUNGEON_THEME_RUIN 0x00000008u
#define ROGUE_DUNGEON_THEME_FORESTED 0x00000010u
#define ROGUE_DUNGEON_THEME_SWAMP 0x00000020u
#define ROGUE_DUNGEON_THEME_MOUNTAIN 0x00000040u
#define ROGUE_DUNGEON_THEME_DEFAULT 0x80000000u

/* Map world biome to default dungeon thematic tags (bitmask). */
unsigned int rogue_dungeon_biome_theme_tags(enum RogueBiomeId biome);

#endif /* ROGUE_WORLD_GEN_DUNGEON_TAXONOMY_H */
