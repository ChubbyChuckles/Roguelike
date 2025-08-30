/* Simple map editing APIs for the debug overlay and headless unit tests */
#ifndef ROGUE_CORE_WORLD_MAP_DEBUG_H
#define ROGUE_CORE_WORLD_MAP_DEBUG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Set a single tile, clamped to bounds. Returns 0 on success, -1 on OOB. */
    int rogue_map_debug_set_tile(int x, int y, unsigned char tile);

    /* Paint a filled square brush centered at (cx,cy) with radius r (size = 2r+1). */
    int rogue_map_debug_brush_square(int cx, int cy, int radius, unsigned char tile);

    /* Paint a filled rectangle inclusive [x0..x1],[y0..y1] (auto-clamped). */
    int rogue_map_debug_brush_rect(int x0, int y0, int x1, int y1, unsigned char tile);

    /* Save the current world map to JSON (RLE encoded tiles). Returns 0 on success. */
    int rogue_map_debug_save_json(const char* path);

    /* Load a world map from JSON produced by save_json. Returns 0 on success. */
    int rogue_map_debug_load_json(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_WORLD_MAP_DEBUG_H */
