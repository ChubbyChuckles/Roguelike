/* Simple projectile subsystem */
#ifndef ROGUE_CORE_PROJECTILES_H
#define ROGUE_CORE_PROJECTILES_H
#include <stddef.h>

typedef struct RogueProjectile {
    int active;
    float x,y;
    float vx,vy; /* units per second */
    float speed;
    float life_ms;
    float max_life_ms;
    int damage;
    /* FX fields */
    float spawn_ms;    /* time since game start when spawned (ms) */
    float anim_t;      /* accumulated animation time (ms) */
    /* Simple trail history (last few positions) */
    #define ROGUE_PROJECTILE_HISTORY 6
    float hx[ROGUE_PROJECTILE_HISTORY];
    float hy[ROGUE_PROJECTILE_HISTORY];
    int hcount; /* number of valid history samples */
} RogueProjectile;

void rogue_projectiles_init(void);
void rogue_projectiles_spawn(float x, float y, float dir_x, float dir_y, float speed, float life_ms, int damage);
void rogue_projectiles_update(float dt_ms);
void rogue_projectiles_render(void);

/* Testing / introspection helpers */
int rogue_projectiles_active_count(void);
int rogue_projectiles_last_damage(void);

#endif
