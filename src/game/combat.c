#include "game/combat.h"
#include <math.h>
#include <stdlib.h>

/* External helpers */
void rogue_app_add_hitstop(float ms);

/* Provided by app.c */
void rogue_add_damage_number(float x,float y,int amount,int from_player);
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit);

void rogue_combat_init(RoguePlayerCombat* pc){
    pc->phase=ROGUE_ATTACK_IDLE; pc->timer=0; pc->combo=0; pc->stamina=100.0f; pc->stamina_regen_delay=0.0f;
    pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0.0f;
}

int rogue_force_attack_active = 0; /* exported */

void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed){
    /* Input buffering: remember press during windup/strike/recover */
    if(attack_pressed){
        if(pc->phase==ROGUE_ATTACK_IDLE){
            pc->buffered_attack=1; /* will consume below */
        } else {
            pc->buffered_attack=1; /* store for chain */
        }
    }

    /* Dynamic timing constants (ms) with slight combo speed-up */
    float combo_speed = 1.0f - (pc->combo * 0.07f); if(combo_speed < 0.75f) combo_speed = 0.75f;
    float WINDUP_MS  = 110.0f * combo_speed; /* faster with combo */
    float STRIKE_MS  = 70.0f * combo_speed;
    float RECOVER_MS = 120.0f * combo_speed;

    pc->timer += dt_ms;
    if(pc->phase==ROGUE_ATTACK_IDLE){
        if(pc->buffered_attack && pc->stamina >= 14.0f){
            pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->stamina -= 14.0f; pc->stamina_regen_delay = 500.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; }
    } else if(pc->phase==ROGUE_ATTACK_WINDUP){
        if(pc->timer >= WINDUP_MS){ pc->phase = ROGUE_ATTACK_STRIKE; pc->timer = 0; pc->strike_time_ms=0; }
    } else if(pc->phase==ROGUE_ATTACK_STRIKE){
        pc->strike_time_ms += dt_ms;
        int early_cancel_ready = (pc->hit_confirmed && pc->strike_time_ms >= STRIKE_MS*0.45f);
        if(pc->timer >= STRIKE_MS || (early_cancel_ready && pc->buffered_attack)){
            pc->phase = ROGUE_ATTACK_RECOVER; pc->timer = 0; pc->combo++; if(pc->combo>5) pc->combo=5; }
    } else if(pc->phase==ROGUE_ATTACK_RECOVER){
        /* Allow late-chain: buffering during recover triggers next windup slightly early */
        if(pc->timer >= RECOVER_MS){
            if(pc->buffered_attack && pc->stamina >= 10.0f){
                pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->stamina -= 10.0f; pc->stamina_regen_delay=450.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; }
            else { pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; pc->combo = (pc->combo>0)?pc->combo:0; pc->hit_confirmed=0; pc->buffered_attack=0; }
        }
    }

    if(pc->stamina_regen_delay>0){ pc->stamina_regen_delay -= dt_ms; }
    else {
        extern RoguePlayer g_exposed_player_for_stats; /* declared in app for access */
        float dex = (float)g_exposed_player_for_stats.dexterity;
        float intel = (float)g_exposed_player_for_stats.intelligence;
        float regen = 0.055f + (dex*0.00085f) + (intel*0.00055f); /* slight buff */
        pc->stamina += dt_ms * regen; if(pc->stamina>100.0f) pc->stamina=100.0f; }
}

