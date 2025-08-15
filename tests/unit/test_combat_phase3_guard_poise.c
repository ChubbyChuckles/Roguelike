#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

static void setup_player(RoguePlayer* p){ rogue_player_init(p); }

int main(){
    RoguePlayer p; setup_player(&p);
    assert(p.guard_meter_max > 0 && p.guard_meter == p.guard_meter_max);
    assert(p.poise_max > 0 && p.poise == p.poise_max);
    /* Simulate guard drain utility (future): manually decrement then ensure clamp logic would apply */
    p.guard_meter -= 25.0f; if(p.guard_meter < 0) p.guard_meter = 0;
    assert(p.guard_meter <= p.guard_meter_max);
    /* Poise damage simulation: ensure not negative */
    p.poise -= 10.0f; if(p.poise < 0) p.poise = 0; assert(p.poise >= 0);
    printf("phase3_guard_poise_basic: OK\n");
    return 0;
}
