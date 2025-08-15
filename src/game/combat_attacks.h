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

typedef struct RogueAttackWindow {
    float start_ms; float end_ms;          /* [start_ms, end_ms) interval within strike phase */
    unsigned short flags;                  /* per-window cancel/status flags (reuse global cancel bits subset) */
    float damage_mult;                     /* per-window scalar applied to base + scaling damage (1.0 default) */
    float bleed_build; float frost_build;  /* per-window status buildup contributions (Phase 1A status scaffolding) */
    short start_frame; short end_frame;    /* derived frame indices (lazy-computed); inclusive frame span */
} RogueAttackWindow;

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
    RogueAttackWindow windows[ROGUE_MAX_ATTACK_WINDOWS]; /* if num_windows==0 treat whole active_ms as single implicit window */
    /* --- Newly added extended fields (Phase 1.2/1.5 advancements) --- */
    float poise_cost;               /* attacker poise cost / tax (placeholder) */
    unsigned short cancel_flags;    /* bit0=on_hit (implicit), bit1=on_whiff early cancel, bit2=on_block (future) */
    float whiff_cancel_pct;         /* fraction of active_ms after which whiff cancel allowed */
    float bleed_build;              /* status buildup contributions (future pipeline) */
    float frost_build;
    // Cancel flag bits (expand later):
    // 0x0001 = early cancel on hit confirm allowed
    // 0x0002 = early cancel on whiff after whiff_cancel_pct of active window elapsed
    // 0x0004 = early cancel when attack is blocked (deflected) via notify hook
    // Additional bits reserved for parry, dodge, special, etc.
} RogueAttackDef;

/* Cancel flag convenience macros */
#define ROGUE_CANCEL_ON_HIT   0x0001
#define ROGUE_CANCEL_ON_WHIFF 0x0002
#define ROGUE_CANCEL_ON_BLOCK 0x0004

/* Returns pointer to attack def for archetype + chain index (wrap/clamp). */
const RogueAttackDef* rogue_attack_get(RogueWeaponArchetype arch, int chain_index);
/* Returns length of chain (number of defs) for archetype. */
int rogue_attack_chain_length(RogueWeaponArchetype arch);

#ifdef __cplusplus
}
#endif

#endif
