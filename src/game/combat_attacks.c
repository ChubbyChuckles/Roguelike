#include "game/combat_attacks.h"
#include <stddef.h>

/* Static in-memory attack definition table.
   For now we author a tiny chain per archetype (light: 3, heavy:2, thrust:2, ranged:2, spell:1).
   Active windows are simplistic single continuous windows; future versions may add multi-hit splits. */

typedef struct RogueAttackChain { const RogueAttackDef* defs; int count; } RogueAttackChain;

static const RogueAttackDef g_light_chain[] = {
    {0,"light_1",ROGUE_WEAPON_LIGHT,0,110,70,120,14,10,5,0,0.25f,0.10f,0.00f,1,{{0,70}}, 0.0f, 0x0002, 0.50f, 1.0f, 0.0f},
    {1,"light_2",ROGUE_WEAPON_LIGHT,1,95, 65,115,12,12,6,0,0.27f,0.12f,0.00f,1,{{0,65}}, 0.0f, 0x0002, 0.45f, 1.2f, 0.0f},
    {2,"light_3",ROGUE_WEAPON_LIGHT,2,105,75,140,16,15,8,0,0.30f,0.14f,0.00f,1,{{0,75}}, 0.0f, 0x0002, 0.40f, 1.5f, 0.0f},
};
static const RogueAttackDef g_heavy_chain[] = {
    {0,"heavy_1",ROGUE_WEAPON_HEAVY,0,170,90,180,24,28,14,0,0.45f,0.05f,0.00f,1,{{0,90}}, 0.0f, 0x0000, 0.65f, 3.5f, 0.0f},
    {1,"heavy_2",ROGUE_WEAPON_HEAVY,1,190,105,200,28,35,18,0,0.50f,0.05f,0.00f,1,{{0,105}}, 0.0f, 0x0000, 0.65f, 4.0f, 0.0f},
};
static const RogueAttackDef g_thrust_chain[] = {
    {0,"thrust_1",ROGUE_WEAPON_THRUST,0,120,55,110,13,12,7,0,0.20f,0.20f,0.00f,1,{{0,55}}, 0.0f, 0x0002, 0.45f, 0.8f, 0.0f},
    {1,"thrust_2",ROGUE_WEAPON_THRUST,1,125,60,115,14,14,8,0,0.22f,0.22f,0.00f,1,{{0,60}}, 0.0f, 0x0002, 0.45f, 0.9f, 0.0f},
};
static const RogueAttackDef g_ranged_chain[] = {
    {0,"ranged_1",ROGUE_WEAPON_RANGED,0,140,40,100,10,0,4,0,0.05f,0.30f,0.00f,1,{{0,40}}, 0.0f, 0x0002, 0.35f, 0.0f, 0.6f},
    {1,"ranged_2",ROGUE_WEAPON_RANGED,1,150,50,110,12,0,5,0,0.05f,0.34f,0.00f,1,{{0,50}}, 0.0f, 0x0002, 0.35f, 0.0f, 0.7f},
};
static const RogueAttackDef g_spell_chain[] = {
    {0,"spell_1",ROGUE_WEAPON_SPELL_FOCUS,0,160,60,140,16,0,9,3,0.00f,0.00f,0.40f,1,{{0,60}}, 0.0f, 0x0002, 0.40f, 0.0f, 1.8f},
};

static const RogueAttackChain g_chains[ROGUE_WEAPON_ARCHETYPE_COUNT] = {
    {g_light_chain,  (int)(sizeof g_light_chain / sizeof g_light_chain[0])},
    {g_heavy_chain,  (int)(sizeof g_heavy_chain / sizeof g_heavy_chain[0])},
    {g_thrust_chain, (int)(sizeof g_thrust_chain/ sizeof g_thrust_chain[0])},
    {g_ranged_chain, (int)(sizeof g_ranged_chain/ sizeof g_ranged_chain[0])},
    {g_spell_chain,  (int)(sizeof g_spell_chain / sizeof g_spell_chain[0])},
};

const RogueAttackDef* rogue_attack_get(RogueWeaponArchetype arch, int chain_index){
    if(arch < 0 || arch >= ROGUE_WEAPON_ARCHETYPE_COUNT) return NULL;
    const RogueAttackChain* ch = &g_chains[arch];
    if(ch->count==0) return NULL;
    if(chain_index < 0) chain_index = 0;
    if(chain_index >= ch->count) chain_index = ch->count - 1; /* clamp */
    return &ch->defs[chain_index];
}

int rogue_attack_chain_length(RogueWeaponArchetype arch){
    if(arch < 0 || arch >= ROGUE_WEAPON_ARCHETYPE_COUNT) return 0;
    return g_chains[arch].count;
}
