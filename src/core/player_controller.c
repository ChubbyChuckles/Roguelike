#include "core/player_controller.h"
#include "world/tilemap.h"
#include "input/input.h"
#include "core/vegetation.h"

static int pc_tile_block(unsigned char t){
    switch(t){
        case ROGUE_TILE_WATER: case ROGUE_TILE_RIVER: case ROGUE_TILE_RIVER_WIDE: case ROGUE_TILE_RIVER_DELTA:
        case ROGUE_TILE_MOUNTAIN: case ROGUE_TILE_CAVE_WALL: return 1; default: return 0;
    }
}

void rogue_player_controller_update(void){
    float base_speed = (g_app.player_state==2)? g_app.run_speed : (g_app.player_state==1? g_app.walk_speed : 0.0f);
    int ptx=(int)(g_app.player.base.pos.x+0.5f); int pty=(int)(g_app.player.base.pos.y+0.5f);
    float veg_scale = rogue_vegetation_tile_move_scale(ptx,pty);
    float speed = base_speed * veg_scale;
    int moving=0; float orig_x=g_app.player.base.pos.x; float orig_y=g_app.player.base.pos.y; float step = speed * (float)g_app.dt;
    if(rogue_input_is_down(&g_app.input, ROGUE_KEY_UP)) { g_app.player.base.pos.y -= step; g_app.player.facing = 3; moving=1; }
    if(rogue_input_is_down(&g_app.input, ROGUE_KEY_DOWN)) { g_app.player.base.pos.y += step; g_app.player.facing = 0; moving=1; }
    int px_i=(int)(g_app.player.base.pos.x+0.5f); int py_i=(int)(g_app.player.base.pos.y+0.5f);
    if(px_i<0) px_i=0; if(py_i<0) py_i=0; if(px_i>=g_app.world_map.width) px_i=g_app.world_map.width-1; if(py_i>=g_app.world_map.height) py_i=g_app.world_map.height-1;
    if(pc_tile_block(g_app.world_map.tiles[py_i*g_app.world_map.width + px_i]) || rogue_vegetation_tile_blocking(px_i,py_i)) g_app.player.base.pos.y=orig_y;
    if(rogue_input_is_down(&g_app.input, ROGUE_KEY_LEFT)) { g_app.player.base.pos.x -= step; g_app.player.facing = 1; moving=1; }
    if(rogue_input_is_down(&g_app.input, ROGUE_KEY_RIGHT)) { g_app.player.base.pos.x += step; g_app.player.facing = 2; moving=1; }
    px_i=(int)(g_app.player.base.pos.x+0.5f); py_i=(int)(g_app.player.base.pos.y+0.5f);
    if(px_i<0) px_i=0; if(py_i<0) py_i=0; if(px_i>=g_app.world_map.width) px_i=g_app.world_map.width-1; if(py_i>=g_app.world_map.height) py_i=g_app.world_map.height-1;
    if(pc_tile_block(g_app.world_map.tiles[py_i*g_app.world_map.width + px_i]) || rogue_vegetation_tile_blocking(px_i,py_i)) g_app.player.base.pos.x=orig_x;
    if(moving && g_app.player_state==0) g_app.player_state=1; if(!moving) g_app.player_state=0;
    if(g_app.player.base.pos.x<0) g_app.player.base.pos.x=0; if(g_app.player.base.pos.y<0) g_app.player.base.pos.y=0;
    if(g_app.player.base.pos.x>g_app.world_map.width-1) g_app.player.base.pos.x=(float)(g_app.world_map.width-1);
    if(g_app.player.base.pos.y>g_app.world_map.height-1) g_app.player.base.pos.y=(float)(g_app.world_map.height-1);
    g_app.cam_x = g_app.player.base.pos.x * g_app.tile_size - g_app.viewport_w / 2.0f;
    g_app.cam_y = g_app.player.base.pos.y * g_app.tile_size - g_app.viewport_h / 2.0f;
    if(g_app.cam_x<0) g_app.cam_x=0; if(g_app.cam_y<0) g_app.cam_y=0;
    float world_px_w=(float)g_app.world_map.width * g_app.tile_size;
    float world_px_h=(float)g_app.world_map.height * g_app.tile_size;
    if(g_app.cam_x>world_px_w - g_app.viewport_w) g_app.cam_x = world_px_w - g_app.viewport_w;
    if(g_app.cam_y>world_px_h - g_app.viewport_h) g_app.cam_y = world_px_h - g_app.viewport_h;
}
