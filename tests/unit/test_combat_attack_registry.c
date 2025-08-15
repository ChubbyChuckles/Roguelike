#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <stdio.h>
#include <assert.h>
#include "game/combat_attacks.h"

static void test_chain_lengths(){
    int l = rogue_attack_chain_length(ROGUE_WEAPON_LIGHT); assert(l==3);
    int h = rogue_attack_chain_length(ROGUE_WEAPON_HEAVY); assert(h==2);
    int t = rogue_attack_chain_length(ROGUE_WEAPON_THRUST); assert(t==2);
    int r = rogue_attack_chain_length(ROGUE_WEAPON_RANGED); assert(r==2);
    int s = rogue_attack_chain_length(ROGUE_WEAPON_SPELL_FOCUS); assert(s==1);
}

static void test_bounds_and_clamp(){
    const RogueAttackDef* d0 = rogue_attack_get(ROGUE_WEAPON_LIGHT, 0); assert(d0 && d0->chain_index==0);
    const RogueAttackDef* d2 = rogue_attack_get(ROGUE_WEAPON_LIGHT, 2); assert(d2 && d2->chain_index==2);
    const RogueAttackDef* d_over = rogue_attack_get(ROGUE_WEAPON_LIGHT, 99); assert(d_over && d_over->chain_index==2); /* clamp */
    const RogueAttackDef* d_neg = rogue_attack_get(ROGUE_WEAPON_LIGHT, -5); assert(d_neg && d_neg->chain_index==0);
}

static void test_active_window_coverage(){
    /* Ensure each attack's active windows fit inside active_ms. */
    for(int arch=0; arch<ROGUE_WEAPON_ARCHETYPE_COUNT; ++arch){
        int chain_len = rogue_attack_chain_length((RogueWeaponArchetype)arch);
        for(int idx=0; idx<chain_len; ++idx){
            const RogueAttackDef* def = rogue_attack_get((RogueWeaponArchetype)arch, idx);
            assert(def);
            for(int w=0; w<def->num_windows; ++w){
                assert(def->windows[w].start_ms >= 0);
                assert(def->windows[w].end_ms <= def->active_ms + 0.001f);
                assert(def->windows[w].end_ms > def->windows[w].start_ms);
            }
        }
    }
}

int main(){
    test_chain_lengths();
    test_bounds_and_clamp();
    test_active_window_coverage();
    printf("combat_attack_registry: OK\n");
    return 0;
}
