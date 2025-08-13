#include "core/animation_system.h"
#include "core/app_state.h"

void rogue_animation_update(float frame_dt_ms){
    /* Player animation frame progression (moved from app.c) */
    float dt = frame_dt_ms;
    if(dt < 1.0f){
        g_app.anim_dt_accum_ms += dt;
        if(g_app.anim_dt_accum_ms >= 1.0f){
            g_app.player.anim_time += g_app.anim_dt_accum_ms;
            g_app.anim_dt_accum_ms = 0.0f;
        }
    } else {
        g_app.player.anim_time += dt;
    }
    int anim_dir = g_app.player.facing;
    int anim_sheet_dir = (anim_dir==1 || anim_dir==2)? 1 : anim_dir;
    int state_for_anim = (g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER)? 3 : g_app.player_state;
    int frame_count = g_app.player_frame_count[state_for_anim][anim_sheet_dir];
    if(frame_count <=0) frame_count = 1;
    if(state_for_anim == 3){
        const float WINDUP_MS = 120.0f;
        const float STRIKE_MS = 80.0f;
        const float RECOVER_MS = 140.0f;
        float total = WINDUP_MS + STRIKE_MS + RECOVER_MS;
        if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER){
            g_app.attack_anim_time_ms += frame_dt_ms; if(g_app.attack_anim_time_ms > total) g_app.attack_anim_time_ms = total - 0.01f;
            float t = g_app.attack_anim_time_ms / total; if(t<0) t=0; if(t>1) t=1;
            int idx = (int)(t * frame_count); if(idx >= frame_count) idx = frame_count-1;
            g_app.player.anim_frame = idx;
        }
    } else if(state_for_anim == 0){
        g_app.player.anim_frame = 0; g_app.player.anim_time = 0.0f;
    } else {
        int cur = g_app.player.anim_frame;
        int cur_dur = g_app.player_frame_time_ms[state_for_anim][anim_sheet_dir][cur];
        if(cur_dur <=0) cur_dur = 120;
        if(g_app.player.anim_time >= (float)cur_dur){
            g_app.player.anim_time = 0.0f;
            g_app.player.anim_frame = (g_app.player.anim_frame + 1) % frame_count;
        }
    }
    /* Enemy animations: simple frame advance using anim_time */
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(g_app.enemies[i].alive){
        RogueEnemy* e=&g_app.enemies[i]; e->anim_time += frame_dt_ms;
        if(e->anim_time >= 140.0f){ e->anim_time = 0.0f; e->anim_frame = (e->anim_frame + 1) % 8; }
    }
}
