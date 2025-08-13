#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED
#endif
#include "game/combat.h"
#include <stdio.h>

extern int g_attack_frame_override;
RoguePlayer g_exposed_player_for_stats;
void test_local_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void test_local_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
#define rogue_add_damage_number test_local_add_damage_number
#define rogue_add_damage_number_ex test_local_add_damage_number_ex

int main(void){
    RoguePlayer player; rogue_player_init(&player); player.base.pos.x=0; player.base.pos.y=0; player.facing = 2; /* right */
    RogueEnemy enemy; enemy.alive=1; enemy.base.pos.x=0.9f; enemy.base.pos.y=0.0f; enemy.health=10; enemy.max_health=10;
    RogueEnemy list[1] = { enemy };
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase = ROGUE_ATTACK_STRIKE;
    /* Frames 0-1 should not hit (mask=0) */
    g_attack_frame_override=0; rogue_combat_player_strike(&combat,&player,list,1); if(list[0].health!=10){ printf("frame0 should not damage\n"); return 1; }
    g_attack_frame_override=1; rogue_combat_player_strike(&combat,&player,list,1); if(list[0].health!=10){ printf("frame1 should not damage\n"); return 1; }
    /* Frame 2 still mask=1? (mask was 0,0,1...) ensure it can damage */
    g_attack_frame_override=2; rogue_combat_player_strike(&combat,&player,list,1); if(list[0].health==10){ printf("frame2 should damage\n"); return 1; }
    printf("combat gating ok\n");
    return 0; }
