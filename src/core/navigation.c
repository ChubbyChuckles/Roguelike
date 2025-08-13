#include <math.h>
#include "core/navigation.h"
#include "core/app_state.h"
#include "core/vegetation.h"

static int tile_block(unsigned char t){
    switch(t){
        case ROGUE_TILE_WATER: case ROGUE_TILE_RIVER: case ROGUE_TILE_RIVER_WIDE: case ROGUE_TILE_RIVER_DELTA:
        case ROGUE_TILE_MOUNTAIN: case ROGUE_TILE_CAVE_WALL: return 1; default: return 0;
    }
}

int rogue_nav_is_blocked(int tx,int ty){
    if(tx<0||ty<0||tx>=g_app.world_map.width||ty>=g_app.world_map.height) return 1;
    unsigned char t = g_app.world_map.tiles[ty*g_app.world_map.width + tx];
    if(tile_block(t)) return 1;
    if(rogue_vegetation_tile_blocking(tx,ty)) return 1;
    return 0;
}

float rogue_nav_tile_cost(int tx,int ty){
    if(tx<0||ty<0||tx>=g_app.world_map.width||ty>=g_app.world_map.height) return 9999.0f;
    float base = 1.0f;
    float scale = rogue_vegetation_tile_move_scale(tx,ty); /* 1 or <1 */
    if(scale < 0.999f){ base *= (1.0f/scale); } /* slower move => higher cost */
    return base;
}

void rogue_nav_cardinal_step_towards(float sx,float sy,float tx,float ty,int* out_dx,int* out_dy){
    int best_dx=0,best_dy=0; float best_score=1e9f;
    int dir_candidates[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<4;i++){
        int dx=dir_candidates[i][0]; int dy=dir_candidates[i][1];
        int nx=(int)floorf(sx+dx+0.5f); int ny=(int)floorf(sy+dy+0.5f);
        if(rogue_nav_is_blocked(nx,ny)) continue;
        float manhattan = fabsf((tx - nx)) + fabsf((ty - ny));
        float cost = rogue_nav_tile_cost(nx,ny);
        float score = manhattan + cost*0.15f; /* weight cost lightly */
        if(score < best_score){ best_score=score; best_dx=dx; best_dy=dy; }
    }
    *out_dx=best_dx; *out_dy=best_dy; /* (0,0) if no move */
}
