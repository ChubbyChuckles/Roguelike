#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include "game/combat.h"

static int g_frame=3; int rogue_get_current_attack_frame(void){ return g_frame; }
RoguePlayer g_exposed_player_for_stats;
void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}

int main(){
    srand(1);
    RoguePlayer p; rogue_player_init(&p); p.facing=2; /* right */
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase=ROGUE_ATTACK_STRIKE;
    /* Enemy clearly inside forward cone */
    RogueEnemy inside; inside.alive=1; inside.base.pos.x=1.2f; inside.base.pos.y=0.05f; inside.health=10; inside.max_health=10;
    /* Place outside with large perpendicular offset beyond reach*0.95 (~2.0) while keeping within dist2 by reducing forward x */
    RogueEnemy outside; outside.alive=1; outside.base.pos.x=0.4f; outside.base.pos.y=3.0f; outside.health=10; outside.max_health=10;
    RogueEnemy arr[2] = {inside,outside};
    int k = rogue_combat_player_strike(&combat,&p,arr,2); (void)k;
    if(arr[0].health==10){ printf("inside enemy not hit (health=%d) outside=%d\n", arr[0].health, arr[1].health); return 1; }
    if(arr[1].health<10){ printf("outside enemy should not be hit (inside=%d outside=%d)\n", arr[0].health, arr[1].health); return 1; }
    printf("attack cone regression ok\n");
    return 0;
}
