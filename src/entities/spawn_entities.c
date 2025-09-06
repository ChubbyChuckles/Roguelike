#include "../core/app/app_state.h"
#include "entity.h"
#include <string.h>

/* Lightweight transient entity pool (placeholder for Phase 6 summon system). */
#define ROGUE_SPAWN_ENTITY_CAP 64

typedef struct SpawnEnt
{
    RogueEntity e;
    double expiry_ms;
    unsigned char active;
} SpawnEnt;

static SpawnEnt g_spawn_entities[ROGUE_SPAWN_ENTITY_CAP];

void rogue_spawn_entities_reset(void) { memset(g_spawn_entities, 0, sizeof g_spawn_entities); }

void rogue_spawn_entity(float x, float y, double expiry_ms)
{
    for (int i = 0; i < ROGUE_SPAWN_ENTITY_CAP; ++i)
    {
        if (!g_spawn_entities[i].active)
        {
            g_spawn_entities[i].active = 1;
            g_spawn_entities[i].e.pos.x = x;
            g_spawn_entities[i].e.pos.y = y;
            g_spawn_entities[i].e.vel.x = 0.0f;
            g_spawn_entities[i].e.vel.y = 0.0f;
            g_spawn_entities[i].expiry_ms = expiry_ms;
            return;
        }
    }
}

int rogue_spawn_entity_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_SPAWN_ENTITY_CAP; ++i)
        if (g_spawn_entities[i].active)
            c++;
    return c;
}

void rogue_spawn_entities_update(double now_ms)
{
    for (int i = 0; i < ROGUE_SPAWN_ENTITY_CAP; ++i)
    {
        if (g_spawn_entities[i].active && now_ms >= g_spawn_entities[i].expiry_ms)
            g_spawn_entities[i].active = 0;
    }
}
