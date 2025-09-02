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

int rogue_entity_debug_duplicate(int src_slot_index, float dx, float dy)
{
    if (src_slot_index < 0 || src_slot_index >= ROGUE_MAX_ENEMIES)
        return -1;
    RogueEnemy* src = &g_app.enemies[src_slot_index];
    if (!src->alive)
        return -1;
    /* Find a free slot */
    int dst_idx = -1;
    for (int i = 0; i < ROGUE_MAX_ENEMIES; ++i)
    {
        if (!g_app.enemies[i].alive)
        {
            dst_idx = i;
            break;
        }
    }
    if (dst_idx < 0)
        return -1;
    RogueEnemy* dst = &g_app.enemies[dst_idx];
    /* Copy key fields */
    *dst = *src; /* start from a shallow copy */
    /* Adjust position and reset some runtime/transient fields for determinism */
    float nx = src->base.pos.x + dx;
    float ny = src->base.pos.y + dy;
    dst->base.pos.x = nx;
    dst->base.pos.y = ny;
    dst->anchor_x = nx;
    dst->anchor_y = ny;
    dst->patrol_target_x = nx;
    dst->patrol_target_y = ny;
    dst->hurt_timer = 0;
    dst->anim_time = 0;
    dst->anim_frame = 0;
    dst->death_fade = 1.0f;
    dst->tint_phase = 0;
    dst->flash_timer = 0;
    /* Keep current health/max, type_index, facing, AI state as copied */
    dst->alive = 1;
    g_app.enemy_count++;
    if (dst->type_index >= 0 && dst->type_index < ROGUE_MAX_ENEMY_TYPES)
        g_app.per_type_counts[dst->type_index]++;
    return dst_idx;
}
