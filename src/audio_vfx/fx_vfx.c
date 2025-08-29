#include "effects.h"
#include "fx_internal.h"
#include <math.h>
#include <string.h>

typedef struct VfxReg
{
    char id[24];
    uint8_t layer;
    uint8_t world_space;
    uint32_t lifetime_ms;
    float emit_hz;
    uint32_t p_lifetime_ms;
    int p_max;
    uint8_t var_scale_mode;
    float var_scale_a, var_scale_b;
    uint8_t var_life_mode;
    float var_life_a, var_life_b;
    uint8_t comp_mode;
    uint8_t comp_child_count;
    uint16_t comp_child_indices[8];
    uint32_t comp_child_delays[8];
    uint8_t blend;
    float trail_hz;
    uint32_t trail_life_ms;
    int trail_max;
} VfxReg;

typedef struct VfxInst
{
    uint16_t reg_index;
    uint16_t active;
    float x, y;
    uint32_t age_ms;
    float emit_accum;
    uint32_t ov_lifetime_ms;
    float ov_scale;
    uint32_t ov_color_rgba;
    uint8_t comp_next_child;
    uint32_t comp_last_spawn_ms;
    float trail_accum;
} VfxInst;

#define ROGUE_VFX_REG_CAP 64
static VfxReg g_vfx_reg[ROGUE_VFX_REG_CAP];
static int g_vfx_reg_count = 0;
#define ROGUE_VFX_INST_CAP 256
static VfxInst g_vfx_inst[ROGUE_VFX_INST_CAP];
static float g_vfx_timescale = 1.0f;
static int g_vfx_frozen = 0;
static float g_vfx_perf_scale = 1.0f;
static int g_vfx_gpu_batch = 0;

typedef struct VfxParticle
{
    uint8_t active, layer, world_space;
    uint16_t inst_idx;
    float x, y, scale;
    uint32_t color_rgba;
    uint32_t age_ms, lifetime_ms;
    uint8_t is_trail;
} VfxParticle;
#define ROGUE_VFX_PART_CAP 1024
static VfxParticle g_vfx_parts[ROGUE_VFX_PART_CAP];
static float g_cam_x = 0.0f, g_cam_y = 0.0f;
static float g_pixels_per_world = 32.0f;

typedef struct VfxFrameStats
{
    int spawned_core, spawned_trail, culled_soft, culled_hard, culled_pacing, active_particles,
        active_instances, active_decals;
} VfxFrameStats;
static VfxFrameStats g_vfx_stats_last, g_vfx_stats_accum;
static int g_budget_soft = 0, g_budget_hard = 0, g_pacing_enabled = 0, g_pacing_threshold = 0;

typedef struct DecalReg
{
    char id[24];
    uint8_t layer;
    uint8_t world_space;
    uint32_t lifetime_ms;
    float size;
} DecalReg;
typedef struct DecalInst
{
    uint16_t reg_index;
    uint8_t active;
    float x, y;
    float angle;
    float scale;
    uint32_t age_ms;
} DecalInst;
#define ROGUE_VFX_DECAL_REG_CAP 64
static DecalReg g_decal_reg[ROGUE_VFX_DECAL_REG_CAP];
static int g_decal_reg_count = 0;
#define ROGUE_VFX_DECAL_INST_CAP 256
static DecalInst g_decal_inst[ROGUE_VFX_DECAL_INST_CAP];

typedef struct Shake
{
    float amp, freq_hz;
    uint32_t dur_ms, age_ms;
    uint8_t active;
} Shake;
static Shake g_shakes[8];

static int vfx_reg_find(const char* id)
{
    for (int i = 0; i < g_vfx_reg_count; ++i)
        if (strncmp(g_vfx_reg[i].id, id, sizeof g_vfx_reg[i].id) == 0)
            return i;
    return -1;
}
static int vfx_part_alloc(void)
{
    for (int i = 0; i < ROGUE_VFX_PART_CAP; ++i)
        if (!g_vfx_parts[i].active)
            return i;
    return -1;
}
static void vfx_particles_update(float dt)
{
    uint32_t dms = (uint32_t) dt;
    for (int i = 0; i < ROGUE_VFX_PART_CAP; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        g_vfx_parts[i].age_ms += dms;
        if (g_vfx_parts[i].age_ms > g_vfx_parts[i].lifetime_ms)
            g_vfx_parts[i].active = 0;
    }
}
static int vfx_particles_layer_count(RogueVfxLayer layer)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP; ++i)
        if (g_vfx_parts[i].active && g_vfx_parts[i].layer == (uint8_t) layer)
            ++c;
    return c;
}

