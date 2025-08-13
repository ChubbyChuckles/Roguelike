#include "game/combat.h"
#include <math.h>

void rogue_combat_init(RoguePlayerCombat* pc){ pc->phase=ROGUE_ATTACK_IDLE; pc->timer=0; pc->combo=0; pc->stamina=100.0f; pc->stamina_regen_delay=0.0f; }

void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed){
    pc->timer += dt_ms;
    if(pc->phase==ROGUE_ATTACK_IDLE){
        if(attack_pressed && pc->stamina >= 15.0f){
            pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->stamina -= 15.0f; pc->stamina_regen_delay = 600.0f; }
    } else if(pc->phase==ROGUE_ATTACK_WINDUP){
        if(pc->timer >= 120.0f){ pc->phase = ROGUE_ATTACK_STRIKE; pc->timer = 0; }
    } else if(pc->phase==ROGUE_ATTACK_STRIKE){
        if(pc->timer >= 80.0f){ pc->phase = ROGUE_ATTACK_RECOVER; pc->timer = 0; pc->combo++; }
    } else if(pc->phase==ROGUE_ATTACK_RECOVER){
        if(pc->timer >= 140.0f){ pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; if(pc->combo>3) pc->combo=0; }
    }
    if(pc->stamina_regen_delay>0){ pc->stamina_regen_delay -= dt_ms; }
    else { pc->stamina += dt_ms * 0.05f; if(pc->stamina>100.0f) pc->stamina=100.0f; }
}

int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count){
    if(pc->phase != ROGUE_ATTACK_STRIKE) return 0;
    int kills=0;
    float px = player->base.pos.x; float py = player->base.pos.y;
    float reach = 1.2f; /* tiles */
    float dirx=0, diry=0;
    switch(player->facing){ case 0: diry=1; break; case 1: dirx=-1; break; case 2: dirx=1; break; case 3: diry=-1; break; }
    float sx = px + dirx*reach*0.5f; float sy = py + diry*reach*0.5f;
    for(int i=0;i<enemy_count;i++){
        if(!enemies[i].alive) continue;
        float dx = enemies[i].base.pos.x - sx; float dy = enemies[i].base.pos.y - sy;
        float dist2 = dx*dx + dy*dy;
        if(dist2 < (reach*reach*0.5f)){ enemies[i].health -= 1; enemies[i].hurt_timer = 150.0f; if(enemies[i].health<=0){ enemies[i].alive=0; kills++; } }
    }
    return kills;
}
