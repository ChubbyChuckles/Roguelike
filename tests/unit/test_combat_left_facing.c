#define SDL_MAIN_HANDLED
#include <stdio.h>
#include "game/combat.h"

/* Provide deterministic frame */
int test_local_get_current_attack_frame(void){ return 3; }
#define rogue_get_current_attack_frame test_local_get_current_attack_frame
RoguePlayer g_exposed_player_for_stats; /* stamina regen unused */
void test_local_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void test_local_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
#define rogue_add_damage_number test_local_add_damage_number
#define rogue_add_damage_number_ex test_local_add_damage_number_ex

/* Validate player can damage an enemy positioned to the immediate left when facing left. */
int main(){
    RoguePlayer p; rogue_player_init(&p);
    p.base.pos.x = 10.0f; p.base.pos.y = 5.0f; p.facing = 1; /* left */
    RoguePlayerCombat combat; rogue_combat_init(&combat); combat.phase = ROGUE_ATTACK_STRIKE; /* force active strike */

    /* Place enemy slightly left within expected reach arc.
       We keep small vertical offset (0) to stay centered; horizontal offset chosen within reach.
       Base reach for frame3 ~ 1.6 * 1.35 = 2.16 (+ strength*0.012). Enemy at -0.8 is safe. */
    RogueEnemy e; e.alive=1; e.base.pos.x = p.base.pos.x - 0.8f; e.base.pos.y = p.base.pos.y; e.health=12; e.max_health=12;

    int hp_before = e.health;
    rogue_combat_player_strike(&combat,&p,&e,1);
    if(e.health == hp_before){
        printf("left facing strike failed (hp unchanged)\n");
        return 1;
    }
    printf("left facing strike ok dmg=%d\n", hp_before - e.health);
    return 0;
}
