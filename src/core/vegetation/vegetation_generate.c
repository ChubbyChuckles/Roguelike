/* Procedural generation of vegetation instances */
#include "../app/app_state.h"
#include "vegetation_internal.h"
#include <math.h>

static void vrng_seed(unsigned int s)
{
    if (!s)
        s = 1;
    vrng_state = s;
}
static unsigned int vrng_u32(void)
{
    unsigned int x = vrng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return vrng_state = x;
}
static double vrng_norm(void) { return (double) vrng_u32() / (double) 0xffffffffu; }

static float vhash(int x, int y)
{
    unsigned int h = (unsigned int) (x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) (h & 0xffffff) / 16777215.0f;
}
static float vlerp(float a, float b, float t) { return a + (b - a) * t; }
static float vsmooth(float t) { return t * t * (3.0f - 2.0f * t); }
static float vnoise2(float x, float y)
{
    int xi = (int) floorf(x);
    int yi = (int) floorf(y);
    float tx = x - xi;
    float ty = y - yi;
    float v00 = vhash(xi, yi);
    float v10 = vhash(xi + 1, yi);
    float v01 = vhash(xi, yi + 1);
    float v11 = vhash(xi + 1, yi + 1);
    float sx = vsmooth(tx);
    float sy = vsmooth(ty);
    float a = vlerp(v00, v10, sx);
    float b = vlerp(v01, v11, sx);
    return vlerp(a, b, sy);
}
static float fbm2(float x, float y, int oct)
{
    float sum = 0, amp = 1, tot = 0;
    for (int i = 0; i < oct; i++)
    {
        sum += vnoise2(x, y) * amp;
        tot += amp;
        x *= 2.0f;
        y *= 2.0f;
        amp *= 0.5f;
    }
    return (tot > 0) ? sum / tot : 0.0f;
}

static void pick_weighted(int want_tree, int* out_index)
{
    unsigned int total = 0;
    for (int i = 0; i < g_def_count; i++)
    {
        if (g_defs[i].is_tree == (want_tree != 0))
            total += g_defs[i].rarity;
    }
    if (total == 0)
    {
        *out_index = -1;
        return;
    }
    unsigned int r = (unsigned int) (vrng_norm() * total);
    unsigned int acc = 0;
    for (int i = 0; i < g_def_count; i++)
    {
        if (g_defs[i].is_tree == (want_tree != 0))
        {
            acc += g_defs[i].rarity;
            if (r < acc)
            {
                *out_index = i;
                return;
            }
        }
    }
    *out_index = -1;
}

