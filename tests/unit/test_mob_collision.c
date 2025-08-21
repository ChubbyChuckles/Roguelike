#include "core/app_state.h"
#include "core/vegetation/vegetation.h"
#include "core/enemy/enemy_system.h"
#include "core/collision.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Rely on core library's global state & derived stat routines; avoid local redefinitions that caused duplicate symbols. */

static int init_world(void){
    if(!rogue_tilemap_init(&g_app.world_map, 64, 64)) return 0;
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(1234u,0,0);
    if(!rogue_world_generate(&g_app.world_map,&cfg)) return 0;
    return 1;
}

static void make_enemy_type(void){
    g_app.enemy_type_count = 1; g_app.per_type_counts[0]=0;
    RogueEnemyTypeDef* t = &g_app.enemy_types[0];
    memset(t,0,sizeof *t);
    t->speed = 3.0f; t->patrol_radius=4; t->aggro_radius=6; t->group_min=1; t->group_max=1; t->pop_target=0; t->xp_reward=1; t->loot_chance=0.0f;
}

int main(void){
    if(!init_world()){ printf("world_fail\n"); return 1; }
    make_enemy_type();
    g_app.dt = 0.016; /* 16ms */
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg","../assets/trees.cfg");
    rogue_vegetation_generate(0.15f, 777u);
    if(rogue_vegetation_tree_count()==0){ printf("no_trees\n"); return 2; }
    int tree_tx=-1, tree_ty=-1; for(int y=0;y<g_app.world_map.height && tree_tx<0;y++) for(int x=0;x<g_app.world_map.width && tree_tx<0;x++){ if(rogue_vegetation_tile_blocking(x,y)){ tree_tx=x; tree_ty=y; }}
    if(tree_tx<0){ printf("no_tree_found\n"); return 3; }
    RogueEnemy* e = &g_app.enemies[0]; memset(e,0,sizeof *e); e->alive=1; e->type_index=0; e->base.pos.x=(float)tree_tx - 1.0f; e->base.pos.y=(float)tree_ty; e->anchor_x=e->base.pos.x; e->anchor_y=e->base.pos.y; e->patrol_target_x=(float)tree_tx; e->patrol_target_y=(float)tree_ty; g_app.enemy_count=1; g_app.per_type_counts[0]=1; e->ai_state=ROGUE_ENEMY_AI_PATROL; e->max_health=5; e->health=5;
    /* Disable spawner for deterministic movement */
    g_app.enemy_type_count = 0;
    for(int i=0;i<80;i++){ rogue_enemy_system_update(16.0f); }
    int exi=(int)(e->base.pos.x+0.5f); int eyi=(int)(e->base.pos.y+0.5f);
    if(exi==tree_tx && eyi==tree_ty){ printf("enemy_entered_tree\n"); return 4; }
    g_app.player.base.pos.x = e->base.pos.x; g_app.player.base.pos.y = e->base.pos.y; /* Overlap */
    rogue_collision_resolve_enemy_player(e);
    float dx=e->base.pos.x - g_app.player.base.pos.x; float dy=e->base.pos.y - g_app.player.base.pos.y; float d2=dx*dx+dy*dy; if(d2 < 0.30f*0.30f - 0.00005f){ printf("player_overlap\n"); return 5; }
    return 0;
}
