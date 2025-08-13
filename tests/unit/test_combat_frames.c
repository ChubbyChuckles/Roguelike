#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED
#endif
#include "game/combat.h"
#include <stdio.h>

/* Minimal stubs to satisfy externs if needed */
int rogue_get_current_attack_frame(void){ return 3; }
RoguePlayer g_exposed_player_for_stats; /* not used here */

int main(void){
    RoguePlayer player; rogue_player_init(&player); player.strength = 10; player.base.pos.x=0; player.base.pos.y=0;
    RogueEnemy enemies[4]; for(int i=0;i<4;i++){ enemies[i].alive=1; enemies[i].base.pos.x = 0.8f + i*0.2f; enemies[i].base.pos.y=0; enemies[i].health=10; enemies[i].max_health=10; }
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase = ROGUE_ATTACK_STRIKE;
    int kills = rogue_combat_player_strike(&combat,&player,enemies,4);
    int damaged=0; for(int i=0;i<4;i++) if(enemies[i].health<10) damaged++;
    if(damaged==0){ printf("no enemies damaged on hit frame\n"); return 1; }
    (void)kills; return 0; }