int rogue_vfx_particles_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP; ++i)
        if (g_vfx_parts[i].active)
            ++c;
    return c;
}
int rogue_vfx_particles_trail_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP; ++i)
        if (g_vfx_parts[i].active && g_vfx_parts[i].is_trail)
            ++c;
    return c;
}
int rogue_vfx_particles_layer_count(RogueVfxLayer layer)
{
    return vfx_particles_layer_count(layer);
}
int rogue_vfx_particles_collect_ordered(uint8_t* out_layers, int max)
{
    if (!out_layers || max <= 0)
        return 0;
    int w = 0;
    for (int lay = (int) ROGUE_VFX_LAYER_BG; lay <= (int) ROGUE_VFX_LAYER_UI; ++lay)
    {
        if (w >= max)
            break;
        if (vfx_particles_layer_count((RogueVfxLayer) lay) > 0)
            out_layers[w++] = (uint8_t) lay;
    }
    return w;
}
void rogue_vfx_set_camera(float cam_x, float cam_y, float pixels_per_world)
{
    g_cam_x = cam_x;
    g_cam_y = cam_y;
    if (pixels_per_world > 0.0f)
        g_pixels_per_world = pixels_per_world;
}
int rogue_vfx_particles_collect_screen(float* out_xy, uint8_t* out_layers, int max)
{
    if (!out_xy || max <= 0)
        return 0;
    int w = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && w < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        float sx = g_vfx_parts[i].x, sy = g_vfx_parts[i].y;
        if (g_vfx_parts[i].world_space)
        {
            sx = (sx - g_cam_x) * g_pixels_per_world;
            sy = (sy - g_cam_y) * g_pixels_per_world;
        }
        out_xy[w * 2 + 0] = sx;
        out_xy[w * 2 + 1] = sy;
        if (out_layers)
            out_layers[w] = g_vfx_parts[i].layer;
        ++w;
    }
    return w;
}

int rogue_vfx_registry_register(const char* id, RogueVfxLayer layer, uint32_t lifetime_ms,
                                int world_space)
{
    if (!id || !*id)
        return -1;
    int idx = vfx_reg_find(id);
    if (idx < 0)
    {
        if (g_vfx_reg_count >= ROGUE_VFX_REG_CAP)
            return -2;
        idx = g_vfx_reg_count++;
        memset(&g_vfx_reg[idx], 0, sizeof g_vfx_reg[idx]);
#if defined(_MSC_VER)
        strncpy_s(g_vfx_reg[idx].id, sizeof g_vfx_reg[idx].id, id, _TRUNCATE);
#else
        strncpy(g_vfx_reg[idx].id, id, sizeof g_vfx_reg[idx].id - 1);
        g_vfx_reg[idx].id[sizeof g_vfx_reg[idx].id - 1] = '\0';
#endif
    }
    g_vfx_reg[idx].layer = (uint8_t) layer;
    g_vfx_reg[idx].lifetime_ms = lifetime_ms;
    g_vfx_reg[idx].world_space = world_space ? 1 : 0;
    g_vfx_reg[idx].emit_hz = 0.0f;
    g_vfx_reg[idx].p_lifetime_ms = 0;
    g_vfx_reg[idx].p_max = 0;
    g_vfx_reg[idx].var_scale_mode = 0;
    g_vfx_reg[idx].var_scale_a = 1.0f;
    g_vfx_reg[idx].var_scale_b = 1.0f;
    g_vfx_reg[idx].var_life_mode = 0;
    g_vfx_reg[idx].var_life_a = 1.0f;
    g_vfx_reg[idx].var_life_b = 1.0f;
    g_vfx_reg[idx].comp_mode = 0;
    g_vfx_reg[idx].comp_child_count = 0;
    g_vfx_reg[idx].blend = (uint8_t) ROGUE_VFX_BLEND_ALPHA;
    g_vfx_reg[idx].trail_hz = 0.0f;
    g_vfx_reg[idx].trail_life_ms = 0u;
    g_vfx_reg[idx].trail_max = 0;
    return 0;
}
int rogue_vfx_registry_get(const char* id, RogueVfxLayer* out_layer, uint32_t* out_lifetime_ms,
                           int* out_world_space)
{
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -1;
    if (out_layer)
        *out_layer = (RogueVfxLayer) g_vfx_reg[idx].layer;
    if (out_lifetime_ms)
        *out_lifetime_ms = g_vfx_reg[idx].lifetime_ms;
    if (out_world_space)
        *out_world_space = g_vfx_reg[idx].world_space;
    return 0;
}
void rogue_vfx_registry_clear(void) { g_vfx_reg_count = 0; }
int rogue_vfx_registry_set_emitter(const char* id, float spawn_rate_hz,
                                   uint32_t particle_lifetime_ms, int max_particles)
{
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -1;
    if (spawn_rate_hz < 0)
        spawn_rate_hz = 0;
    if (max_particles < 0)
        max_particles = 0;
    g_vfx_reg[idx].emit_hz = spawn_rate_hz;
    g_vfx_reg[idx].p_lifetime_ms = particle_lifetime_ms;
    g_vfx_reg[idx].p_max = max_particles;
    return 0;
}
int rogue_vfx_registry_set_trail(const char* id, float trail_hz, uint32_t trail_lifetime_ms,
                                 int max_trail_particles)
{
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -1;
    if (trail_hz < 0)
        trail_hz = 0;
    if (max_trail_particles < 0)
        max_trail_particles = 0;
    g_vfx_reg[idx].trail_hz = trail_hz;
    g_vfx_reg[idx].trail_life_ms = trail_lifetime_ms;
    g_vfx_reg[idx].trail_max = max_trail_particles;
    return 0;
}
int rogue_vfx_registry_set_blend(const char* id, RogueVfxBlend blend)
{
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -1;
    g_vfx_reg[idx].blend = (uint8_t) blend;
    return 0;
}
int rogue_vfx_registry_get_blend(const char* id, RogueVfxBlend* out_blend)
{
    int idx = vfx_reg_find(id);
    if (idx < 0 || !out_blend)
        return -1;
    *out_blend = (RogueVfxBlend) g_vfx_reg[idx].blend;
    return 0;
}
int rogue_vfx_registry_set_variation(const char* id, RogueVfxDist scale_mode, float scale_a,
                                     float scale_b, RogueVfxDist lifetime_mode, float life_a,
                                     float life_b)
{
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -1;
    VfxReg* r = &g_vfx_reg[idx];
    r->var_scale_mode = (uint8_t) scale_mode;
    r->var_scale_a = scale_a;
    r->var_scale_b = scale_b;
    r->var_life_mode = (uint8_t) lifetime_mode;
    r->var_life_a = life_a;
    r->var_life_b = life_b;
    return 0;
}

