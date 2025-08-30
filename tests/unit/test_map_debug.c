#include "../../src/core/app/app_state.h"
#include "../../src/core/world/map_debug.h"
#include "../../src/world/tilemap.h"
#include <stdio.h>
#include <string.h>

extern struct RogueAppState g_app;

static int init_small_map(int w, int h)
{
    if (g_app.world_map.tiles)
    {
        rogue_tilemap_free(&g_app.world_map);
    }
    if (!rogue_tilemap_init(&g_app.world_map, w, h))
        return 0;
    memset(g_app.world_map.tiles, 0, (size_t) w * (size_t) h);
    return 1;
}

int main(void)
{
    g_app.tile_size = 16;
    if (!init_small_map(16, 12))
    {
        printf("init_small_map failed\n");
        return 1;
    }
    /* Single set */
    if (rogue_map_debug_set_tile(2, 3, 7) != 0)
        return 2;
    if (g_app.world_map.tiles[3 * g_app.world_map.width + 2] != 7)
        return 3;
    /* Square brush */
    if (rogue_map_debug_brush_square(8, 6, 1, 5) != 0)
        return 4;
    int painted = 0;
    for (int y = 5; y <= 7; ++y)
        for (int x = 7; x <= 9; ++x)
            if (g_app.world_map.tiles[y * g_app.world_map.width + x] == 5)
                painted++;
    if (painted < 9)
        return 5;
    /* Rect brush */
    if (rogue_map_debug_brush_rect(0, 0, 3, 0, 2) != 0)
        return 6;
    for (int x = 0; x <= 3; ++x)
        if (g_app.world_map.tiles[x] != 2)
            return 7;
    /* Save and load roundtrip */
    const char* path = "test_map_debug_roundtrip.json";
    if (rogue_map_debug_save_json(path) != 0)
        return 8;
    /* Zero map, then load */
    memset(g_app.world_map.tiles, 0,
           (size_t) g_app.world_map.width * (size_t) g_app.world_map.height);
    if (rogue_map_debug_load_json(path) != 0)
        return 9;
    /* Verify a few sentinels persisted */
    if (g_app.world_map.tiles[3 * g_app.world_map.width + 2] != 7)
        return 10;
    if (g_app.world_map.tiles[6 * g_app.world_map.width + 8] != 5)
        return 11;
    if (g_app.world_map.tiles[0] != 2 || g_app.world_map.tiles[3] != 2)
        return 12;
    printf("test_map_debug ok\n");
    return 0;
}
