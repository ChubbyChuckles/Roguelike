#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED
#endif
#include "game/combat.h"
#include <stdio.h>
#include <math.h>

/* Minimal stubs to satisfy externs if needed */
extern int g_attack_frame_override;
/* Set override to frame 3 */
RoguePlayer g_exposed_player_for_stats; /* not used here */
void test_local_add_damage_number(float x,float y,int amount,int from_player) {(void)x;(void)y;(void)amount;(void)from_player;}
void test_local_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
#define rogue_add_damage_number test_local_add_damage_number
#define rogue_add_damage_number_ex test_local_add_damage_number_ex

int main(void){
    RoguePlayer player; rogue_player_init(&player); player.strength = 10; player.base.pos.x=0; player.base.pos.y=0; player.facing = 2; /* right */
    RogueEnemy enemies[4];
    /* Place enemies left and right to ensure bidirectional side detection */
    for(int i=0;i<4;i++){ enemies[i].alive=1; enemies[i].base.pos.y=0; enemies[i].health=10; enemies[i].max_health=10; }
    /* Place all enemies in a semicircle around slight forward arc so new perpendicular limit does not exclude them when facing right */
    enemies[0].base.pos.x = 0.6f; enemies[0].base.pos.y = -0.2f;
    enemies[1].base.pos.x = 0.8f; enemies[1].base.pos.y = 0.0f;
    enemies[2].base.pos.x = 0.6f; enemies[2].base.pos.y = 0.2f;
    enemies[3].base.pos.x = 0.9f; enemies[3].base.pos.y = 0.0f;
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase = ROGUE_ATTACK_STRIKE;
    g_attack_frame_override = 3;
    int kills = rogue_combat_player_strike(&combat,&player,enemies,4);
    int damaged=0; for(int i=0;i<4;i++) if(enemies[i].health<10) damaged++;
    if(damaged==0){ printf("no enemies damaged on hit frame\n"); return 1; }
    (void)kills; return 0; }
