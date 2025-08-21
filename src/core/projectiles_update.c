#include "core/app_state.h"
#include "core/projectiles_config.h"
#include "core/projectiles_internal.h"
#include "entities/enemy.h"
#include "game/damage_numbers.h"
#include <math.h>
#include <stdlib.h>

void rogue__spawn_shards(float x, float y, int count)
{
    const struct RogueProjectileTuning* t = rogue_projectiles_tuning();
    for (int c = 0; c < count; c++)
    {
        for (int i = 0; i < ROGUE_MAX_SHARDS; i++)
            if (!g_shards[i].active)
            {
                float ang = (float) ((double) rand() / (double) RAND_MAX) * 6.2831853f;
                float spd =
                    t->shard_speed_min + ((float) rand() / (float) RAND_MAX) * t->shard_speed_var;
                g_shards[i].active = 1;
                g_shards[i].x = x;
                g_shards[i].y = y;
                g_shards[i].vx = cosf(ang) * spd;
                g_shards[i].vy = sinf(ang) * spd;
                g_shards[i].life_ms = t->shard_life_min_ms +
                                      ((float) rand() / (float) RAND_MAX) * t->shard_life_var_ms;
                g_shards[i].total_ms = g_shards[i].life_ms;
                g_shards[i].size =
                    t->shard_size_min + ((float) rand() / (float) RAND_MAX) * t->shard_size_var;
                break;
            }
    }
}
static void projectile_hit_enemy(RogueProjectile* p, struct RogueEnemy* e)
{
    e->health -= p->damage;
    rogue_add_damage_number(p->x, p->y - 0.3f, p->damage, 1);
    rogue__spawn_impact(p->x, p->y);
    rogue__spawn_shards(p->x, p->y, rogue_projectiles_tuning()->shard_count_hit);
    if (e->health <= 0)
    {
        e->alive = 0;
        g_app.enemy_count--;
        if (g_app.per_type_counts[e->type_index] > 0)
            g_app.per_type_counts[e->type_index]--;
    }
}
void rogue__projectile_hit_enemy(struct RogueProjectile* p, struct RogueEnemy* e)
{
    projectile_hit_enemy(p, e);
}

void rogue_projectiles_update(float dt_ms)
{
    for (int i = 0; i < ROGUE_MAX_PROJECTILES; i++)
        if (g_projectiles[i].active)
        {
            RogueProjectile* p = &g_projectiles[i];
            p->life_ms += dt_ms;
            if (p->life_ms >= p->max_life_ms)
            {
                rogue__spawn_impact(p->x, p->y);
                rogue__spawn_shards(p->x, p->y, rogue_projectiles_tuning()->shard_count_expire);
                p->active = 0;
                continue;
            }
            p->anim_t += dt_ms;
            if (p->hcount < ROGUE_PROJECTILE_HISTORY)
            {
                for (int h = p->hcount; h > 0; --h)
                {
                    p->hx[h] = p->hx[h - 1];
                    p->hy[h] = p->hy[h - 1];
                }
                p->hx[0] = p->x;
                p->hy[0] = p->y;
                p->hcount++;
            }
            else
            {
                for (int h = ROGUE_PROJECTILE_HISTORY - 1; h > 0; --h)
                {
                    p->hx[h] = p->hx[h - 1];
                    p->hy[h] = p->hy[h - 1];
                }
                p->hx[0] = p->x;
                p->hy[0] = p->y;
            }
            p->x += p->vx * (dt_ms * (1.0f / 1000.0f));
            p->y += p->vy * (dt_ms * (1.0f / 1000.0f));
            if (p->x < 0 || p->y < 0 || p->x >= g_app.world_map.width ||
                p->y >= g_app.world_map.height)
            {
                p->active = 0;
                continue;
            }
            for (int ei = 0; ei < ROGUE_MAX_ENEMIES; ++ei)
            {
                if (g_app.enemies[ei].alive)
                {
                    float dx = g_app.enemies[ei].base.pos.x - p->x;
                    float dy = g_app.enemies[ei].base.pos.y - p->y;
                    if (dx * dx + dy * dy < 0.5f * 0.5f)
                    {
                        projectile_hit_enemy(p, &g_app.enemies[ei]);
                        p->active = 0;
                        break;
                    }
                }
            }
        }
}

static void update_impacts(float dt_ms)
{
    for (int i = 0; i < ROGUE_MAX_IMPACT_BURSTS; i++)
        if (g_impacts[i].active)
        {
            g_impacts[i].life_ms -= dt_ms;
            if (g_impacts[i].life_ms <= 0)
            {
                g_impacts[i].active = 0;
            }
        }
}
void rogue__update_impacts(float dt_ms) { update_impacts(dt_ms); }

static void update_shards(float dt_ms)
{
    float dt = dt_ms * (1.0f / 1000.0f);
    for (int i = 0; i < ROGUE_MAX_SHARDS; i++)
        if (g_shards[i].active)
        {
            g_shards[i].life_ms -= dt_ms;
            g_shards[i].x += g_shards[i].vx * dt;
            g_shards[i].y += g_shards[i].vy * dt;
            g_shards[i].vy += rogue_projectiles_tuning()->shard_gravity * dt;
            if (g_shards[i].life_ms <= 0)
            {
                g_shards[i].active = 0;
            }
        }
}
void rogue__update_shards(float dt_ms) { update_shards(dt_ms); }

int rogue_projectiles_active_count(void)
{
    int n = 0;
    for (int i = 0; i < ROGUE_MAX_PROJECTILES; i++)
        if (g_projectiles[i].active)
            n++;
    return n;
}
int rogue_projectiles_last_damage(void) { return g_last_projectile_damage; }
