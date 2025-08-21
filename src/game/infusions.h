#ifndef ROGUE_GAME_INFUSIONS_H
#define ROGUE_GAME_INFUSIONS_H

/* Phase 7.4: Weapon Infusion system - adjusts damage type split & scaling */

typedef struct RogueInfusionDef
{
    int id;                 /* infusion id */
    const char* name;       /* display */
    float phys_scalar;      /* scales portion of physical left (multiplicative) */
    float fire_add;         /* fraction of total pre-mitigation base converted to fire */
    float frost_add;        /* fraction converted to frost */
    float arcane_add;       /* fraction converted to arcane */
    float bleed_build_add;  /* flat bleed buildup added per hit */
    float poison_build_add; /* flat poison buildup per hit */
    float str_scale_mult;   /* modifies weapon STR scaling */
    float dex_scale_mult;   /* modifies DEX scaling */
    float int_scale_mult;   /* modifies INT scaling */
} RogueInfusionDef;

const RogueInfusionDef* rogue_infusion_get(int id);

#endif
