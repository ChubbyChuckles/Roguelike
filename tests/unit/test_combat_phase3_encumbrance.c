#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

static void tick_regen(RoguePlayerCombat* pc, float ms){
    /* simulate stamina drain then regen window */
    pc->stamina = 50.0f; pc->stamina_regen_delay = 0.0f; /* immediate regen */
    rogue_combat_update_player(pc, ms, 0);
}

int main(){
    RoguePlayer p; rogue_player_init(&p); g_exposed_player_for_stats = p; /* copy for global reference */
    RoguePlayerCombat c; rogue_combat_init(&c);
    /* Light tier (0) baseline regen sample over 1000ms */
    tick_regen(&c, 1000.0f); float regen_light = c.stamina - 50.0f; assert(regen_light > 0.0f);
    /* Force encumbrance tiers and compare */
    g_exposed_player_for_stats.encumbrance = g_exposed_player_for_stats.encumbrance_capacity * 0.5f; rogue_player_recalc_derived(&g_exposed_player_for_stats); /* medium */
    c.stamina = 50.0f; c.stamina_regen_delay=0.0f; tick_regen(&c, 1000.0f); float regen_medium = c.stamina - 50.0f; assert(regen_medium < regen_light);
    g_exposed_player_for_stats.encumbrance = g_exposed_player_for_stats.encumbrance_capacity * 0.85f; rogue_player_recalc_derived(&g_exposed_player_for_stats); /* heavy */
    c.stamina = 50.0f; c.stamina_regen_delay=0.0f; tick_regen(&c, 1000.0f); float regen_heavy = c.stamina - 50.0f; assert(regen_heavy < regen_medium);
    g_exposed_player_for_stats.encumbrance = g_exposed_player_for_stats.encumbrance_capacity * 1.05f; rogue_player_recalc_derived(&g_exposed_player_for_stats); /* overloaded */
    c.stamina = 50.0f; c.stamina_regen_delay=0.0f; tick_regen(&c, 1000.0f); float regen_over = c.stamina - 50.0f; assert(regen_over < regen_heavy);
    printf("encumbrance_regen: light=%.3f medium=%.3f heavy=%.3f over=%.3f\n", regen_light, regen_medium, regen_heavy, regen_over);
    /* Movement speed scaling sanity via controller: we only assert tier changes exist (detailed movement tests elsewhere) */
    assert(g_exposed_player_for_stats.encumbrance_tier == 3);
    printf("phase3_encumbrance_basic: OK\n");
    return 0;
}