int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count){
    if(pc->phase != ROGUE_ATTACK_STRIKE) return 0;
    int kills=0;
    /* Attack frame gating/ reach curve */
    static const float reach_curve[8] = {0.65f,0.95f,1.25f,1.35f,1.35f,1.18f,0.95f,0.75f}; /* subtle reach buff */
    static const unsigned char hit_mask[8] = {0,0,1,1,1,1,0,0};
    extern int rogue_get_current_attack_frame(void); /* provided by app */
    int afr = rogue_get_current_attack_frame(); if(afr<0 || afr>7) afr=0;
    if(!rogue_force_attack_active){
        if(!hit_mask[afr]) return 0; /* only damage on active frames */
    }
    extern RoguePlayer g_exposed_player_for_stats; /* for consistency if needed */
    float px = player->base.pos.x; float py = player->base.pos.y;
    float base_reach = 1.6f * reach_curve[afr];
    float reach = base_reach + (player->strength * 0.012f);
    float dirx=0, diry=0; int facing = player->facing;
    switch(facing){ case 0: diry=1; break; case 1: dirx=-1; break; case 2: dirx=1; break; case 3: diry=-1; break; }
    /* Center for arc slightly forward (unbiased horizontally/vertically) */
    float cx = px + dirx * reach * 0.45f;
    float cy = py + diry * reach * 0.45f;
    float reach2 = reach*reach;
    for(int i=0;i<enemy_count;i++){
        if(!enemies[i].alive) continue;
        float ex = enemies[i].base.pos.x; float ey = enemies[i].base.pos.y;
        float dx = ex - cx; float dy = ey - cy; float dist2 = dx*dx + dy*dy; if(dist2 > reach2) continue;
        /* Directional filter: allow a bit of behind tolerance (10%) to feel responsive */
        float dot = dx*dirx + dy*diry; if(dot < -0.15f) continue;
        /* Slight lateral clamp: reject if perpendicular component too large (narrow forward cone) */
    float perp = dx* (-diry) + dy* dirx; /* signed perpendicular */
    float lateral_limit = reach * 0.95f; if(fabsf(perp) > lateral_limit) continue;
        /* Simple damage */
    int base = 1 + player->strength/5;
    /* Combo damage scaling (up to +40%). Integer truncation on small bases caused plateaus; enforce monotonic gain. */
    float combo_scale = 1.0f + (pc->combo * 0.08f); if(combo_scale>1.4f) combo_scale=1.4f;
    /* Crit: base 5% + DEX*0.35%, cap 60%, multiplier 1.9x */
    float crit_chance = 0.05f + player->dexterity * 0.0035f;
    if(crit_chance>0.60f) crit_chance=0.60f;
    int crit = (((float)rand()/(float)RAND_MAX) < crit_chance) ? 1 : 0;
    /* Round to nearest instead of floor to smooth early scaling */
    float raw = base * combo_scale;
    int dmg = crit ? (int)floorf(base * 1.9f * combo_scale + 0.5f) : (int)floorf(raw + 0.5f);
    if(!crit && pc->combo>0){
        /* Guarantee at least +1 per combo step up to scaled cap (prevents early plateaus for low base values) */
        int min_noncrit = base + pc->combo;
        int hard_cap = (int)floorf(base * 1.4f + 0.5f); /* do not exceed nominal 40% cap rounding */
        if(min_noncrit > hard_cap) min_noncrit = hard_cap;
        if(dmg < min_noncrit) dmg = min_noncrit;
    } else if(crit && pc->combo>0){
        /* For crits, ensure damage not below non-crit minimum for that combo */
        int noncrit_floor = base + pc->combo;
        int hard_cap = (int)floorf(base * 1.4f * 1.9f + 0.5f);
        if(noncrit_floor > hard_cap) noncrit_floor = hard_cap;
        if(dmg < noncrit_floor) dmg = noncrit_floor;
    }
    enemies[i].health -= dmg; enemies[i].hurt_timer = 150.0f; enemies[i].flash_timer = 70.0f; pc->hit_confirmed=1;
    /* Hitstop injection (access app global) */
    float hs = crit ? 90.0f : 55.0f; hs += pc->combo * 5.0f; if(hs>140.0f) hs=140.0f; rogue_app_add_hitstop(hs);
    /* Simple knockback */
    enemies[i].base.pos.x += dirx * 0.07f;
    enemies[i].base.pos.y += diry * 0.07f;
    /* Micro hitstop: signal via global dt dampening (handled externally not implemented; placeholder) */
    rogue_add_damage_number_ex(ex, ey - 0.4f, dmg, 1, crit);
        if(enemies[i].health<=0){ enemies[i].alive=0; kills++; }
    }
    return kills;
}
