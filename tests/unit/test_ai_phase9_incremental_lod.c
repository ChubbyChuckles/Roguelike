#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include "ai/core/ai_scheduler.h"
#include "ai/core/ai_profiler.h"
#include "entities/enemy.h"
#include "core/app_state.h"

static void init_enemy(RogueEnemy* e, float x, float y){
    memset(e,0,sizeof *e);
    e->alive = 1;
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->ai_bt_enabled = 0; /* ensure enable actually builds tree */
    rogue_enemy_ai_bt_enable(e);
}

int main(void){
    rogue_ai_scheduler_reset_for_tests();
    rogue_ai_scheduler_set_buckets(1); /* single bucket ensures every BT enemy ticks each frame */
    rogue_ai_lod_set_radius(10.0f); /* enemies farther than sqrt(100)=10 tiles get maintenance only */
    /* Setup player position */
    g_app.player.base.pos.x = 0.0f; g_app.player.base.pos.y = 0.0f;
    RogueEnemy enemies[8]; for(int i=0;i<8;i++){ init_enemy(&enemies[i], (float)i*2.0f, 0.0f); }
    /* Place one far enemy beyond LOD radius */
    init_enemy(&enemies[7], 50.0f, 0.0f);
    /* Record initial x of a near enemy (index 1 so it's offset from player at 0) */
    float start_x = enemies[1].base.pos.x;
    /* Simulate frames equal to buckets so each near enemy should have been processed once */
    /* Track movement occurrence by manually invoking scheduler many frames */
    for(int f=0; f<16; f++){ rogue_ai_scheduler_tick(enemies,8,0.05f); }
    if(enemies[1].base.pos.x >= start_x - 0.0001f){ printf("AI_INC_FAIL no_progress bucketed enemy start=%.2f cur=%.2f\n", start_x, enemies[1].base.pos.x); return 1; }
    /* Far enemy should not have moved significantly (MoveToPlayer drives toward 0). Allow tiny epsilon. */
    if(enemies[7].base.pos.x < 49.0f){ printf("AI_INC_FAIL lod_far_moved dist=%f\n", enemies[7].base.pos.x); return 1; }
    /* Now shrink LOD radius to include far enemy and tick multiple frames; expect movement */
    rogue_ai_lod_set_radius(100.0f);
    float far_start = enemies[7].base.pos.x;
    for(int f=0; f<5; f++){ rogue_ai_scheduler_tick(enemies,8,0.1f); }
    if(enemies[7].base.pos.x >= far_start){ printf("AI_INC_FAIL lod_inclusion_no_move\n"); return 1; }
    printf("AI_INC_OK buckets=%d frame=%u moved_near=%.2f far_delta=%.2f\n", rogue_ai_scheduler_get_buckets(), rogue_ai_scheduler_frame(), start_x - enemies[1].base.pos.x, far_start - enemies[7].base.pos.x);
    return 0;
}
