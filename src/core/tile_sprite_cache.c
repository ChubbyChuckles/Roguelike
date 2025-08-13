#include "core/tile_sprite_cache.h"
#include "core/app_state.h"
#include "graphics/tile_sprites.h"
#include "util/log.h"
#include <stdlib.h>

void rogue_tile_sprite_cache_ensure(void){
    if(g_app.tileset_loaded) return;
    rogue_tile_sprites_init(g_app.tile_size);
    if(!rogue_tile_sprites_load_config("assets/tiles.cfg")){
        rogue_tile_sprite_define(ROGUE_TILE_GRASS, "assets/tiles.png", 0, 0);
        rogue_tile_sprite_define(ROGUE_TILE_WATER, "assets/tiles.png", 1, 0);
        rogue_tile_sprite_define(ROGUE_TILE_FOREST, "assets/tiles.png", 2, 0);
        rogue_tile_sprite_define(ROGUE_TILE_MOUNTAIN, "assets/tiles.png", 3, 0);
        rogue_tile_sprite_define(ROGUE_TILE_CAVE_WALL, "assets/tiles.png", 4, 0);
        rogue_tile_sprite_define(ROGUE_TILE_CAVE_FLOOR, "assets/tiles.png", 5, 0);
        rogue_tile_sprite_define(ROGUE_TILE_RIVER, "assets/tiles.png", 6, 0);
    }
    g_app.tileset_loaded = rogue_tile_sprites_finalize();
    if(!g_app.tileset_loaded){
        ROGUE_LOG_WARN("Tile sprites finalize failed; falling back to debug tiles");
        return;
    }
    size_t total = (size_t)g_app.world_map.width * (size_t)g_app.world_map.height;
    g_app.tile_sprite_lut = (const RogueSprite**)malloc(total * sizeof(const RogueSprite*));
    if(!g_app.tile_sprite_lut){
        ROGUE_LOG_WARN("Failed to allocate tile sprite LUT (%zu entries)", total);
        return;
    }
    for(int y=0; y<g_app.world_map.height; y++){
        for(int x=0; x<g_app.world_map.width; x++){
            unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
            const RogueSprite* spr = NULL;
            if(t < ROGUE_TILE_MAX){ spr = rogue_tile_sprite_get_xy((RogueTileType)t, x, y); }
            g_app.tile_sprite_lut[y*g_app.world_map.width + x] = spr;
        }
    }
    g_app.tile_sprite_lut_ready = 1;
    ROGUE_LOG_INFO("Precomputed tile sprite LUT built (%dx%d)", g_app.world_map.width, g_app.world_map.height);
}

void rogue_tile_sprite_cache_free(void){
    if(g_app.tile_sprite_lut){ free((void*)g_app.tile_sprite_lut); g_app.tile_sprite_lut=NULL; }
    g_app.tile_sprite_lut_ready = 0;
    g_app.tileset_loaded = 0;
}
