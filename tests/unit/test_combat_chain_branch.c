#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

/* Minimal stubs for external symbols referenced by combat.c */
RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

static void simulate_ms(RoguePlayerCombat* c, float ms){
    /* Step in 10ms increments to progress phases deterministically. */
    float acc=0; while(acc < ms){ float step = 10.0f; if(acc+step>ms) step = ms-acc; rogue_combat_update_player(c, step, 0); acc+=step; }
}

int main(){
    RoguePlayerCombat combat; rogue_combat_init(&combat);
    assert(rogue_combat_current_archetype(&combat)==ROGUE_WEAPON_LIGHT);
    /* Start first attack */
    rogue_combat_update_player(&combat, 0, 1); /* press */
    simulate_ms(&combat, 1000.0f); /* allow full cycle to finish (chained) */
    /* Buffer next attack late in recovery */
    rogue_combat_update_player(&combat, 0, 1);
    simulate_ms(&combat, 1000.0f);
    int idx_after_two = rogue_combat_current_chain_index(&combat);
    assert(idx_after_two==2 || idx_after_two==1); /* should progress along chain (wrap possible) */
    /* Queue branch to heavy archetype */
    rogue_combat_queue_branch(&combat, ROGUE_WEAPON_HEAVY);
    rogue_combat_update_player(&combat, 0, 1); /* press again */
    simulate_ms(&combat, 600.0f);
    assert(rogue_combat_current_archetype(&combat)==ROGUE_WEAPON_HEAVY);
    assert(rogue_combat_current_chain_index(&combat)==0); /* reset on branch */
    printf("combat_chain_branch: OK\n");
    return 0;
}
