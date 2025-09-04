#ifndef ROGUE_WORLD_GEN_DUNGEON_PUZZLE_H
#define ROGUE_WORLD_GEN_DUNGEON_PUZZLE_H

#include "world_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 6: Puzzle & Traversal Subsystem (minimal, deterministic slice)
     * Contract
     * inputs: ctx (seeded), graph, assist flags
     * outputs: places traversal markers into tilemap and returns number of placements
     * success: >= 0; error -> -1
     */

    typedef enum RogueTraversalMarkerType
    {
        ROGUE_TRAVERSAL_NONE = 0,
        ROGUE_TRAVERSAL_JUMP_GLYPH = 1,
        ROGUE_TRAVERSAL_TIMED_DOOR = 2,
        ROGUE_TRAVERSAL_SECRET_PASSAGE = 3,
        ROGUE_TRAVERSAL_MOVING_PLATFORM = 4
    } RogueTraversalMarkerType;

    typedef struct RogueAssistToggles
    {
        int longer_timers;     /* 1 = extend timers for accessibility */
        int highlight_objects; /* 1 = visually hint interactables (no-op in headless tests) */
    } RogueAssistToggles;

    typedef struct RoguePuzzleTemplateDesc
    {
        char logic_type[24]; /* e.g., "timed_switch", "runes" */
        int param0;          /* generic parameter slot */
        int min_skill_req;   /* capability gate (0..N), placeholder for skill system */
    } RoguePuzzleTemplateDesc;

    /* Load a minimal puzzle template from JSON text; returns 1 on success. */
    int rogue_puzzle_template_load_json_text(const char* json_text, RoguePuzzleTemplateDesc* out,
                                             char* err, size_t err_cap);

    /* Place simple traversal markers inside each room to validate integration plumbing.
       Policy: mark the center tile of rooms tagged PUZZLE with a jump glyph and the center tile
       of treasure rooms with a timed door marker. Returns count of markers stamped. */
    int rogue_dungeon_place_traversal(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                      const RogueDungeonGraph* graph,
                                      const RogueAssistToggles* assist);

    /* Soft-lock watchdog: verifies that from start room, all objective rooms remain reachable
       given locked doors in the map (simplified: checks graph connectivity). Returns 1 if Ok, 0 if
       potential soft-lock detected. */
    int rogue_dungeon_softlock_watchdog(const RogueDungeonGraph* graph);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_DUNGEON_PUZZLE_H */
