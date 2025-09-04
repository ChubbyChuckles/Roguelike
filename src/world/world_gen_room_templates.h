#ifndef ROGUE_WORLD_GEN_ROOM_TEMPLATES_H
#define ROGUE_WORLD_GEN_ROOM_TEMPLATES_H

#include "tilemap.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Size class hint for selection heuristics (optional) */
    typedef enum RogueRoomSizeClass
    {
        ROGUE_ROOM_SMALL = 0,
        ROGUE_ROOM_MEDIUM = 1,
        ROGUE_ROOM_LARGE = 2
    } RogueRoomSizeClass;

    typedef struct RogueRoomExit
    {
        int x;   /* local template coord */
        int y;   /* local template coord */
        int dir; /* 0=up,1=right,2=down,3=left (hint) */
    } RogueRoomExit;

    /* Door metadata (Phase 2.3): carry type and optional key id for locked doors */
    typedef enum RogueDoorType
    {
        ROGUE_DOOR_NORMAL = 0,
        ROGUE_DOOR_LOCKED = 1,
        ROGUE_DOOR_SECRET = 2
    } RogueDoorType;

    typedef struct RogueRoomDoor
    {
        int x; /* local template coord */
        int y; /* local template coord */
        RogueDoorType type;
        int key_id; /* optional key requirement; -1 if not applicable */
    } RogueRoomDoor;

    /* Environmental deco marker (Phase 2.4): semantic label at a local coord */
    typedef struct RogueDecoMarker
    {
        int x;         /* local template coord */
        int y;         /* local template coord */
        char kind[24]; /* small label e.g., "pillar", "banner" */
    } RogueDecoMarker;

    typedef struct RogueRoomTemplate
    {
        int id;                 /* template id */
        RogueRoomSizeClass cls; /* size class */
        int width;              /* template grid width */
        int height;             /* template grid height */
        /* ASCII grid, row-major, len = width*height.
           '#' wall, '.' floor, 'D' door, 'L' locked, 'S' secret, 'T' trap, 'K' key, 'E' exit */
        char* grid;
        /* Optional exits parsed from 'E' markers */
        RogueRoomExit exits[16];
        int exit_count;
        /* Doors parsed from ASCII ('D','L','S') or provided by JSON */
        RogueRoomDoor doors[32];
        int door_count;
        /* Aggregated biome/theme tags (comma-separated) when loaded via JSON (optional) */
        char biome_tags[128];
        /* Optional counts for encounter/hazard/puzzle slots (from JSON) */
        int encounter_slots;
        int hazard_slots;
        int puzzle_slot;
        /* Optional environmental deco markers (from JSON) */
        RogueDecoMarker deco[32];
        int deco_count;
    } RogueRoomTemplate;

    /* Build a template from ASCII rows (rows[h], each with equal length w). Returns 0 on success.
     */
    int rogue_room_template_from_ascii(const char** rows, int h, int id, RogueRoomSizeClass cls,
                                       RogueRoomTemplate* out);

    /* Free internals allocated by from_ascii */
    void rogue_room_template_free(RogueRoomTemplate* t);

    /* Compute rotated/reflected dimensions for placement. rot in {0,90,180,270}. */
    void rogue_room_template_dims_after_xform(const RogueRoomTemplate* t, int rot_deg,
                                              int reflect_x, int* out_w, int* out_h);

    /* Stamp the template into io_map at (ox,oy) as top-left after applying rotation/reflection.
       Returns number of tiles written. Bounds are clamped safely. */
    int rogue_dungeon_stamp_template(RogueTileMap* io_map, int ox, int oy,
                                     const RogueRoomTemplate* t, int rot_deg, int reflect_x);

    /* Compute exit positions after transform, stored into out_exits (local coords 0..w-1/0..h-1
       after transform). Returns count. */
    int rogue_room_template_compute_exits(const RogueRoomTemplate* t, int rot_deg, int reflect_x,
                                          RogueRoomExit* out_exits, int max);

    /* Compute door positions after transform; returns count. */
    int rogue_room_template_compute_doors(const RogueRoomTemplate* t, int rot_deg, int reflect_x,
                                          RogueRoomDoor* out_doors, int max);

    /* JSON loader (Phase 2.1): parse a single room template definition from JSON text. Returns 1 on
        success, 0 on error and writes a brief message to err if provided. */
    int rogue_room_template_load_json_text(const char* json_text, RogueRoomTemplate* out, char* err,
                                           size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_ROOM_TEMPLATES_H */
