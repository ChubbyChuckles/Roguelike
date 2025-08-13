#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include "game/combat.h"

static int g_frame=3; int test_local_get_current_attack_frame(void){ return g_frame; }
#define rogue_get_current_attack_frame test_local_get_current_attack_frame
RoguePlayer g_exposed_player_for_stats;
void test_local_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void test_local_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
#define rogue_add_damage_number test_local_add_damage_number
#define rogue_add_damage_number_ex test_local_add_damage_number_ex

int main(){
    srand(1);
    RoguePlayer p; rogue_player_init(&p); p.facing=2; /* right */
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase=ROGUE_ATTACK_STRIKE;
    /* Enemy clearly inside forward cone */
    /* Reconstruct reach used in frame 3 (afr=3): base_reach = 1.6 * reach_curve[3] (1.35f) = 2.16; reach += strength*0.012 (strength default ~5) */
    float reach_curve = 1.35f; float base_reach = 1.6f * reach_curve; float reach = base_reach + (p.strength * 0.012f);
    float dirx=1.0f, diry=0.0f; float cx = p.base.pos.x + dirx * reach * 0.45f; float cy = p.base.pos.y;
    float lateral_limit = reach * 0.95f;
    /* Inside enemy: within reach radius and within lateral limit */
    RogueEnemy inside; inside.alive=1; inside.base.pos.x = cx + 0.4f; inside.base.pos.y = cy + 0.2f; inside.health=10; inside.max_health=10;
    /* Outside enemy: same forward distance but just beyond lateral limit */
    RogueEnemy outside; outside.alive=1; outside.base.pos.x = cx + 0.4f; outside.base.pos.y = cy + (lateral_limit + 0.15f); outside.health=10; outside.max_health=10;
    RogueEnemy arr[2] = {inside,outside};
    int k = rogue_combat_player_strike(&combat,&p,arr,2); (void)k;
    if(arr[0].health==10){ printf("inside enemy not hit (inside=%d outside=%d)\n", arr[0].health, arr[1].health); return 1; }
    if(arr[1].health<10){ printf("outside enemy should not be hit (inside=%d outside=%d lateral_limit=%.2f reach=%.2f)\n", arr[0].health, arr[1].health, lateral_limit, reach); return 1; }
    printf("attack cone regression ok\n");
    return 0;
}
