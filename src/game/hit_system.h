/* Hit System Phase 1-2: Weapon geometry & runtime sweep capsule generation */
#pragma once
#include "entities/enemy.h"
#include "entities/player.h"

#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueWeaponHitGeo {
    int weapon_id;          /* matches RogueWeaponDef id */
    float length;           /* reach in world units */
    float width;            /* diameter of capsule (width) */
    float pivot_dx;         /* offset from player origin */
    float pivot_dy;
    int slash_vfx_id;       /* reserved for Phase 5 */
} RogueWeaponHitGeo;

/* Load weapon geometry table from JSON file; returns count or <0 on error. */
int rogue_weapon_hit_geo_load_json(const char* path);
/* Fallback: register a single default geometry if none loaded. */
void rogue_weapon_hit_geo_ensure_default(void);
/* Lookup geometry for weapon id; returns NULL if not found. */
const RogueWeaponHitGeo* rogue_weapon_hit_geo_get(int weapon_id);

/* Capsule representation (segment + radius) */
typedef struct RogueCapsule { float x0,y0,x1,y1,r; } RogueCapsule;

/* Build capsule for current strike frame based on player + geometry. */
int rogue_weapon_build_capsule(const RoguePlayer* p, const RogueWeaponHitGeo* geo, RogueCapsule* out);

/* Test enemy overlap (treat enemy as circle radius 0.40f) */
int rogue_capsule_overlaps_enemy(const RogueCapsule* c, const RogueEnemy* e);

/* Apply sweep: returns hit count; integrates with combat_strike path (Phase 2). */
struct RoguePlayerCombat; /* forward declare to avoid including combat.h heavy dependency */
int rogue_combat_weapon_sweep_apply(struct RoguePlayerCombat* pc, struct RoguePlayer* player, struct RogueEnemy enemies[], int enemy_count);

/* Debug capture for overlay */
typedef struct RogueHitDebugFrame {
    RogueCapsule last_capsule; int capsule_valid; int last_hits[32]; int hit_count; int frame_id; float normals[32][2];
} RogueHitDebugFrame;

const RogueHitDebugFrame* rogue_hit_debug_last(void);
void rogue_hit_debug_store(const RogueCapsule* c, int* indices, float (*normals)[2], int hit_count, int frame_id);
void rogue_hit_debug_toggle(int on);
extern int g_hit_debug_enabled;

/* Reset per-strike hit mask (call when entering STRIKE phase) */
void rogue_hit_sweep_reset(void);
/* Retrieve last sweep indices list (returns count, sets *out_indices) */
int rogue_hit_last_indices(const int** out_indices);

/* Runtime hitbox tuning (adjustable via hotkeys, persisted to external file) */
typedef struct RogueHitboxTuning {
    float player_offset_x, player_offset_y; /* shifts player capsule anchor */
    float player_length, player_width;      /* overrides geometry length/width if >0 */
    float enemy_radius;                     /* enemy collision circle radius */
    float enemy_offset_x, enemy_offset_y;   /* shift enemy circle center */
    float pursue_offset_x, pursue_offset_y; /* enemy AI target point relative to player (Shift+1..4 adjust) */
} RogueHitboxTuning;

RogueHitboxTuning* rogue_hitbox_tuning_get(void);
int rogue_hitbox_tuning_load(const char* path, RogueHitboxTuning* out);
int rogue_hitbox_tuning_save(const char* path, const RogueHitboxTuning* t);
/* Resolve and cache a usable path for tuning file given typical run dirs; returns 1 if resolved */
int rogue_hitbox_tuning_resolve_path(void);
/* Returns last resolved full relative path */
const char* rogue_hitbox_tuning_path(void);
/* Convenience: save using resolved path (calls resolve if needed) */
int rogue_hitbox_tuning_save_resolved(void);

#ifdef __cplusplus
}
#endif
