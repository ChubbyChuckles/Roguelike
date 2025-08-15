#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}

int main(void){
    RoguePlayerCombat c; rogue_combat_init(&c);
    /* Start attack */
    rogue_combat_update_player(&c,0,1);
    /* Advance with very small dt slices to accumulate startup */
    for(int i=0;i<20000 && c.phase!=ROGUE_ATTACK_STRIKE;i++){
        rogue_combat_update_player(&c,0.01f,0);
    }
    assert(c.phase==ROGUE_ATTACK_STRIKE && "Expected transition using drift-resistant accumulator");
    printf("combat_drift_timing: OK\n");
    return 0;
}