static int vfx_inst_alloc(void)
{
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
    {
        if (!g_vfx_inst[i].active)
        {
            g_vfx_inst[i].active = 1;
            g_vfx_inst[i].age_ms = 0;
            g_vfx_inst[i].emit_accum = 0.0f;
            g_vfx_inst[i].ov_lifetime_ms = 0;
            g_vfx_inst[i].ov_scale = 0.0f;
            g_vfx_inst[i].ov_color_rgba = 0u;
            g_vfx_inst[i].comp_next_child = 0;
            g_vfx_inst[i].comp_last_spawn_ms = 0;
            g_vfx_inst[i].trail_accum = 0.0f;
            return i;
        }
    }
    return -1;
}

void rogue_vfx_update(uint32_t dt_ms)
{
    if (g_vfx_frozen)
        return;
    memset(&g_vfx_stats_accum, 0, sizeof g_vfx_stats_accum);
    float dt = (float) dt_ms * (g_vfx_timescale < 0 ? 0 : g_vfx_timescale);
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
    {
        if (!g_vfx_inst[i].active)
            continue;
        g_vfx_inst[i].age_ms += (uint32_t) dt;
        VfxReg* r = &g_vfx_reg[g_vfx_inst[i].reg_index];
        uint32_t inst_life =
            g_vfx_inst[i].ov_lifetime_ms ? g_vfx_inst[i].ov_lifetime_ms : r->lifetime_ms;
        if (g_vfx_inst[i].age_ms >= inst_life)
            g_vfx_inst[i].active = 0;
        if (r->comp_mode && g_vfx_inst[i].active)
        {
            while (g_vfx_inst[i].comp_next_child < r->comp_child_count)
            {
                uint8_t ci = g_vfx_inst[i].comp_next_child;
                uint32_t delay = r->comp_child_delays[ci];
                uint32_t ref = (r->comp_mode == 1) ? g_vfx_inst[i].comp_last_spawn_ms : 0u;
                if (g_vfx_inst[i].age_ms >= ref + delay)
                {
                    uint16_t child_ridx = r->comp_child_indices[ci];
                    if (child_ridx < (uint16_t) g_vfx_reg_count)
                    {
                        int ii2 = vfx_inst_alloc();
                        if (ii2 >= 0)
                        {
                            g_vfx_inst[ii2].reg_index = child_ridx;
                            g_vfx_inst[ii2].x = g_vfx_inst[i].x;
                            g_vfx_inst[ii2].y = g_vfx_inst[i].y;
                            g_vfx_inst[ii2].age_ms = 0;
                        }
                    }
                    g_vfx_inst[i].comp_last_spawn_ms = g_vfx_inst[i].age_ms;
                    g_vfx_inst[i].comp_next_child++;
                    continue;
                }
                break;
            }
        }
    }
    if (!g_vfx_frozen)
    {
        float dt_sec = dt * 0.001f;
        for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
        {
            if (!g_vfx_inst[i].active)
                continue;
            VfxReg* r = &g_vfx_reg[g_vfx_inst[i].reg_index];
            if (r->emit_hz > 0 && r->p_lifetime_ms > 0 && r->p_max > 0)
            {
                g_vfx_inst[i].emit_accum += r->emit_hz * dt_sec * g_vfx_perf_scale;
                int want = (int) g_vfx_inst[i].emit_accum;
                if (want > 0)
                {
                    g_vfx_inst[i].emit_accum -= (float) want;
                    int cur = 0;
                    for (int p = 0; p < ROGUE_VFX_PART_CAP; ++p)
                        if (g_vfx_parts[p].active && g_vfx_parts[p].inst_idx == (uint16_t) i &&
                            !g_vfx_parts[p].is_trail)
                            ++cur;
                    int can = r->p_max - cur;
                    int to_spawn = want < can ? want : can;
                    if (g_pacing_enabled && g_pacing_threshold > 0)
                    {
                        int allowed = g_pacing_threshold - (g_vfx_stats_accum.spawned_core +
                                                            g_vfx_stats_accum.spawned_trail);
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_pacing += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    if (g_budget_soft > 0)
                    {
                        int spawned =
                            g_vfx_stats_accum.spawned_core + g_vfx_stats_accum.spawned_trail;
                        int allowed = g_budget_soft - spawned;
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_soft += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    if (g_budget_hard > 0)
                    {
                        int spawned =
                            g_vfx_stats_accum.spawned_core + g_vfx_stats_accum.spawned_trail;
                        int allowed = g_budget_hard - spawned;
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_hard += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    for (int s = 0; s < to_spawn; ++s)
                    {
                        int pi = vfx_part_alloc();
                        if (pi < 0)
                            break;
                        g_vfx_parts[pi].active = 1;
                        g_vfx_parts[pi].inst_idx = (uint16_t) i;
                        g_vfx_parts[pi].layer = r->layer;
                        g_vfx_parts[pi].world_space = r->world_space;
                        g_vfx_parts[pi].x = g_vfx_inst[i].x;
                        g_vfx_parts[pi].y = g_vfx_inst[i].y;
                        g_vfx_parts[pi].is_trail = 0;
                        float base_scale =
                            (g_vfx_inst[i].ov_scale > 0.0f) ? g_vfx_inst[i].ov_scale : 1.0f;
                        float scale_mul = 1.0f;
                        if (r->var_scale_mode == ROGUE_VFX_DIST_UNIFORM)
                        {
                            float t = rogue_fx_rand01();
                            float mn = r->var_scale_a, mx = r->var_scale_b;
                            if (mx < mn)
                            {
                                float tmp = mn;
                                mn = mx;
                                mx = tmp;
                            }
                            scale_mul = mn + (mx - mn) * t;
                        }
                        else if (r->var_scale_mode == ROGUE_VFX_DIST_NORMAL)
                        {
                            float mean = r->var_scale_a, sigma = r->var_scale_b;
                            float z = rogue_fx_rand_normal01();
                            scale_mul = mean + sigma * z;
                            if (scale_mul <= 0.01f)
                                scale_mul = 0.01f;
                        }
                        g_vfx_parts[pi].scale = base_scale * scale_mul;
                        g_vfx_parts[pi].color_rgba =
                            g_vfx_inst[i].ov_color_rgba ? g_vfx_inst[i].ov_color_rgba : 0xFFFFFFFFu;
                        g_vfx_parts[pi].age_ms = 0;
                        float life_ms = (float) r->p_lifetime_ms;
                        if (r->var_life_mode == ROGUE_VFX_DIST_UNIFORM)
                        {
                            float t = rogue_fx_rand01();
                            float mn = r->var_life_a, mx = r->var_life_b;
                            if (mx < mn)
                            {
                                float tmp = mn;
                                mn = mx;
                                mx = tmp;
                            }
                            float mul = mn + (mx - mn) * t;
                            if (mul <= 0.01f)
                                mul = 0.01f;
                            life_ms = life_ms * mul;
                        }
                        else if (r->var_life_mode == ROGUE_VFX_DIST_NORMAL)
                        {
                            float mean = r->var_life_a, sigma = r->var_life_b;
                            float mul = mean + sigma * rogue_fx_rand_normal01();
                            if (mul <= 0.01f)
                                mul = 0.01f;
                            life_ms = life_ms * mul;
                        }
                        if (life_ms < 1.0f)
                            life_ms = 1.0f;
                        g_vfx_parts[pi].lifetime_ms = (uint32_t) life_ms;
                        g_vfx_stats_accum.spawned_core++;
                    }
                }
            }
            if (r->trail_hz > 0 && r->trail_life_ms > 0 && r->trail_max > 0)
            {
                g_vfx_inst[i].trail_accum += r->trail_hz * dt_sec * g_vfx_perf_scale;
                int want = (int) g_vfx_inst[i].trail_accum;
                if (want > 0)
                {
                    g_vfx_inst[i].trail_accum -= (float) want;
                    int cur = 0;
                    for (int p = 0; p < ROGUE_VFX_PART_CAP; ++p)
                        if (g_vfx_parts[p].active && g_vfx_parts[p].inst_idx == (uint16_t) i &&
                            g_vfx_parts[p].is_trail)
                            ++cur;
                    int can = r->trail_max - cur;
                    int to_spawn = want < can ? want : can;
                    if (g_pacing_enabled && g_pacing_threshold > 0)
                    {
                        int allowed = g_pacing_threshold - (g_vfx_stats_accum.spawned_core +
                                                            g_vfx_stats_accum.spawned_trail);
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_pacing += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    if (g_budget_soft > 0)
                    {
                        int spawned =
                            g_vfx_stats_accum.spawned_core + g_vfx_stats_accum.spawned_trail;
                        int allowed = g_budget_soft - spawned;
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_soft += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    if (g_budget_hard > 0)
                    {
                        int spawned =
                            g_vfx_stats_accum.spawned_core + g_vfx_stats_accum.spawned_trail;
                        int allowed = g_budget_hard - spawned;
                        if (allowed < 0)
                            allowed = 0;
                        if (to_spawn > allowed)
                        {
                            g_vfx_stats_accum.culled_hard += (to_spawn - allowed);
                            to_spawn = allowed;
                        }
                    }
                    for (int s = 0; s < to_spawn; ++s)
                    {
                        int pi = vfx_part_alloc();
                        if (pi < 0)
                            break;
                        g_vfx_parts[pi].active = 1;
                        g_vfx_parts[pi].inst_idx = (uint16_t) i;
                        g_vfx_parts[pi].layer = r->layer;
                        g_vfx_parts[pi].world_space = r->world_space;
                        g_vfx_parts[pi].x = g_vfx_inst[i].x;
                        g_vfx_parts[pi].y = g_vfx_inst[i].y;
                        g_vfx_parts[pi].is_trail = 1;
                        g_vfx_parts[pi].scale =
                            (g_vfx_inst[i].ov_scale > 0.0f) ? g_vfx_inst[i].ov_scale : 1.0f;
                        g_vfx_parts[pi].color_rgba =
                            g_vfx_inst[i].ov_color_rgba ? g_vfx_inst[i].ov_color_rgba : 0xFFFFFFFFu;
                        g_vfx_parts[pi].age_ms = 0;
                        g_vfx_parts[pi].lifetime_ms = r->trail_life_ms;
                        g_vfx_stats_accum.spawned_trail++;
                    }
                }
            }
        }
        vfx_particles_update(dt);
    }
    for (int i = 0; i < 8; ++i)
    {
        if (!g_shakes[i].active)
            continue;
        g_shakes[i].age_ms += (uint32_t) dt;
        if (g_shakes[i].age_ms >= g_shakes[i].dur_ms)
            g_shakes[i].active = 0;
    }
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP; ++i)
    {
        if (!g_decal_inst[i].active)
            continue;
        g_decal_inst[i].age_ms += (uint32_t) dt;
        DecalReg* r = &g_decal_reg[g_decal_inst[i].reg_index];
        if (g_decal_inst[i].age_ms > r->lifetime_ms)
            g_decal_inst[i].active = 0;
    }
    g_vfx_stats_accum.active_particles = rogue_vfx_particles_active_count();
    g_vfx_stats_accum.active_instances = 0;
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
        if (g_vfx_inst[i].active)
            g_vfx_stats_accum.active_instances++;
    int dact = 0;
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP; ++i)
        if (g_decal_inst[i].active)
            ++dact;
    g_vfx_stats_accum.active_decals = dact;
    g_vfx_stats_last = g_vfx_stats_accum;
}

void rogue_vfx_set_timescale(float s) { g_vfx_timescale = s; }
void rogue_vfx_set_frozen(int frozen) { g_vfx_frozen = frozen ? 1 : 0; }
int rogue_vfx_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
        if (g_vfx_inst[i].active)
            ++c;
    return c;
}
int rogue_vfx_layer_active_count(RogueVfxLayer layer)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
    {
        if (!g_vfx_inst[i].active)
            continue;
        if (g_vfx_reg[g_vfx_inst[i].reg_index].layer == (uint8_t) layer)
            ++c;
    }
    return c;
}
void rogue_vfx_clear_active(void)
{
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
        g_vfx_inst[i].active = 0;
}
int rogue_vfx_debug_peek_first(const char* id, int* out_world_space, float* out_x, float* out_y)
{
    int ridx = vfx_reg_find(id);
    if (ridx < 0)
        return -1;
    for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
    {
        if (g_vfx_inst[i].active && g_vfx_inst[i].reg_index == (uint16_t) ridx)
        {
            if (out_world_space)
                *out_world_space = g_vfx_reg[ridx].world_space;
            if (out_x)
                *out_x = g_vfx_inst[i].x;
            if (out_y)
                *out_y = g_vfx_inst[i].y;
            return 0;
        }
    }
    return -2;
}
int rogue_vfx_spawn_by_id(const char* id, float x, float y)
{
    int ridx = vfx_reg_find(id);
    if (ridx < 0)
        return -1;
    int ii = vfx_inst_alloc();
    if (ii < 0)
        return -2;
    g_vfx_inst[ii].reg_index = (uint16_t) ridx;
    g_vfx_inst[ii].x = x;
    g_vfx_inst[ii].y = y;
    g_vfx_inst[ii].age_ms = 0;
    return 0;
}
int rogue_vfx_spawn_with_overrides(const char* id, float x, float y, const RogueVfxOverrides* ov)
{
    int ridx = vfx_reg_find(id);
    if (ridx < 0)
        return -1;
    int ii = vfx_inst_alloc();
    if (ii < 0)
        return -2;
    g_vfx_inst[ii].reg_index = (uint16_t) ridx;
    g_vfx_inst[ii].x = x;
    g_vfx_inst[ii].y = y;
    g_vfx_inst[ii].age_ms = 0;
    if (ov)
    {
        g_vfx_inst[ii].ov_lifetime_ms = ov->lifetime_ms;
        g_vfx_inst[ii].ov_scale = ov->scale;
        g_vfx_inst[ii].ov_color_rgba = ov->color_rgba;
    }
    return 0;
}
/* bus -> vfx spawn */
void rogue_vfx_dispatch_spawn_event(const RogueEffectEvent* e)
{
    if (!e)
        return;
    (void) rogue_vfx_spawn_by_id(e->id, e->x, e->y);
}

int rogue_vfx_shake_add(float amplitude, float frequency_hz, uint32_t duration_ms)
{
    if (amplitude <= 0.0f || frequency_hz <= 0.0f || duration_ms == 0)
        return -1;
    for (int i = 0; i < 8; ++i)
    {
        if (!g_shakes[i].active)
        {
            g_shakes[i].active = 1;
            g_shakes[i].amp = amplitude;
            g_shakes[i].freq_hz = frequency_hz;
            g_shakes[i].dur_ms = duration_ms;
            g_shakes[i].age_ms = 0;
            return i;
        }
    }
    return -2;
}
void rogue_vfx_shake_clear(void)
{
    for (int i = 0; i < 8; ++i)
        g_shakes[i].active = 0;
}
void rogue_vfx_shake_get_offset(float* out_x, float* out_y)
{
    float ox = 0.0f, oy = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        if (!g_shakes[i].active)
            continue;
        float t = g_shakes[i].age_ms * 0.001f;
        float phase = t * g_shakes[i].freq_hz * 6.2831853f;
        float fade = 1.0f - (float) g_shakes[i].age_ms / (float) g_shakes[i].dur_ms;
        if (fade < 0.0f)
            fade = 0.0f;
        ox += g_shakes[i].amp * fade * sinf(phase);
        oy += g_shakes[i].amp * fade * cosf(phase * 0.7f);
    }
    if (out_x)
        *out_x = ox;
    if (out_y)
        *out_y = oy;
}

