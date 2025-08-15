// combat_attacks.h - Data-driven (static table for now) attack definitions
#ifndef ROGUE_GAME_COMBAT_ATTACKS_H
#define ROGUE_GAME_COMBAT_ATTACKS_H

#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueWeaponArchetype {
    ROGUE_WEAPON_LIGHT = 0,
    ROGUE_WEAPON_HEAVY,
    ROGUE_WEAPON_THRUST,
    ROGUE_WEAPON_RANGED,
    ROGUE_WEAPON_SPELL_FOCUS,
    ROGUE_WEAPON_ARCHETYPE_COUNT
} RogueWeaponArchetype;

#define ROGUE_MAX_ATTACK_WINDOWS 4

typedef struct RogueAttackWindow { float start_ms; float end_ms; } RogueAttackWindow;

typedef struct RogueAttackDef {
    int id;                         /* stable id within table */
    const char* name;               /* debug */
    RogueWeaponArchetype archetype; /* owning archetype */
    int chain_index;                /* position inside combo/branch chain */
    float startup_ms;               /* windup */
    float active_ms;                /* strike phase nominal length */
    float recovery_ms;              /* recovery */
    float stamina_cost;             /* stamina cost when initiating */
    float poise_damage;             /* future: applied to enemy poise */
    float base_damage;              /* additive base before stat scaling */
    unsigned char damage_type;      /* 0=physical,1=fire,2=frost,3=arcane (temp enum) */
    float str_scale;                /* strength scaling coefficient */
    float dex_scale;                /* dex scaling coefficient */
    float int_scale;                /* intelligence scaling coefficient */
    int   num_windows;              /* active hit windows (strike sub-intervals) */
    RogueAttackWindow windows[ROGUE_MAX_ATTACK_WINDOWS];
} RogueAttackDef;

/* Returns pointer to attack def for archetype + chain index (wrap/clamp). */
const RogueAttackDef* rogue_attack_get(RogueWeaponArchetype arch, int chain_index);
/* Returns length of chain (number of defs) for archetype. */
int rogue_attack_chain_length(RogueWeaponArchetype arch);

#ifdef __cplusplus
}
#endif

#endif
