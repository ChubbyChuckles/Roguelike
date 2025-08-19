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
    /* Legacy capsule debug */
    RogueCapsule last_capsule; int capsule_valid;
    /* Authoritative hits used for damage (pixel if available else capsule) */
    int last_hits[32]; int hit_count; float normals[32][2];
    /* Dual-path comparison (Slice C) */
    int capsule_hits[32]; int capsule_hit_count; /* raw capsule results */
    int pixel_hits[32]; int pixel_hit_count;     /* raw pixel results */
    int pixel_used;                              /* 1 if pixel hits were authoritative */
    int mismatch_pixel_only;                    /* count for this frame */
    int mismatch_capsule_only;                  /* count for this frame */
    /* Pixel mask visualization data */
    int pixel_mask_valid; int mask_w, mask_h; int mask_origin_x, mask_origin_y; unsigned int mask_pitch_words; const uint32_t* mask_bits;
    float mask_player_x, mask_player_y; float mask_pose_dx, mask_pose_dy; float mask_scale_x; float mask_scale_y; /* independent axis scale */
    int frame_id;
} RogueHitDebugFrame;

const RogueHitDebugFrame* rogue_hit_debug_last(void);
/* Internal debug helper (not for gameplay code) */
struct RogueHitDebugFrame* rogue__debug_frame_mut(void);
void rogue_hit_debug_store(const RogueCapsule* c, int* indices, float (*normals)[2], int hit_count, int frame_id);
/* Extended dual-path store capturing both capsule & pixel sets plus transform info (Slice C) */
void rogue_hit_debug_store_dual(const RogueCapsule* c,
    int* capsule_indices, int capsule_count,
    int* pixel_indices, int pixel_count,
    float (*normals)[2], int pixel_used,
    int mismatch_pixel_only, int mismatch_capsule_only,
    int frame_id,
    int mask_w, int mask_h, int mask_origin_x, int mask_origin_y,
    float player_x, float player_y, float pose_dx, float pose_dy, float scale_x, float scale_y);
void rogue_hit_debug_toggle(int on);
extern int g_hit_debug_enabled;

/* Reset per-strike hit mask (call when entering STRIKE phase) */
void rogue_hit_sweep_reset(void);
/* Retrieve last sweep indices list (returns count, sets *out_indices) */
int rogue_hit_last_indices(const int** out_indices);

/* Mismatch counters (cumulative since start / reset on demand) */
void rogue_hit_mismatch_counters(int* out_pixel_only, int* out_capsule_only);
void rogue_hit_mismatch_counters_reset(void);

/* Runtime hitbox tuning (adjustable via hotkeys, persisted to external file) */
typedef struct RogueHitboxTuning {
    float player_offset_x, player_offset_y; /* shifts player capsule anchor */
    float player_length, player_width;      /* overrides geometry length/width if >0 */
    float enemy_radius;                     /* enemy collision circle radius */
    float enemy_offset_x, enemy_offset_y;   /* shift enemy circle center */
    float pursue_offset_x, pursue_offset_y; /* enemy AI target point relative to player (Shift+1..4 adjust) */
    /* Per facing (0=down,1=left,2=right,3=up) pixel mask offset + scale overrides for visualization & sampling */
    float mask_dx[4];
    float mask_dy[4];
    float mask_scale_x[4]; /* width scale */
    float mask_scale_y[4]; /* height scale */
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