void rogue_vfx_set_perf_scale(float s)
{
    if (s < 0.0f)
        s = 0.0f;
    if (s > 1.0f)
        s = 1.0f;
    g_vfx_perf_scale = s;
}
float rogue_vfx_get_perf_scale(void) { return g_vfx_perf_scale; }
void rogue_vfx_set_gpu_batch_enabled(int enable) { g_vfx_gpu_batch = enable ? 1 : 0; }
int rogue_vfx_get_gpu_batch_enabled(void) { return g_vfx_gpu_batch; }

int rogue_vfx_registry_define_composite(const char* id, RogueVfxLayer layer, uint32_t lifetime_ms,
                                        int world_space, const char** child_ids,
                                        const uint32_t* delays_ms, int child_count, int chain_mode)
{
    if (!id || !*id || child_count < 0)
        return -1;
    if (child_count > 8)
        child_count = 8;
    int rc = rogue_vfx_registry_register(id, layer, lifetime_ms, world_space);
    if (rc != 0)
        return rc;
    int idx = vfx_reg_find(id);
    if (idx < 0)
        return -2;
    VfxReg* r = &g_vfx_reg[idx];
    r->comp_mode = chain_mode ? 1 : 2;
    r->comp_child_count = (uint8_t) child_count;
    for (int i = 0; i < child_count; ++i)
    {
        r->comp_child_delays[i] = delays_ms ? delays_ms[i] : 0u;
        r->comp_child_indices[i] = 0xFFFFu;
        if (child_ids && child_ids[i])
        {
            int cidx = vfx_reg_find(child_ids[i]);
            if (cidx >= 0)
                r->comp_child_indices[i] = (uint16_t) cidx;
        }
    }
    return 0;
}

