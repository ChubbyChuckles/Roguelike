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
} RogueProjectile;

void rogue_projectiles_init(void);
void rogue_projectiles_spawn(float x, float y, float dir_x, float dir_y, float speed, float life_ms, int damage);
void rogue_projectiles_update(float dt_ms);
void rogue_projectiles_render(void);

#endif
