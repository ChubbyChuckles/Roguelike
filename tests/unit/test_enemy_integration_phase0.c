#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core/enemy_integration.h"
#include "core/enemy_difficulty_scaling.h"
#include "entities/enemy.h"
#include "core/app_state.h"

/* Minimal fake app state population using already loaded enemy types via app_init would be ideal, but here we directly build mapping on existing g_app state after app initialization in runtime tests. For unit test simplicity we fabricate two type defs locally. */

static void fabricate_types(){
    g_app.enemy_type_count = 2;
    memset(&g_app.enemy_types[0],0,sizeof(g_app.enemy_types[0]));
#if defined(_MSC_VER)
    strncpy_s(g_app.enemy_types[0].id,sizeof g_app.enemy_types[0].id,"goblin_grunt",_TRUNCATE);
    strncpy_s(g_app.enemy_types[0].name,sizeof g_app.enemy_types[0].name,"Goblin Grunt",_TRUNCATE);
#else
    strncpy(g_app.enemy_types[0].id,"goblin_grunt",sizeof g_app.enemy_types[0].id -1);
    strncpy(g_app.enemy_types[0].name,"Goblin Grunt",sizeof g_app.enemy_types[0].name -1);
#endif
    g_app.enemy_types[0].group_min=2; g_app.enemy_types[0].group_max=3; g_app.enemy_types[0].tier_id=0; g_app.enemy_types[0].base_level_offset=0; g_app.enemy_types[0].archetype_id=0;

    memset(&g_app.enemy_types[1],0,sizeof(g_app.enemy_types[1]));
#if defined(_MSC_VER)
    strncpy_s(g_app.enemy_types[1].id,sizeof g_app.enemy_types[1].id,"goblin_elite",_TRUNCATE);
    strncpy_s(g_app.enemy_types[1].name,sizeof g_app.enemy_types[1].name,"Goblin Elite",_TRUNCATE);
#else
    strncpy(g_app.enemy_types[1].id,"goblin_elite",sizeof g_app.enemy_types[1].id -1);
    strncpy(g_app.enemy_types[1].name,"Goblin Elite",sizeof g_app.enemy_types[1].name -1);
#endif
    g_app.enemy_types[1].group_min=1; g_app.enemy_types[1].group_max=1; g_app.enemy_types[1].tier_id=2; g_app.enemy_types[1].base_level_offset=1; g_app.enemy_types[1].archetype_id=0;
}

static void test_mappings(){
    fabricate_types();
    RogueEnemyTypeMapping maps[8]; int count=0; int ok = rogue_enemy_integration_build_mappings(maps,8,&count); assert(ok && count==2); assert(rogue_enemy_integration_validate_unique(maps,count));
    int idx0 = rogue_enemy_integration_find_by_type(0,maps,count); int idx1 = rogue_enemy_integration_find_by_type(1,maps,count); assert(idx0>=0 && idx1>=0);
    assert(maps[idx0].tier_id==0 && maps[idx1].tier_id==2);
    assert(maps[idx1].base_level_offset==1);
}

static void test_spawn_apply(){
    RogueEnemy e; memset(&e,0,sizeof e); e.type_index=1; /* elite */
    RogueEnemyTypeMapping maps[8]; int count=0; rogue_enemy_integration_build_mappings(maps,8,&count);
    int idx = rogue_enemy_integration_find_by_type(1,maps,count); assert(idx>=0);
    int player_level = 10;
    rogue_enemy_integration_apply_spawn(&e,&maps[idx],player_level);
    assert(e.level==player_level + maps[idx].base_level_offset);
    assert(e.max_health>=1 && e.health==e.max_health);
    assert(e.final_hp>0 && e.final_damage>0);
}

int main(void){ test_mappings(); test_spawn_apply(); printf("OK test_enemy_integration_phase0\n"); return 0; }
