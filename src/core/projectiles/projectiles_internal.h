#ifndef ROGUE_PROJECTILES_INTERNAL_H
#define ROGUE_PROJECTILES_INTERNAL_H
#include "projectiles.h"

#define ROGUE_MAX_PROJECTILES 128
#define ROGUE_MAX_IMPACT_BURSTS 64
#define ROGUE_MAX_SHARDS 256

typedef struct RogueImpactBurst
{
    float x, y;
    float life_ms;
    float total_ms;
    int active;
} RogueImpactBurst;
typedef struct RogueShard
{
    float x, y;
    float vx, vy;
    float life_ms;
    float total_ms;
    int active;
    float size;
} RogueShard;

extern RogueImpactBurst g_impacts[ROGUE_MAX_IMPACT_BURSTS];
extern RogueShard g_shards[ROGUE_MAX_SHARDS];
extern RogueProjectile g_projectiles[ROGUE_MAX_PROJECTILES];
extern int g_last_projectile_damage;

/* Internal helpers shared across update/render */
void rogue__spawn_impact(float x, float y);
void rogue__spawn_shards(float x, float y, int count);
struct RogueEnemy; /* forward declaration */
void rogue__projectile_hit_enemy(struct RogueProjectile* p, struct RogueEnemy* e);
void rogue__update_impacts(float dt_ms);
void rogue__update_shards(float dt_ms);

#endif /* ROGUE_PROJECTILES_INTERNAL_H */
