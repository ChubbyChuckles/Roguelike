#include "entity_debug.h"
#include "../app/app_state.h"
#include <string.h>

int rogue_entity_debug_count(void)
{
    int n = 0;
    for (int i = 0; i < ROGUE_MAX_ENEMIES; ++i)
        if (g_app.enemies[i].alive)
            n++;
    return n;
}

int rogue_entity_debug_list(int* out_indices, int cap)
{
    if (!out_indices || cap <= 0)
        return 0;
    int n = 0;
    for (int i = 0; i < ROGUE_MAX_ENEMIES && n < cap; ++i)
    {
        if (g_app.enemies[i].alive)
            out_indices[n++] = i;
    }
    return n;
}

int rogue_entity_debug_get_info(int slot_index, RogueEntityDebugInfo* out)
{
    if (!out || slot_index < 0 || slot_index >= ROGUE_MAX_ENEMIES)
        return -1;
    const RogueEnemy* e = &g_app.enemies[slot_index];
    out->slot_index = slot_index;
    out->alive = e->alive;
    out->type_index = e->type_index;
    out->x = e->base.pos.x;
    out->y = e->base.pos.y;
    out->health = e->health;
    out->max_health = e->max_health;
    return 0;
}

int rogue_entity_debug_teleport(int slot_index, float x, float y)
{
    if (slot_index < 0 || slot_index >= ROGUE_MAX_ENEMIES)
        return -1;
    RogueEnemy* e = &g_app.enemies[slot_index];
    if (!e->alive)
        return -2;
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->anchor_x = x;
    e->anchor_y = y;
    e->patrol_target_x = x;
    e->patrol_target_y = y;
    return 0;
}

int rogue_entity_debug_kill(int slot_index)
{
    if (slot_index < 0 || slot_index >= ROGUE_MAX_ENEMIES)
        return -1;
    RogueEnemy* e = &g_app.enemies[slot_index];
    if (!e->alive)
        return -2;
    e->alive = 0;
    if (g_app.enemy_count > 0)
        g_app.enemy_count--;
    return 0;
}

int rogue_entity_debug_spawn_at_player(float dx, float dy)
{
    /* Use existing test helper to ensure consistent initialization */
    RogueEnemy* ne = rogue_test_spawn_hostile_enemy(dx, dy);
    if (!ne)
        return -1;
    int idx = (int) (ne - g_app.enemies);
    return idx;
}
