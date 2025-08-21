#include "ai/core/ai_scheduler.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <math.h>

static unsigned int g_frame = 0;
static int g_buckets = 4;          /* default bucket count (tests may override) */
static float g_lod_radius = 18.0f; /* tiles */
static float g_lod_radius_sq = 18.0f * 18.0f;

void rogue_ai_scheduler_set_buckets(int buckets)
{
    if (buckets < 1)
        buckets = 1;
    g_buckets = buckets;
}
int rogue_ai_scheduler_get_buckets(void) { return g_buckets; }
void rogue_ai_lod_set_radius(float radius)
{
    if (radius < 0.0f)
        radius = 0.0f;
    g_lod_radius = radius;
    g_lod_radius_sq = radius * radius;
}
float rogue_ai_lod_get_radius(void) { return g_lod_radius; }
unsigned int rogue_ai_scheduler_frame(void) { return g_frame; }
void rogue_ai_scheduler_reset_for_tests(void)
{
    g_frame = 0;
    g_buckets = 4;
    rogue_ai_lod_set_radius(18.0f);
}

/* Cheap maintenance tick: could handle timers/decay w/o full BT cost (placeholder). */
static void maintenance_tick(RogueEnemy* e, float dt)
{
    (void) dt; /* future: threat decay, status timers */
    (void) e;
}

void rogue_ai_scheduler_tick(RogueEnemy* enemies, int count, float dt_seconds)
{
    if (!enemies || count <= 0)
    {
        g_frame++;
        return;
    }
    int bucket = g_frame % g_buckets;
    for (int i = 0; i < count; i++)
    {
        RogueEnemy* e = &enemies[i];
        if (!e->alive)
            continue;
        if (!e->ai_bt_enabled)
            continue; /* only BT-enabled enemies considered */
        /* LOD distance test */
        float dx = e->base.pos.x - g_app.player.base.pos.x;
        float dy = e->base.pos.y - g_app.player.base.pos.y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 > g_lod_radius_sq)
        {
            maintenance_tick(e, dt_seconds);
            continue;
        }
        /* Bucket selection: only process BT for enemies whose index mod bucket count equals current
         * bucket */
        if (g_buckets > 1)
        {
            if ((i % g_buckets) != bucket)
            {
                maintenance_tick(e, dt_seconds);
                continue;
            }
        }
        /* Full behavior tree tick */
        rogue_enemy_ai_bt_tick(e, dt_seconds);
    }
    g_frame++;
}