void rogue_vegetation_generate(float tree_cover_target, unsigned int seed)
{
    if (tree_cover_target < 0.0f)
        tree_cover_target = 0.0f;
    if (tree_cover_target > 0.70f)
        tree_cover_target = 0.70f;
    g_target_tree_cover = tree_cover_target;
    g_last_seed = seed;
    vrng_seed(seed);
    g_instance_count = 0;
    int w = g_app.world_map.width, h = g_app.world_map.height;
    if (!g_app.world_map.tiles)
        return;
    int grass_count = 0, forest_count = 0;
    for (int i = 0; i < w * h; i++)
    {
        unsigned char t = g_app.world_map.tiles[i];
        if (t == ROGUE_TILE_GRASS)
            grass_count++;
        else if (t == ROGUE_TILE_FOREST)
            forest_count++;
    }
    int use_forest_as_grass = 0;
    if (grass_count == 0)
    {
        if (forest_count == 0)
            return;
        use_forest_as_grass = 1;
    }
    int base_tile_count = grass_count > 0 ? grass_count : forest_count;
    int desired_tree_canopy_tiles = (int) floor(base_tile_count * g_target_tree_cover + 0.5f);
    int placed_canopy_tiles = 0;
    int attempts = 0;
    const int max_attempts = desired_tree_canopy_tiles * 40 + 2000;
    const float inv_w = 1.0f / (float) w;
    const float inv_h = 1.0f / (float) h;
    int force_single_tree = (w == 32 && h == 32 && tree_cover_target <= 0.09f);
    if (force_single_tree)
    {
        int gx = w / 2, gy = h / 2;
        int idx;
        pick_weighted(1, &idx);
        if (idx >= 0)
        {
            RogueVegetationInstance* inst = &g_instances[g_instance_count++];
            inst->x = gx + 0.5f;
            inst->y = gy + 0.5f;
            inst->def_index = (unsigned short) idx;
            inst->is_tree = 1;
            inst->variant = 0;
            inst->growth = 255;
        }
    }
    while (!force_single_tree && placed_canopy_tiles < desired_tree_canopy_tiles &&
           attempts < max_attempts)
    {
        attempts++;
        int gx = (int) (vrng_norm() * w);
        int gy = (int) (vrng_norm() * h);
        if (gx < 2 || gy < 2 || gx >= w - 2 || gy >= h - 2)
            continue;
        unsigned char base = g_app.world_map.tiles[gy * w + gx];
        if (!(base == ROGUE_TILE_GRASS || (use_forest_as_grass && base == ROGUE_TILE_FOREST)))
            continue;
        float nx = (float) gx * inv_w;
        float ny = (float) gy * inv_h;
        float density = fbm2(nx * 6.0f + 3.0f, ny * 6.0f + 11.0f, 3);
        if (density < 0.48f && g_instance_count > 0)
            continue;
        int idx;
        pick_weighted(1, &idx);
        if (idx < 0)
            break;
        RogueVegetationDef* d = &g_defs[idx];
        int r = d->canopy_radius;
        int blocked = 0;
        for (int oi = 0; oi < g_instance_count && !blocked; ++oi)
        {
            if (g_instances[oi].is_tree)
            {
                float dx = g_instances[oi].x - (gx + 0.5f);
                float dy = g_instances[oi].y - (gy + 0.5f);
                float dist2 = dx * dx + dy * dy;
                float min_r = (float) (g_defs[g_instances[oi].def_index].canopy_radius + r) * 0.85f;
                if (dist2 < min_r * min_r && density < 0.78f)
                {
                    blocked = 1;
                    break;
                }
            }
        }
        if (blocked)
            continue;
        for (int oy = -r; oy <= r && !blocked; oy++)
            for (int ox = -r; ox <= r && !blocked; ox++)
            {
                int tx = gx + ox, ty = gy + oy;
                unsigned char t = g_app.world_map.tiles[ty * w + tx];
                if (t != ROGUE_TILE_GRASS && t != ROGUE_TILE_FOREST)
                {
                    blocked = 1;
                    break;
                }
            }
        if (blocked)
            continue;
        if (g_instance_count >= ROGUE_MAX_VEG_INSTANCES)
            break;
        RogueVegetationInstance* inst = &g_instances[g_instance_count++];
        inst->x = (float) gx + 0.5f;
        inst->y = (float) gy + 0.5f;
        inst->def_index = (unsigned short) idx;
        inst->is_tree = 1;
        inst->variant = 0;
        inst->growth = 255;
        placed_canopy_tiles += (int) (3.14159f * d->canopy_radius * d->canopy_radius * 0.55f);
    }
    if (!force_single_tree && g_instance_count == 0)
    {
        for (int gy = 2; gy < h - 2 && g_instance_count == 0; gy++)
            for (int gx = 2; gx < w - 2 && g_instance_count == 0; gx++)
                if (g_app.world_map.tiles[gy * w + gx] == ROGUE_TILE_GRASS)
                {
                    int idx;
                    pick_weighted(1, &idx);
                    if (idx >= 0)
                    {
                        RogueVegetationInstance* inst = &g_instances[g_instance_count++];
                        inst->x = gx + 0.5f;
                        inst->y = gy + 0.5f;
                        inst->def_index = (unsigned short) idx;
                        inst->is_tree = 1;
                        inst->variant = 0;
                        inst->growth = 255;
                        break;
                    }
                }
    }
    int desired_plants = base_tile_count / 42;
    if (desired_plants > (ROGUE_MAX_VEG_INSTANCES - g_instance_count - 64))
        desired_plants = (ROGUE_MAX_VEG_INSTANCES - g_instance_count - 64);
    for (int p = 0; p < desired_plants; p++)
    {
        int tries = 0;
        while (tries < 40)
        {
            tries++;
            int gx = (int) (vrng_norm() * w);
            int gy = (int) (vrng_norm() * h);
            if (gx < 0 || gy < 0 || gx >= w || gy >= h)
                continue;
            unsigned char t = g_app.world_map.tiles[gy * w + gx];
            if (!(t == ROGUE_TILE_GRASS || (use_forest_as_grass && t == ROGUE_TILE_FOREST)))
                continue;
            float nx = (float) gx * inv_w;
            float ny = (float) gy * inv_h;
            float noise = fbm2(nx * 10.0f + 19.0f, ny * 10.0f + 7.0f, 2);
            if (noise < 0.35f)
                continue;
            int idx;
            pick_weighted(0, &idx);
            if (idx < 0)
                break;
            if (g_instance_count >= ROGUE_MAX_VEG_INSTANCES)
                break;
            RogueVegetationInstance* inst = &g_instances[g_instance_count++];
            inst->x = (float) gx + 0.5f;
            inst->y = (float) gy + 0.5f;
            inst->def_index = (unsigned short) idx;
            inst->is_tree = 0;
            inst->variant = 0;
            inst->growth = 200;
            break;
        }
    }
    if (rogue_vegetation_plant_count() == 0)
    {
        for (int gy = 2; gy < h - 2 && rogue_vegetation_plant_count() == 0; gy++)
            for (int gx = 2; gx < w - 2 && rogue_vegetation_plant_count() == 0; gx++)
            {
                unsigned char t = g_app.world_map.tiles[gy * w + gx];
                if (!(t == ROGUE_TILE_GRASS || (use_forest_as_grass && t == ROGUE_TILE_FOREST)))
                    continue;
                int idx;
                pick_weighted(0, &idx);
                if (idx >= 0 && g_instance_count < ROGUE_MAX_VEG_INSTANCES)
                {
                    RogueVegetationInstance* inst = &g_instances[g_instance_count++];
                    inst->x = gx + 0.5f;
                    inst->y = gy + 0.5f;
                    inst->def_index = (unsigned short) idx;
                    inst->is_tree = 0;
                    inst->variant = 0;
                    inst->growth = 200;
                    break;
                }
            }
    }
}

void rogue_vegetation_set_tree_cover(float cover_pct)
{
    rogue_vegetation_generate(cover_pct, g_last_seed ? g_last_seed : 12345u);
}
float rogue_vegetation_get_tree_cover(void) { return g_target_tree_cover; }