int rogue_vfx_particles_collect_scales(float* out_scales, int max)
{
    if (!out_scales || max <= 0)
        return 0;
    int w = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && w < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_scales[w++] = g_vfx_parts[i].scale;
    }
    return w;
}
int rogue_vfx_particles_collect_colors(uint32_t* out_rgba, int max)
{
    if (!out_rgba || max <= 0)
        return 0;
    int w = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && w < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_rgba[w++] = g_vfx_parts[i].color_rgba;
    }
    return w;
}
int rogue_vfx_particles_collect_lifetimes(uint32_t* out_ms, int max)
{
    if (!out_ms || max <= 0)
        return 0;
    int w = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && w < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_ms[w++] = g_vfx_parts[i].lifetime_ms;
    }
    return w;
}

void rogue_vfx_profiler_get_last(RogueVfxFrameStats* out)
{
    if (!out)
        return;
    out->spawned_core = g_vfx_stats_last.spawned_core;
    out->spawned_trail = g_vfx_stats_last.spawned_trail;
    out->culled_soft = g_vfx_stats_last.culled_soft;
    out->culled_hard = g_vfx_stats_last.culled_hard;
    out->culled_pacing = g_vfx_stats_last.culled_pacing;
    out->active_particles = g_vfx_stats_last.active_particles;
    out->active_instances = g_vfx_stats_last.active_instances;
    out->active_decals = g_vfx_stats_last.active_decals;
}
void rogue_vfx_set_spawn_budgets(int soft_cap_per_frame, int hard_cap_per_frame)
{
    g_budget_soft = soft_cap_per_frame;
    g_budget_hard = hard_cap_per_frame;
}
void rogue_vfx_set_pacing_guard(int enable, int threshold_per_frame)
{
    g_pacing_enabled = enable ? 1 : 0;
    g_pacing_threshold = threshold_per_frame;
}
static void audit_pool_generic(int total_slots, int (*is_active)(int), int* out_active,
                               int* out_free, int* out_free_runs, int* out_max_free_run)
{
    int active = 0, freec = 0, runs = 0, maxrun = 0, run = 0;
    for (int i = 0; i < total_slots; ++i)
    {
        int a = is_active(i);
        if (a)
        {
            active++;
            if (run > 0)
            {
                runs++;
                if (run > maxrun)
                    maxrun = run;
                run = 0;
            }
        }
        else
        {
            freec++;
            run++;
        }
    }
    if (run > 0)
    {
        runs++;
        if (run > maxrun)
            maxrun = run;
    }
    if (out_active)
        *out_active = active;
    if (out_free)
        *out_free = freec;
    if (out_free_runs)
        *out_free_runs = runs;
    if (out_max_free_run)
        *out_max_free_run = maxrun;
}
static int particles_is_active(int idx) { return g_vfx_parts[idx].active ? 1 : 0; }
static int instances_is_active(int idx) { return g_vfx_inst[idx].active ? 1 : 0; }
void rogue_vfx_particle_pool_audit(int* out_active, int* out_free, int* out_free_runs,
                                   int* out_max_free_run)
{
    audit_pool_generic(ROGUE_VFX_PART_CAP, particles_is_active, out_active, out_free, out_free_runs,
                       out_max_free_run);
}
void rogue_vfx_instance_pool_audit(int* out_active, int* out_free, int* out_free_runs,
                                   int* out_max_free_run)
{
    audit_pool_generic(ROGUE_VFX_INST_CAP, instances_is_active, out_active, out_free, out_free_runs,
                       out_max_free_run);
}

