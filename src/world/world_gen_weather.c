/* Phase 10: Weather & Environmental Simulation Implementation */
#include "world/world_gen.h"
#include <string.h>

#ifndef ROGUE_MAX_WEATHER_PATTERNS
#define ROGUE_MAX_WEATHER_PATTERNS 32
#endif

typedef struct RogueWeatherRegistryEntry
{
    RogueWeatherPatternDesc desc;
} RogueWeatherRegistryEntry;

static RogueWeatherRegistryEntry g_weather_registry[ROGUE_MAX_WEATHER_PATTERNS];
static int g_weather_registry_count = 0;

static void safe_copy(char* dst, size_t cap, const char* src)
{
    if (cap == 0)
        return;
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

int rogue_weather_register(const RogueWeatherPatternDesc* d)
{
    if (!d || g_weather_registry_count >= ROGUE_MAX_WEATHER_PATTERNS)
        return -1;
    g_weather_registry[g_weather_registry_count].desc = *d; /* struct copy */
    /* ensure id is null terminated */
    g_weather_registry[g_weather_registry_count].desc.id[sizeof(d->id) - 1] = '\0';
    return g_weather_registry_count++;
}

void rogue_weather_clear_registry(void) { g_weather_registry_count = 0; }
int rogue_weather_registry_count(void) { return g_weather_registry_count; }

static unsigned int rng_u32(RogueRngChannel* ch) { return rogue_worldgen_rand_u32(ch); }
static float rng_norm(RogueRngChannel* ch) { return (float) rogue_worldgen_rand_norm(ch); }

/* Select a weather pattern weighted by (base_weight * biome_mask inclusion) */
static int select_pattern(RogueWorldGenContext* ctx, int biome_id)
{
    if (g_weather_registry_count == 0)
        return -1;
    double total = 0.0;
    double weights[ROGUE_MAX_WEATHER_PATTERNS];
    for (int i = 0; i < g_weather_registry_count; i++)
    {
        const RogueWeatherPatternDesc* d = &g_weather_registry[i].desc;
        int allowed = (d->biome_mask & (1u << biome_id)) != 0;
        if (!allowed)
        {
            weights[i] = 0.0;
            continue;
        }
        double w = d->base_weight;
        if (w < 0)
            w = 0;
        weights[i] = w;
        total += w;
    }
    if (total == 0.0)
        return -1;
    double r = (double) rng_norm(&ctx->macro_rng); /* macro channel for coarse scheduling */
    if (r < 0)
        r = 0;
    if (r > 1)
        r = 1; /* guard */
    double acc = 0.0;
    for (int i = 0; i < g_weather_registry_count; i++)
    {
        acc += weights[i] / total;
        if (r <= acc)
            return i;
    }
    return g_weather_registry_count - 1;
}

int rogue_weather_init(RogueWorldGenContext* ctx, RogueActiveWeather* state)
{
    if (!ctx || !state)
        return 0;
    state->pattern_index = -1;
    state->remaining_ticks = 0;
    state->intensity = 0;
    state->target_intensity = 0;
    return 1;
}

int rogue_weather_update(RogueWorldGenContext* ctx, RogueActiveWeather* state, int ticks,
                         int biome_id)
{
    if (!ctx || !state)
        return -1;
    if (ticks <= 0)
        ticks = 1;
    int changed = -1;
    while (ticks > 0)
    {
        if (state->remaining_ticks <= 0 || state->pattern_index < 0)
        {
            int p = select_pattern(ctx, biome_id);
            if (p < 0)
            {
                state->pattern_index = -1;
                state->remaining_ticks = 0;
                state->intensity = 0;
                state->target_intensity = 0;
                return changed;
            }
            const RogueWeatherPatternDesc* d = &g_weather_registry[p].desc;
            int span = d->max_duration_ticks - d->min_duration_ticks + 1;
            if (span < 1)
                span = 1;
            int dur =
                d->min_duration_ticks + (int) (rng_u32(&ctx->macro_rng) % (unsigned int) span);
            if (dur < 1)
                dur = 1;
            state->pattern_index = p;
            state->remaining_ticks = dur;
            state->intensity = 0.0f; /* ramp from 0 */
            float irange = (d->intensity_max - d->intensity_min);
            if (irange < 0)
                irange = 0;
            state->target_intensity = d->intensity_min + rng_norm(&ctx->micro_rng) * irange;
            changed = p;
        }
        /* advance one tick */
        state->remaining_ticks -= 1;
        ticks -= 1;
        /* easing toward target */
        float delta = state->target_intensity - state->intensity;
        state->intensity += delta * 0.05f; /* 5% per tick */
        if (state->remaining_ticks == 0)
        {
            state->target_intensity = 0.0f; /* fade out */
        }
        if (state->remaining_ticks <= 0 && state->intensity < 0.01f)
        { /* will select new next loop */
        }
    }
    return changed;
}

void rogue_weather_sample_lighting(const RogueActiveWeather* state, unsigned char* r,
                                   unsigned char* g, unsigned char* b)
{
    if (!state || !r || !g || !b)
    {
        return;
    }
    float base = 160.0f;
    float factor = 1.0f - 0.3f * state->intensity;
    if (factor < 0.5f)
        factor = 0.5f;
    unsigned char val = (unsigned char) (base * factor);
    *r = val;
    *g = val;
    *b = (unsigned char) (val + (unsigned char) (20 * state->intensity));
}

float rogue_weather_movement_factor(const RogueActiveWeather* state)
{
    if (!state)
        return 1.0f;
    float slow = 1.0f - 0.25f * state->intensity;
    if (slow < 0.5f)
        slow = 0.5f;
    return slow;
}
