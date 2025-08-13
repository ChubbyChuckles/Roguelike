#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED
#endif
#include "game/combat.h"
#include <stdio.h>

static int current_frame = 0;
int rogue_get_current_attack_frame(void){ return current_frame; }
RoguePlayer g_exposed_player_for_stats;

int main(void){
    RoguePlayer player; rogue_player_init(&player); player.base.pos.x=0; player.base.pos.y=0;
    RogueEnemy enemy; enemy.alive=1; enemy.base.pos.x=1.0f; enemy.base.pos.y=0; enemy.health=10; enemy.max_health=10;
    RogueEnemy list[1] = { enemy };
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase = ROGUE_ATTACK_STRIKE;
    /* Frames 0 should not hit */
    current_frame=0; int k0 = rogue_combat_player_strike(&combat,&player,list,1); if(list[0].health!=10){ printf("frame0 should not damage\n"); return 1; }
    /* Frame 3 should hit */
    current_frame=3; int k1 = rogue_combat_player_strike(&combat,&player,list,1); if(list[0].health==10){ printf("frame3 should damage\n"); return 1; }
    (void)k0; (void)k1; return 0; }