static int decal_reg_find(const char* id)
{
    for (int i = 0; i < g_decal_reg_count; ++i)
        if (strncmp(g_decal_reg[i].id, id, sizeof g_decal_reg[i].id) == 0)
            return i;
    return -1;
}
int rogue_vfx_decal_registry_register(const char* id, RogueVfxLayer layer, uint32_t lifetime_ms,
                                      int world_space, float size)
{
    if (!id || !*id)
        return -1;
    int idx = decal_reg_find(id);
    if (idx < 0)
    {
        if (g_decal_reg_count >= ROGUE_VFX_DECAL_REG_CAP)
            return -2;
        idx = g_decal_reg_count++;
        memset(&g_decal_reg[idx], 0, sizeof g_decal_reg[idx]);
#if defined(_MSC_VER)
        strncpy_s(g_decal_reg[idx].id, sizeof g_decal_reg[idx].id, id, _TRUNCATE);
#else
        strncpy(g_decal_reg[idx].id, id, sizeof g_decal_reg[idx].id - 1);
        g_decal_reg[idx].id[sizeof g_decal_reg[idx].id - 1] = '\0';
#endif
    }
    g_decal_reg[idx].layer = (uint8_t) layer;
    g_decal_reg[idx].lifetime_ms = lifetime_ms;
    g_decal_reg[idx].world_space = world_space ? 1 : 0;
    g_decal_reg[idx].size = (size <= 0.0f) ? 1.0f : size;
    return 0;
}
int rogue_vfx_decal_registry_get(const char* id, RogueVfxLayer* out_layer,
                                 uint32_t* out_lifetime_ms, int* out_world_space, float* out_size)
{
    int idx = decal_reg_find(id);
    if (idx < 0)
        return -1;
    if (out_layer)
        *out_layer = (RogueVfxLayer) g_decal_reg[idx].layer;
    if (out_lifetime_ms)
        *out_lifetime_ms = g_decal_reg[idx].lifetime_ms;
    if (out_world_space)
        *out_world_space = g_decal_reg[idx].world_space;
    if (out_size)
        *out_size = g_decal_reg[idx].size;
    return 0;
}
void rogue_vfx_decal_registry_clear(void) { g_decal_reg_count = 0; }
static int decal_inst_alloc(void)
{
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP; ++i)
        if (!g_decal_inst[i].active)
            return i;
    return -1;
}
int rogue_vfx_decal_spawn(const char* id, float x, float y, float angle_rad, float scale)
{
    int ridx = decal_reg_find(id);
    if (ridx < 0)
        return -1;
    int ii = decal_inst_alloc();
    if (ii < 0)
        return -2;
    g_decal_inst[ii].active = 1;
    g_decal_inst[ii].reg_index = (uint16_t) ridx;
    g_decal_inst[ii].x = x;
    g_decal_inst[ii].y = y;
    g_decal_inst[ii].angle = angle_rad;
    g_decal_inst[ii].scale = (scale <= 0.0f) ? 1.0f : scale;
    g_decal_inst[ii].age_ms = 0;
    return 0;
}
int rogue_vfx_decal_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP; ++i)
        if (g_decal_inst[i].active)
            ++c;
    return c;
}
int rogue_vfx_decal_layer_count(RogueVfxLayer layer)
{
    int c = 0;
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP; ++i)
        if (g_decal_inst[i].active &&
            g_decal_reg[g_decal_inst[i].reg_index].layer == (uint8_t) layer)
            ++c;
    return c;
}
int rogue_vfx_decals_collect_screen(float* out_xy, uint8_t* out_layers, int max)
{
    if (!out_xy || max <= 0)
        return 0;
    int w = 0;
    for (int i = 0; i < ROGUE_VFX_DECAL_INST_CAP && w < max; ++i)
    {
        if (!g_decal_inst[i].active)
            continue;
        DecalReg* r = &g_decal_reg[g_decal_inst[i].reg_index];
        float sx = g_decal_inst[i].x, sy = g_decal_inst[i].y;
        if (r->world_space)
        {
            sx = (sx - g_cam_x) * g_pixels_per_world;
            sy = (sy - g_cam_y) * g_pixels_per_world;
        }
        out_xy[w * 2 + 0] = sx;
        out_xy[w * 2 + 1] = sy;
        if (out_layers)
            out_layers[w] = r->layer;
        ++w;
    }
    return w;
}

