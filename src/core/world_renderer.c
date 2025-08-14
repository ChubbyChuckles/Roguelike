#include "core/world_renderer.h"
#include "core/app_state.h"
#include "core/loot_instances.h"
#include "graphics/tile_sprites.h"
#include "graphics/sprite.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_world_render_tiles(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
#endif
    int scale = 1;
    int tsz = g_app.tile_size;
    int first_tx = (int)(g_app.cam_x / tsz); if(first_tx<0) first_tx=0;
    int first_ty = (int)(g_app.cam_y / tsz); if(first_ty<0) first_ty=0;
    int vis_tx = g_app.viewport_w / tsz + 2;
    int vis_ty = g_app.viewport_h / tsz + 2;
    int last_tx = first_tx + vis_tx; if(last_tx > g_app.world_map.width) last_tx = g_app.world_map.width;
    int last_ty = first_ty + vis_ty; if(last_ty > g_app.world_map.height) last_ty = g_app.world_map.height;
    if(g_app.tileset_loaded){
        for(int y=first_ty; y<last_ty; y++){
            int x = first_tx;
            while(x < last_tx){
                const RogueSprite* spr = NULL;
                if(g_app.tile_sprite_lut_ready){
                    spr = g_app.tile_sprite_lut[y*g_app.world_map.width + x];
                } else {
                    unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                    if(t < ROGUE_TILE_MAX) spr = rogue_tile_sprite_get_xy((RogueTileType)t, x, y);
                }
                if(!(spr && spr->sw)) { x++; continue; }
                int run = 1;
                while(x+run < last_tx){
                    const RogueSprite* spr2 = g_app.tile_sprite_lut_ready ? g_app.tile_sprite_lut[y*g_app.world_map.width + (x+run)] : NULL;
                    if(spr2 != spr) break;
                    run++;
                }
#ifdef ROGUE_HAVE_SDL
                SDL_Rect src = { spr->sx, spr->sy, spr->sw, spr->sh };
                for(int i=0;i<run;i++){
                    SDL_Rect dst = { (int)((x+i)*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), tsz*scale, tsz*scale };
                    SDL_RenderCopy(g_app.renderer, spr->tex->handle, &src, &dst);
                }
#else
                for(int i=0;i<run;i++) rogue_sprite_draw(spr, (int)((x+i)*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), scale);
#endif
                x += run;
            }
        }
    } else {
#ifdef ROGUE_HAVE_SDL
        for(int y=first_ty; y<last_ty; y++){
            for(int x=first_tx; x<last_tx; x++){
                unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                RogueColor c = { (unsigned char)(t*20), (unsigned char)(t*15), (unsigned char)(t*10), 255};
                SDL_SetRenderDrawColor(g_app.renderer, c.r,c.g,c.b,c.a);
                SDL_Rect r = { (int)(x*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), tsz*scale, tsz*scale };
                SDL_RenderFillRect(g_app.renderer, &r); g_app.frame_draw_calls++; g_app.frame_tile_quads++;
            }
        }
#else
        (void)first_ty; (void)last_ty; /* headless no-op */
#endif
    }
}

void rogue_world_render_items(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return; int ts=g_app.tile_size; 
    if(!g_app.item_instances) return;
    for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){
        const RogueItemInstance* it = &g_app.item_instances[i];
        unsigned char r=240,g=210,b=60,a=255; /* default common */
        switch(it->rarity){
            case 1: r=80; g=220; b=80; break; /* uncommon */
            case 2: r=80; g=120; b=255; break; /* rare */
            case 3: r=180; g=70; b=220; break; /* epic */
            case 4: r=255; g=140; b=0; break; /* legendary */
            default: break;
        }
    SDL_SetRenderDrawColor(g_app.renderer, r,g,b,a);
    SDL_Rect rect_ri={ (int)(it->x*ts - g_app.cam_x), (int)(it->y*ts - g_app.cam_y), ts/2, ts/2};
    SDL_RenderFillRect(g_app.renderer,&rect_ri);
    }
#endif
}
