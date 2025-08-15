#ifndef ROGUE_GAME_HITBOX_H
#define ROGUE_GAME_HITBOX_H

/* Phase 5.1: Spatial Hitbox Primitives
   These lightweight structs provide a unified representation for transient attack
   collision volumes ahead of full authoring / broadphase integration (5.2+).

   Design goals:
   - POD types (no dynamic allocation) suitable for stack / SoA packing later.
   - Deterministic math (pure functions; callers supply world positions).
   - Minimal intersection helpers vs points (enemy/player centers) for tests.
*/

typedef enum RogueHitboxType {
    ROGUE_HITBOX_CAPSULE = 1,
    ROGUE_HITBOX_ARC     = 2,
    ROGUE_HITBOX_CHAIN   = 3,
    ROGUE_HITBOX_PROJECTILE_SPAWN = 4
} RogueHitboxType;

typedef struct RogueHitboxCapsule { float ax, ay; float bx, by; float radius; } RogueHitboxCapsule;
typedef struct RogueHitboxArc { float ox, oy; float radius; float angle_start; float angle_end; float inner_radius; } RogueHitboxArc; /* inner_radius=0 for wedge */
/* Chain: up to N points, each consecutive pair forms a capsule with uniform radius */
#define ROGUE_HITBOX_CHAIN_MAX_POINTS 8
typedef struct RogueHitboxChain { int count; float radius; float px[ROGUE_HITBOX_CHAIN_MAX_POINTS]; float py[ROGUE_HITBOX_CHAIN_MAX_POINTS]; } RogueHitboxChain;

/* Projectile spawn descriptor (not a spatial test; describes future projectiles) */
typedef struct RogueHitboxProjectileSpawn { int projectile_count; float origin_x, origin_y; float base_speed; float spread_radians; float angle_center; } RogueHitboxProjectileSpawn;

typedef struct RogueHitbox {
    RogueHitboxType type;
    union {
        RogueHitboxCapsule capsule;
        RogueHitboxArc arc;
        RogueHitboxChain chain;
        RogueHitboxProjectileSpawn proj;
    } u;
} RogueHitbox;

/* Builders */
static inline RogueHitbox rogue_hitbox_make_capsule(float ax,float ay,float bx,float by,float radius){ RogueHitbox h; h.type=ROGUE_HITBOX_CAPSULE; h.u.capsule.ax=ax; h.u.capsule.ay=ay; h.u.capsule.bx=bx; h.u.capsule.by=by; h.u.capsule.radius=radius; return h; }
static inline RogueHitbox rogue_hitbox_make_arc(float ox,float oy,float radius,float a0,float a1,float inner_r){ RogueHitbox h; h.type=ROGUE_HITBOX_ARC; h.u.arc.ox=ox; h.u.arc.oy=oy; h.u.arc.radius=radius; h.u.arc.angle_start=a0; h.u.arc.angle_end=a1; h.u.arc.inner_radius=inner_r; return h; }
static inline RogueHitbox rogue_hitbox_make_chain(float radius){ RogueHitbox h; h.type=ROGUE_HITBOX_CHAIN; h.u.chain.count=0; h.u.chain.radius=radius; return h; }
static inline RogueHitbox rogue_hitbox_make_projectile_spawn(int count,float ox,float oy,float speed,float spread,float center){ RogueHitbox h; h.type=ROGUE_HITBOX_PROJECTILE_SPAWN; h.u.proj.projectile_count=count; h.u.proj.origin_x=ox; h.u.proj.origin_y=oy; h.u.proj.base_speed=speed; h.u.proj.spread_radians=spread; h.u.proj.angle_center=center; return h; }

/* Mutators for chain */
static inline void rogue_hitbox_chain_add_point(RogueHitbox* h,float x,float y){ if(!h||h->type!=ROGUE_HITBOX_CHAIN) return; if(h->u.chain.count < ROGUE_HITBOX_CHAIN_MAX_POINTS){ h->u.chain.px[h->u.chain.count]=x; h->u.chain.py[h->u.chain.count]=y; h->u.chain.count++; }}

/* Intersection helpers */
int rogue_hitbox_point_overlap(const RogueHitbox* h, float x, float y);

/* Utility: compute per-projectile firing angle i (0..count-1) evenly distributed across spread centered at angle_center.
   If count==1, returns center. */
float rogue_hitbox_projectile_spawn_angle(const RogueHitboxProjectileSpawn* ps, int index);

#endif