/* Post-processing stubs */
static int g_bloom_enabled = 0;
static float g_bloom_threshold = 1.0f;
static float g_bloom_intensity = 0.5f;
static char g_lut_id[24];
static float g_lut_strength = 0.0f;
void rogue_vfx_post_set_bloom_enabled(int enable) { g_bloom_enabled = enable ? 1 : 0; }
int rogue_vfx_post_get_bloom_enabled(void) { return g_bloom_enabled; }
void rogue_vfx_post_set_bloom_params(float threshold, float intensity)
{
    if (threshold < 0.0f)
        threshold = 0.0f;
    if (intensity < 0.0f)
        intensity = 0.0f;
    g_bloom_threshold = threshold;
    g_bloom_intensity = intensity;
}
void rogue_vfx_post_get_bloom_params(float* out_threshold, float* out_intensity)
{
    if (out_threshold)
        *out_threshold = g_bloom_threshold;
    if (out_intensity)
        *out_intensity = g_bloom_intensity;
}
void rogue_vfx_post_set_color_lut(const char* lut_id, float strength)
{
    if (!lut_id || !*lut_id || strength <= 0.0f)
    {
        g_lut_id[0] = '\0';
        g_lut_strength = 0.0f;
        return;
    }
#if defined(_MSC_VER)
    strncpy_s(g_lut_id, sizeof g_lut_id, lut_id, _TRUNCATE);
#else
    strncpy(g_lut_id, lut_id, sizeof g_lut_id - 1);
    g_lut_id[sizeof g_lut_id - 1] = '\0';
#endif
    if (strength > 1.0f)
        strength = 1.0f;
    g_lut_strength = strength;
}
int rogue_vfx_post_get_color_lut(char* out_id, size_t out_sz, float* out_strength)
{
    if (out_strength)
        *out_strength = g_lut_strength;
    if (g_lut_strength <= 0.0f)
    {
        if (out_id && out_sz)
        {
            if (out_sz > 0)
                out_id[0] = '\0';
        }
        return 0;
    }
    if (out_id && out_sz)
    {
#if defined(_MSC_VER)
        strncpy_s(out_id, out_sz, g_lut_id, _TRUNCATE);
#else
        strncpy(out_id, g_lut_id, out_sz - 1);
        out_id[out_sz - 1] = '\0';
#endif
    }
    return 1;
}

void rogue_vfx_profiler_get_last(
    RogueVfxFrameStats* out); /* already defined above; keep linker happy */
