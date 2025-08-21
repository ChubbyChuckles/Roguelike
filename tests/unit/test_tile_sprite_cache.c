#include "core/app_state.h"
#include "core/tile_sprite_cache.h"
#include <stdio.h>
#include <stdlib.h>

RogueAppState g_app;                    /* zero init */
RoguePlayer g_exposed_player_for_stats; /* unused */

static void fake_world(int w, int h)
{
    g_app.world_map.width = w;
    g_app.world_map.height = h;
    g_app.world_map.tiles = (unsigned char*) malloc((size_t) w * h);
    if (!g_app.world_map.tiles)
    {
        printf("alloc fail\n");
        exit(1);
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            g_app.world_map.tiles[y * w + x] = 0;
        }
}

/* Minimal manual replication of LUT build logic (avoids needing renderer/textures) */
static void build_lut_manual(void)
{
    size_t total = (size_t) g_app.world_map.width * (size_t) g_app.world_map.height;
    g_app.tile_sprite_lut = (const void**) malloc(total * sizeof(void*));
    if (!g_app.tile_sprite_lut)
    {
        printf("lut alloc fail\n");
        exit(1);
    }
    for (size_t i = 0; i < total; i++)
    {
        g_app.tile_sprite_lut[i] = (const void*) 0x1;
    }
    g_app.tile_sprite_lut_ready = 1;
}

int main(void)
{
    fake_world(8, 6);
    g_app.tile_size = 16;
    /* Pretend tileset already loaded (skip real loading that needs renderer) */
    g_app.tileset_loaded = 1;
    build_lut_manual();
    const void* first_ptr = g_app.tile_sprite_lut[0];
    /* Calling ensure should early-return without altering existing LUT */
    rogue_tile_sprite_cache_ensure();
    if (g_app.tile_sprite_lut[0] != first_ptr)
    {
        printf("ensure mutated existing lut unexpectedly\n");
        return 1;
    }
    rogue_tile_sprite_cache_free();
    if (g_app.tile_sprite_lut || g_app.tile_sprite_lut_ready || g_app.tileset_loaded)
    {
        printf("cache not freed cleanly\n");
        return 1;
    }
    free(g_app.world_map.tiles);
    g_app.world_map.tiles = NULL;
    return 0;
}
