/* Collision, queries, toggles, info helpers for vegetation */
#include "vegetation_internal.h"
#include <math.h>

int rogue_vegetation_count(void) { return g_instance_count; }
int rogue_vegetation_tree_count(void)
{
    int c = 0;
    for (int i = 0; i < g_instance_count; i++)
        if (g_instances[i].is_tree)
            c++;
    return c;
}
int rogue_vegetation_plant_count(void)
{
    int c = 0;
    for (int i = 0; i < g_instance_count; i++)
        if (!g_instances[i].is_tree)
            c++;
    return c;
}

int rogue_vegetation_tile_blocking(int tx, int ty)
{
    if (!g_canopy_tile_blocking_enabled)
        return 0;
    float fx = tx + 0.5f, fy = ty + 0.5f;
    for (int i = 0; i < g_instance_count; i++)
    {
        if (!g_instances[i].is_tree)
            continue;
        RogueVegetationInstance* inst = &g_instances[i];
        RogueVegetationDef* def = &g_defs[inst->def_index];
        float dx = inst->x - fx;
        float dy = inst->y - fy;
        float r = (float) def->canopy_radius;
        if (r < 0.5f)
            r = 0.5f;
        if (dx * dx + dy * dy <= (r + 0.1f) * (r + 0.1f))
            return 1;
    }
    return 0;
}
float rogue_vegetation_tile_move_scale(int tx, int ty)
{
    float fx = tx + 0.5f, fy = ty + 0.5f;
    for (int i = 0; i < g_instance_count; i++)
    {
        if (!g_instances[i].is_tree)
        {
            float dx = g_instances[i].x - fx;
            float dy = g_instances[i].y - fy;
            if (fabsf(dx) < 0.51f && fabsf(dy) < 0.51f)
                return 0.85f;
        }
    }
    return 1.0f;
}

int rogue_vegetation_entity_blocking(float ox, float oy, float nx, float ny)
{
    if (!g_trunk_collision_enabled)
        return 0;
    (void) ox;
    (void) oy;
    if (g_instance_count == 0)
        return 0;
    float px = nx, py = ny;
    for (int i = 0; i < g_instance_count; i++)
        if (g_instances[i].is_tree)
        {
            RogueVegetationInstance* inst = &g_instances[i];
            RogueVegetationDef* def = &g_defs[inst->def_index];
            int tiles_w = (int) (def->tile_x2 - def->tile_x + 1);
            float base_x = inst->x;
            float base_y = inst->y;
            float trunk_r = 0.30f + tiles_w * 0.05f;
            if (trunk_r > 0.55f)
                trunk_r = 0.55f;
            if (trunk_r < 0.30f)
                trunk_r = 0.30f;
            float trunk_top = base_y - 0.30f;
            float trunk_bottom = base_y + 0.05f;
            float cushion_top = trunk_top - 0.12f;
            float dx = px - base_x;
            float adx = fabsf(dx);
            float y = py;
            if (adx > trunk_r)
                continue;
            if (y >= trunk_top && y <= trunk_bottom)
                return 1;
            if (ny > oy && y >= cushion_top && y < trunk_top)
                return 1;
        }
    return 0;
}

int rogue_vegetation_first_tree(int* out_tx, int* out_ty, int* out_radius)
{
    for (int i = 0; i < g_instance_count; i++)
        if (g_instances[i].is_tree)
        {
            RogueVegetationInstance* inst = &g_instances[i];
            RogueVegetationDef* def = &g_defs[inst->def_index];
            if (out_tx)
                *out_tx = (int) floorf(inst->x);
            if (out_ty)
                *out_ty = (int) floorf(inst->y);
            if (out_radius)
                *out_radius = (int) def->canopy_radius;
            return 1;
        }
    return 0;
}
int rogue_vegetation_tree_info(int index, float* out_x, float* out_y, int* out_tiles_w,
                               int* out_tiles_h)
{
    int t_index = -1;
    for (int i = 0; i < g_instance_count; i++)
        if (g_instances[i].is_tree)
        {
            ++t_index;
            if (t_index == index)
            {
                RogueVegetationInstance* inst = &g_instances[i];
                RogueVegetationDef* def = &g_defs[inst->def_index];
                if (out_x)
                    *out_x = inst->x;
                if (out_y)
                    *out_y = inst->y;
                if (out_tiles_w)
                    *out_tiles_w = (int) (def->tile_x2 - def->tile_x + 1);
                if (out_tiles_h)
                    *out_tiles_h = (int) (def->tile_y2 - def->tile_y + 1);
                return 1;
            }
        }
    return 0;
}

void rogue_vegetation_set_trunk_collision_enabled(int enabled)
{
    g_trunk_collision_enabled = enabled ? 1 : 0;
}
int rogue_vegetation_get_trunk_collision_enabled(void) { return g_trunk_collision_enabled; }
void rogue_vegetation_set_canopy_tile_blocking_enabled(int enabled)
{
    g_canopy_tile_blocking_enabled = enabled ? 1 : 0;
}
int rogue_vegetation_get_canopy_tile_blocking_enabled(void)
{
    return g_canopy_tile_blocking_enabled;
}
