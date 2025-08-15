#ifndef ROGUE_GAME_WEAPONS_H
#define ROGUE_GAME_WEAPONS_H

/* Phase 7.1 Data-driven weapon definitions (initial in-memory table) */

typedef struct RogueWeaponDef {
    int id;                 /* unique id */
    const char* name;       /* display name */
    int base_damage;        /* flat base added before scaling */
    float str_scale;        /* strength coefficient */
    float dex_scale;        /* dexterity coefficient */
    float int_scale;        /* intelligence coefficient */
    float stamina_cost_mult;/* multiplier applied to attack stamina cost */
    float poise_damage_mult;/* multiplier applied to attack poise damage */
    float durability_max;   /* starting / max durability */
} RogueWeaponDef;

/* Weapon familiarity tracking (7.5): increases small bonus with usage up to a soft cap */
typedef struct RogueWeaponFamiliarity {
    int weapon_id;
    float usage_points; /* accumulates with successful hits */
} RogueWeaponFamiliarity;

const RogueWeaponDef* rogue_weapon_get(int id);
float rogue_weapon_get_familiarity_bonus(int weapon_id);
void rogue_weapon_register_hit(int weapon_id, float damage_done);
void rogue_weapon_tick_durability(int weapon_id, float amount);
float rogue_weapon_current_durability(int weapon_id);

/* Stance modifiers (7.3): returned as scalar adjustments */
typedef struct RogueStanceModifiers { float damage_mult; float stamina_mult; float poise_damage_mult; } RogueStanceModifiers;
RogueStanceModifiers rogue_stance_get_mods(int stance);

#endif
