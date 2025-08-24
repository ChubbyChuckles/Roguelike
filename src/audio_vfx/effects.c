#include "effects.h"
#include "../game/combat.h" /* for RogueDamageEvent & observer API */
#include "../util/log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

/* -------- Event bus (double buffer minimal) -------- */

#define ROGUE_FX_MAX_EVENTS 256

typedef struct FxQueue
{
    RogueEffectEvent ev[ROGUE_FX_MAX_EVENTS];
    int count;
} FxQueue;

static FxQueue g_fx_a, g_fx_b;
static FxQueue* g_write = &g_fx_a;
static FxQueue* g_read = &g_fx_b;
static uint32_t g_frame_index = 0;
static uint32_t g_seq_counter = 0;
static uint32_t g_frame_digest = 0;
static uint32_t g_fx_seed = 0xA5F0C3D2u; /* deterministic tiny RNG for FX */
/* Damage hook observer id (-1 if not bound) */
static int g_damage_observer_id = -1;
/* Gameplay->effects mapping (Phase 5.1) */
typedef struct FxMapEntry
{
    char key[32];
    uint8_t type; /* RogueFxMapType */
    char effect_id[24];
    uint8_t priority; /* RogueEffectPriority */
} FxMapEntry;
static FxMapEntry g_fx_map[96];
static int g_fx_map_count = 0;

static uint32_t rotl32(uint32_t x, int r) { return (x << r) | (x >> (32 - r)); }
static void digest_mix_u32(uint32_t v)
{
    g_frame_digest ^= rotl32(v * 0x9E3779B9u + 0x85EBCA6Bu, 13);
    g_frame_digest *= 0xC2B2AE35u;
}
static void digest_mix_bytes(const void* data, size_t n)
{
    const unsigned char* p = (const unsigned char*) data;
    for (size_t i = 0; i < n; ++i)
        digest_mix_u32((uint32_t) p[i] + 0x100u * (uint32_t) (i & 0xFFu));
}

void rogue_fx_frame_begin(uint32_t frame_index)
{
    g_frame_index = frame_index;
    g_seq_counter = 0;
    g_frame_digest = 0x1234ABCDu ^ frame_index;
    g_write->count = 0;
}

void rogue_fx_frame_end(void)
{
    /* swap buffers */
    FxQueue* tmp = g_read;
    g_read = g_write;
    g_write = tmp;
}

int rogue_fx_emit(const RogueEffectEvent* ev)
{
    if (!ev || g_write->count >= ROGUE_FX_MAX_EVENTS)
        return -1;
    RogueEffectEvent* out = &g_write->ev[g_write->count++];
    *out = *ev;
    out->emit_frame = g_frame_index;
    out->seq = g_seq_counter++;
    return 0;
}

int rogue_fx_map_register(const char* gameplay_event_key, RogueFxMapType type,
                          const char* effect_id, RogueEffectPriority priority)
{
    if (!gameplay_event_key || !*gameplay_event_key || !effect_id || !*effect_id)
        return -1;
    if (g_fx_map_count >= (int) (sizeof g_fx_map / sizeof g_fx_map[0]))
        return -2;
    FxMapEntry* e = &g_fx_map[g_fx_map_count++];
    memset(e, 0, sizeof *e);
#if defined(_MSC_VER)
    strncpy_s(e->key, sizeof e->key, gameplay_event_key, _TRUNCATE);
    strncpy_s(e->effect_id, sizeof e->effect_id, effect_id, _TRUNCATE);
#else
    strncpy(e->key, gameplay_event_key, sizeof e->key - 1);
    e->key[sizeof e->key - 1] = '\0';
    strncpy(e->effect_id, effect_id, sizeof e->effect_id - 1);
    e->effect_id[sizeof e->effect_id - 1] = '\0';
#endif
    e->type = (uint8_t) type;
    e->priority = (uint8_t) priority;
    return 0;
}

void rogue_fx_map_clear(void) { g_fx_map_count = 0; }

int rogue_fx_trigger_event(const char* gameplay_event_key, float x, float y)
{
    if (!gameplay_event_key || !*gameplay_event_key)
        return 0;
    int emitted = 0;
    for (int i = 0; i < g_fx_map_count; ++i)
    {
        if (strncmp(g_fx_map[i].key, gameplay_event_key, sizeof g_fx_map[i].key) != 0)
            continue;
        RogueEffectEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.priority = g_fx_map[i].priority;
        ev.repeats = 1;
#if defined(_MSC_VER)
        strncpy_s(ev.id, sizeof ev.id, g_fx_map[i].effect_id, _TRUNCATE);
#else
        strncpy(ev.id, g_fx_map[i].effect_id, sizeof ev.id - 1);
        ev.id[sizeof ev.id - 1] = '\0';
#endif
        ev.x = x;
        ev.y = y;
        if (g_fx_map[i].type == ROGUE_FX_MAP_AUDIO)
            ev.type = ROGUE_FX_AUDIO_PLAY;
        else if (g_fx_map[i].type == ROGUE_FX_MAP_VFX)
            ev.type = ROGUE_FX_VFX_SPAWN;
        else
            continue;
        if (rogue_fx_emit(&ev) == 0)
            ++emitted;
    }
    return emitted;
}

/* ---- Phase 5.2: Damage event observer hook ---- */
static const char* dmg_type_to_key(unsigned char t)
{
    switch (t)
    {
    case ROGUE_DMG_PHYSICAL:
        return "physical";
    case ROGUE_DMG_BLEED:
        return "bleed";
    case ROGUE_DMG_FIRE:
        return "fire";
    case ROGUE_DMG_FROST:
        return "frost";
    case ROGUE_DMG_ARCANE:
        return "arcane";
    case ROGUE_DMG_POISON:
        return "poison";
    case ROGUE_DMG_TRUE:
        return "true";
    default:
        return "unknown";
    }
}

static void fx_on_damage_event(const RogueDamageEvent* ev, void* user)
{
    (void) user;
    if (!ev)
        return;
    char key[64];
    const char* type_key = dmg_type_to_key(ev->damage_type);
    /* Always emit hit */
#if defined(_MSC_VER)
    _snprintf_s(key, sizeof key, _TRUNCATE, "damage/%s/hit", type_key);
#else
    snprintf(key, sizeof key, "damage/%s/hit", type_key);
#endif
    rogue_fx_trigger_event(key, 0.0f, 0.0f);
    if (ev->crit)
    {
#if defined(_MSC_VER)
        _snprintf_s(key, sizeof key, _TRUNCATE, "damage/%s/crit", type_key);
#else
        snprintf(key, sizeof key, "damage/%s/crit", type_key);
#endif
        rogue_fx_trigger_event(key, 0.0f, 0.0f);
    }
    if (ev->execution)
    {
#if defined(_MSC_VER)
        _snprintf_s(key, sizeof key, _TRUNCATE, "damage/%s/execution", type_key);
#else
        snprintf(key, sizeof key, "damage/%s/execution", type_key);
#endif
        rogue_fx_trigger_event(key, 0.0f, 0.0f);
    }
}

int rogue_fx_damage_hook_bind(void)
{
    if (g_damage_observer_id >= 0)
        return 1; /* already bound */
    g_damage_observer_id = rogue_combat_add_damage_observer(fx_on_damage_event, NULL);
    return (g_damage_observer_id >= 0) ? 1 : 0;
}

void rogue_fx_damage_hook_unbind(void)
{
    if (g_damage_observer_id >= 0)
    {
        rogue_combat_remove_damage_observer(g_damage_observer_id);
        g_damage_observer_id = -1;
    }
}

/* Simple stable sort by (priority, seq) since emit_frame is same for the frame */
static int cmp_ev(const void* a, const void* b)
{
    const RogueEffectEvent* ea = (const RogueEffectEvent*) a;
    const RogueEffectEvent* eb = (const RogueEffectEvent*) b;
    if (ea->priority != eb->priority)
        return (int) ea->priority - (int) eb->priority;
    /* Group by type and id to place identical keys near each other */
    if ((int) ea->type != (int) eb->type)
        return (int) ea->type - (int) eb->type;
    int k = strncmp(ea->id, eb->id, sizeof ea->id);
    if (k != 0)
        return k;
    /* Stable by seq as tiebreaker for deterministic ordering */
    if (ea->seq < eb->seq)
        return -1;
    if (ea->seq > eb->seq)
        return 1;
    return 0;
}

/* -------- Audio registry (minimal) -------- */

typedef struct AudioReg
{
    char id[24];
    char path[128];
    uint8_t cat;
    float base_gain;
#ifdef ROGUE_HAVE_SDL_MIXER
    Mix_Chunk* chunk;
#endif
} AudioReg;

#define ROGUE_AUDIO_REG_CAP 64
static AudioReg g_audio_reg[ROGUE_AUDIO_REG_CAP];
static int g_audio_reg_count = 0;

/* Channel mixer state */
static float g_mixer_master = 1.0f;
static float g_mixer_cat[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static int g_mixer_mute = 0;

/* Positional attenuation */
static int g_positional_enabled = 0;
static float g_listener_x = 0.0f, g_listener_y = 0.0f;
static float g_falloff_radius = 10.0f; /* units */

#ifdef ROGUE_HAVE_SDL_MIXER
/* Very simple global voice cap */
static int g_voice_cap = 16;
#endif

static int audio_reg_find(const char* id)
{
    for (int i = 0; i < g_audio_reg_count; ++i)
    {
        if (strncmp(g_audio_reg[i].id, id, sizeof g_audio_reg[i].id) == 0)
            return i;
    }
    return -1;
}

int rogue_audio_registry_register(const char* id, const char* path, RogueAudioCategory cat,
                                  float base_gain)
{
    if (!id || !*id || !path || !*path)
        return -2;
    int idx = audio_reg_find(id);
    if (idx < 0)
    {
        if (g_audio_reg_count >= ROGUE_AUDIO_REG_CAP)
            return -3;
        idx = g_audio_reg_count++;
        memset(&g_audio_reg[idx], 0, sizeof g_audio_reg[idx]);
#if defined(_MSC_VER)
        strncpy_s(g_audio_reg[idx].id, sizeof g_audio_reg[idx].id, id, _TRUNCATE);
#else
        strncpy(g_audio_reg[idx].id, id, sizeof g_audio_reg[idx].id - 1);
        g_audio_reg[idx].id[sizeof g_audio_reg[idx].id - 1] = '\0';
#endif
    }
#if defined(_MSC_VER)
    strncpy_s(g_audio_reg[idx].path, sizeof g_audio_reg[idx].path, path, _TRUNCATE);
#else
    strncpy(g_audio_reg[idx].path, path, sizeof g_audio_reg[idx].path - 1);
    g_audio_reg[idx].path[sizeof g_audio_reg[idx].path - 1] = '\0';
#endif
    g_audio_reg[idx].cat = (uint8_t) cat;
    g_audio_reg[idx].base_gain = (base_gain < 0 ? 0 : (base_gain > 1 ? 1 : base_gain));
/* Lazy load on first playback (SDL_mixer only) */
#ifdef ROGUE_HAVE_SDL_MIXER
    if (g_audio_reg[idx].chunk)
    {
        Mix_FreeChunk(g_audio_reg[idx].chunk);
        g_audio_reg[idx].chunk = NULL;
    }
#endif
    return 0;
}

void rogue_audio_play_by_id(const char* id)
{
    int idx = audio_reg_find(id);
    if (idx < 0)
    {
        ROGUE_LOG_WARN("Audio id not found: %s", id ? id : "<null>");
        return;
    }
#ifdef ROGUE_HAVE_SDL_MIXER
    if (g_audio_reg[idx].chunk)
    {
        if (g_voice_cap > 0 && Mix_Playing(-1) >= g_voice_cap)
            return;
        /* Volume is expected to be set by caller (dispatcher) using mixer/atten */
        Mix_PlayChannel(-1, g_audio_reg[idx].chunk, 0);
    }
    else
    {
        /* Lazy load */
        g_audio_reg[idx].chunk = Mix_LoadWAV(g_audio_reg[idx].path);
        if (!g_audio_reg[idx].chunk)
        {
            ROGUE_LOG_WARN("Audio lazy load failed id=%s path=%s err=%s", id, g_audio_reg[idx].path,
                           Mix_GetError());
            return;
        }
        if (g_voice_cap > 0 && Mix_Playing(-1) >= g_voice_cap)
            return;
        Mix_PlayChannel(-1, g_audio_reg[idx].chunk, 0);
    }
#else
    (void) idx;
#endif
}

int rogue_audio_registry_get_path(const char* id, char* out, size_t out_sz)
{
    int idx = audio_reg_find(id);
    if (idx < 0 || !out || out_sz == 0)
        return -1;
    size_t n = strlen(g_audio_reg[idx].path);
    if (n + 1 > out_sz)
        n = out_sz - 1;
    memcpy(out, g_audio_reg[idx].path, n);
    out[n] = '\0';
    return 0;
}

void rogue_audio_registry_clear(void)
{
#ifdef ROGUE_HAVE_SDL_MIXER
    for (int i = 0; i < g_audio_reg_count; ++i)
    {
        if (g_audio_reg[i].chunk)
        {
            Mix_FreeChunk(g_audio_reg[i].chunk);
            g_audio_reg[i].chunk = NULL;
        }
    }
#endif
    g_audio_reg_count = 0;
}

void rogue_audio_mixer_set_master(float gain)
{
    g_mixer_master = gain < 0 ? 0 : (gain > 1 ? 1 : gain);
}
float rogue_audio_mixer_get_master(void) { return g_mixer_master; }
void rogue_audio_mixer_set_category(RogueAudioCategory cat, float gain)
{
    if ((int) cat >= 0 && (int) cat < 4)
        g_mixer_cat[cat] = gain < 0 ? 0 : (gain > 1 ? 1 : gain);
}
float rogue_audio_mixer_get_category(RogueAudioCategory cat)
{
    return ((int) cat >= 0 && (int) cat < 4) ? g_mixer_cat[cat] : 1.0f;
}
void rogue_audio_mixer_set_mute(int mute) { g_mixer_mute = mute ? 1 : 0; }
int rogue_audio_mixer_get_mute(void) { return g_mixer_mute; }

void rogue_audio_set_listener(float x, float y)
{
    g_listener_x = x;
    g_listener_y = y;
}
void rogue_audio_enable_positional(int enable) { g_positional_enabled = enable ? 1 : 0; }
void rogue_audio_set_falloff_radius(float r)
{
    if (r > 0)
        g_falloff_radius = r;
}

static float compute_attenuation(float x, float y)
{
    if (!g_positional_enabled)
        return 1.0f;
    float dx = x - g_listener_x;
    float dy = y - g_listener_y;
    float d2 = dx * dx + dy * dy;
    float r2 = g_falloff_radius * g_falloff_radius;
    if (d2 >= r2)
        return 0.0f;
    float d = (float) sqrt((double) d2);
    float a = 1.0f - (d / g_falloff_radius);
    if (a < 0)
        a = 0;
    if (a > 1)
        a = 1;
    return a;
}

/* xorshift32 for deterministic mapping */
static uint32_t fx_rand_u32(void)
{
    uint32_t x = g_fx_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_fx_seed = x ? x : 0xA5F0C3D2u;
    return g_fx_seed;
}

/* Return float in [0,1). */
static float fx_rand01(void)
{
    return (fx_rand_u32() & 0xFFFFFFu) / 16777216.0f; /* 24-bit mantissa */
}

/* Box-Muller transform for standard normal; clamp to [-4,4] to avoid extreme tails. */
static float fx_rand_normal01(void)
{
    float u1 = fx_rand01();
    float u2 = fx_rand01();
    if (u1 < 1e-7f)
        u1 = 1e-7f;
    float z = (float) sqrt(-2.0f * log((double) u1)) * (float) cos(2.0 * 3.141592653589793 * u2);
    if (z < -4.0f)
        z = -4.0f;
    if (z > 4.0f)
        z = 4.0f;
    return z;
}

void rogue_fx_debug_set_seed(uint32_t seed) { g_fx_seed = (seed ? seed : 0xA5F0C3D2u); }

float rogue_audio_debug_effective_gain(const char* id, unsigned repeats, float x, float y)
{
    int idx = audio_reg_find(id);
    if (idx < 0)
        return 0.0f;
    float rep = repeats < 1 ? 1.0f : (float) repeats;
    float base = g_audio_reg[idx].base_gain * (0.7f + 0.3f * rep);
    if (base > 1.0f)
        base = 1.0f;
    float eff =
        base * g_mixer_master * g_mixer_cat[g_audio_reg[idx].cat] * compute_attenuation(x, y);
    if (g_mixer_mute)
        eff = 0.0f;
    if (eff < 0.0f)
        eff = 0.0f;
    if (eff > 1.0f)
        eff = 1.0f;
    return eff;
}

int rogue_fx_dispatch_process(void)
{
    if (g_read->count <= 0)
        return 0;
    qsort(g_read->ev, (size_t) g_read->count, sizeof g_read->ev[0], cmp_ev);

    /* Phase 1.5: frame compaction (merge identical events) */
    int write_i = 0;
    for (int i = 0; i < g_read->count;)
    {
        RogueEffectEvent merged = g_read->ev[i];
        merged.repeats = (merged.repeats == 0 ? 1 : merged.repeats);
        int j = i + 1;
        while (j < g_read->count)
        {
            RogueEffectEvent* n = &g_read->ev[j];
            if (n->type != merged.type || n->priority != merged.priority ||
                strncmp(n->id, merged.id, sizeof merged.id) != 0)
                break;
            merged.repeats += (n->repeats == 0 ? 1 : n->repeats);
            ++j;
        }
        g_read->ev[write_i++] = merged;
        i = j;
    }
    g_read->count = write_i;

    for (int i = 0; i < g_read->count; ++i)
    {
        RogueEffectEvent* e = &g_read->ev[i];
        /* digest contribution */
        digest_mix_u32((uint32_t) e->type);
        digest_mix_u32((uint32_t) e->priority);
        digest_mix_u32(e->seq);
        digest_mix_u32((uint32_t) (e->repeats == 0 ? 1 : e->repeats));
        digest_mix_bytes(e->id, sizeof e->id);
        if (e->type == ROGUE_FX_AUDIO_PLAY)
        {
            /* Determine variation target deterministically if variants exist (id with numbered
             * suffixes) */
            int base_len = (int) strnlen(e->id, sizeof e->id);
            const char* chosen = e->id;
            int var_indices[32];
            int var_count = 0;
            /* Scan registry for IDs like "<id>_N" */
            for (int ridx = 0; ridx < g_audio_reg_count &&
                               var_count < (int) (sizeof var_indices / sizeof var_indices[0]);
                 ++ridx)
            {
                if (strncmp(g_audio_reg[ridx].id, e->id, (size_t) base_len) == 0 &&
                    g_audio_reg[ridx].id[base_len] == '_')
                {
                    var_indices[var_count++] = ridx;
                }
            }
            if (var_count > 0)
            {
                uint32_t s = (g_frame_index * 2654435761u) ^ e->seq ^ fx_rand_u32();
                int pick = (int) (s % (uint32_t) var_count);
                chosen = g_audio_reg[var_indices[pick]].id;
            }
#ifdef ROGUE_HAVE_SDL_MIXER
            /* Compute and set volume */
            int ci = audio_reg_find(chosen);
            if (ci >= 0)
            {
                /* Lazy load chunk if necessary */
                if (!g_audio_reg[ci].chunk)
                    g_audio_reg[ci].chunk = Mix_LoadWAV(g_audio_reg[ci].path);
                if (g_audio_reg[ci].chunk)
                {
                    if (!(g_voice_cap > 0 && Mix_Playing(-1) >= g_voice_cap))
                    {
                        float eff = rogue_audio_debug_effective_gain(
                            chosen, (unsigned) (e->repeats ? e->repeats : 1), e->x, e->y);
                        int vol = (int) (eff * 128.0f);
                        Mix_Volume(-1, vol);
                        Mix_PlayChannel(-1, g_audio_reg[ci].chunk, 0);
                    }
                }
            }
#else
            (void) chosen;
            rogue_audio_play_by_id(e->id);
#endif
        }
        else if (e->type == ROGUE_FX_VFX_SPAWN)
        {
            (void) rogue_vfx_spawn_by_id(e->id, e->x, e->y);
        }
    }
    int processed = g_read->count;
    g_read->count = 0;
    return processed;
}

uint32_t rogue_fx_get_frame_digest(void) { return g_frame_digest; }

/* -------- VFX registry & instances (Phase 3 foundations) -------- */

typedef struct VfxReg
{
    char id[24];
    uint8_t layer;        /* RogueVfxLayer */
    uint8_t world_space;  /* 1=world, 0=screen */
    uint32_t lifetime_ms; /* default lifetime */
    /* Minimal particle emitter config */
    float emit_hz;          /* particles per second */
    uint32_t p_lifetime_ms; /* particle lifetime */
    int p_max;              /* cap per effect instance */
    /* Phase 4.5: variation distributions (multipliers) */
    uint8_t var_scale_mode; /* RogueVfxDist */
    float var_scale_a, var_scale_b;
    uint8_t var_life_mode; /* RogueVfxDist */
    float var_life_a, var_life_b;
    /* Composition (Phase 4.3) */
    uint8_t comp_mode; /* RogueVfxCompMode (0=none) */
    uint8_t comp_child_count;
    uint16_t comp_child_indices[8]; /* up to 8 children */
    uint32_t comp_child_delays[8];  /* delay per child (ms); semantics depend on mode */
} VfxReg;

typedef struct VfxInst
{
    uint16_t reg_index; /* index into registry */
    uint16_t active;    /* 1=active */
    float x, y;         /* spawn position */
    uint32_t age_ms;    /* progressed time */
    float emit_accum;   /* fractional particle accumulator */
    /* Overrides (Phase 4.4) */
    uint32_t ov_lifetime_ms; /* 0 means use registry lifetime */
    float ov_scale;          /* <=0 means default 1.0 */
    uint32_t ov_color_rgba;  /* 0 means default (0xFFFFFFFF) */
    /* Composition runtime (Phase 4.3) */
    uint8_t comp_next_child;     /* next child index to spawn */
    uint32_t comp_last_spawn_ms; /* for CHAIN mode relative delay */
} VfxInst;

#define ROGUE_VFX_REG_CAP 64
static VfxReg g_vfx_reg[ROGUE_VFX_REG_CAP];
static int g_vfx_reg_count = 0;

#define ROGUE_VFX_INST_CAP 256
static VfxInst g_vfx_inst[ROGUE_VFX_INST_CAP];

static float g_vfx_timescale = 1.0f;
static int g_vfx_frozen = 0;

/* ---- Minimal particle pool implementation (internal) ---- */
typedef struct VfxParticle
{
    uint8_t active;
    uint8_t layer; /* RogueVfxLayer */
    uint8_t world_space;
    uint16_t inst_idx; /* owning instance index */
    float x, y;
    float scale;         /* Phase 4.4: per-particle scale */
    uint32_t color_rgba; /* Phase 4.4: per-particle color */
    uint32_t age_ms;
    uint32_t lifetime_ms;
} VfxParticle;

#define ROGUE_VFX_PART_CAP 1024
static VfxParticle g_vfx_parts[ROGUE_VFX_PART_CAP];
/* Simple camera state for world->screen transforms */
static float g_cam_x = 0.0f, g_cam_y = 0.0f; /* world origin at top-left of view */
static float g_pixels_per_world = 32.0f;     /* default: 32 px per world unit */

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
        /* Expire strictly after lifetime (not at the exact boundary) */
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

int rogue_vfx_particles_layer_count(RogueVfxLayer layer)
{
    return vfx_particles_layer_count(layer);
}

int rogue_vfx_particles_collect_ordered(uint8_t* out_layers, int max)
{
    if (!out_layers || max <= 0)
        return 0;
    /* Canonical order: BG, MID, FG, UI. Emit each layer once if any particle exists. */
    int written = 0;
    for (int lay = (int) ROGUE_VFX_LAYER_BG; lay <= (int) ROGUE_VFX_LAYER_UI; ++lay)
    {
        if (written >= max)
            break;
        if (vfx_particles_layer_count((RogueVfxLayer) lay) > 0)
        {
            out_layers[written++] = (uint8_t) lay;
        }
    }
    return written;
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
    int written = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && written < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        float sx = g_vfx_parts[i].x;
        float sy = g_vfx_parts[i].y;
        if (g_vfx_parts[i].world_space)
        {
            /* world -> screen */
            sx = (sx - g_cam_x) * g_pixels_per_world;
            sy = (sy - g_cam_y) * g_pixels_per_world;
        }
        out_xy[written * 2 + 0] = sx;
        out_xy[written * 2 + 1] = sy;
        if (out_layers)
            out_layers[written] = g_vfx_parts[i].layer;
        ++written;
    }
    return written;
}

static int vfx_reg_find(const char* id)
{
    for (int i = 0; i < g_vfx_reg_count; ++i)
    {
        if (strncmp(g_vfx_reg[i].id, id, sizeof g_vfx_reg[i].id) == 0)
            return i;
    }
    return -1;
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
            return i;
        }
    }
    return -1;
}

void rogue_vfx_update(uint32_t dt_ms)
{
    if (g_vfx_frozen)
        return;
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

        /* Composition scheduling: spawn child effects based on age and delays */
        if (r->comp_mode && g_vfx_inst[i].active)
        {
            while (g_vfx_inst[i].comp_next_child < r->comp_child_count)
            {
                uint8_t ci = g_vfx_inst[i].comp_next_child;
                uint32_t delay = r->comp_child_delays[ci];
                uint32_t ref_time = (r->comp_mode == 1 /*CHAIN*/)
                                        ? g_vfx_inst[i].comp_last_spawn_ms
                                        : 0u; /* PARALLEL relative to start */
                if (g_vfx_inst[i].age_ms >= ref_time + delay)
                {
                    uint16_t child_ridx = r->comp_child_indices[ci];
                    if (child_ridx < (uint16_t) g_vfx_reg_count)
                    {
                        /* Spawn child at same position */
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
                    /* Continue loop to allow multiple children to spawn in the same tick */
                    continue;
                }
                break; /* next child not due yet */
            }
        }
    }

    /* Update particles: spawn and age/expire */
    if (!g_vfx_frozen)
    {
        float dt_sec = dt * 0.001f;
        /* Spawn new particles for each active instance based on its registry emitter */
        for (int i = 0; i < ROGUE_VFX_INST_CAP; ++i)
        {
            if (!g_vfx_inst[i].active)
                continue;
            VfxReg* r = &g_vfx_reg[g_vfx_inst[i].reg_index];
            if (r->emit_hz <= 0 || r->p_lifetime_ms == 0 || r->p_max <= 0)
                continue;
            /* accumulate fractional spawns */
            g_vfx_inst[i].emit_accum += r->emit_hz * dt_sec;
            int want = (int) g_vfx_inst[i].emit_accum;
            if (want <= 0)
                continue;
            g_vfx_inst[i].emit_accum -= (float) want;
            /* count current particles for this instance */
            int cur = 0;
            for (int p = 0; p < ROGUE_VFX_PART_CAP; ++p)
                if (g_vfx_parts[p].active && g_vfx_parts[p].inst_idx == (uint16_t) i)
                    ++cur;
            int can = r->p_max - cur;
            if (can <= 0)
                continue;
            int to_spawn = want < can ? want : can;
            for (int s = 0; s < to_spawn; ++s)
            {
                int pi = vfx_part_alloc();
                if (pi < 0)
                    break; /* pool full */
                g_vfx_parts[pi].active = 1;
                g_vfx_parts[pi].inst_idx = (uint16_t) i;
                g_vfx_parts[pi].layer = r->layer;
                g_vfx_parts[pi].world_space = r->world_space;
                g_vfx_parts[pi].x = g_vfx_inst[i].x;
                g_vfx_parts[pi].y = g_vfx_inst[i].y;
                /* Base scale from override or 1.0 */
                float base_scale = (g_vfx_inst[i].ov_scale > 0.0f) ? g_vfx_inst[i].ov_scale : 1.0f;
                float scale_mul = 1.0f;
                if (r->var_scale_mode == ROGUE_VFX_DIST_UNIFORM)
                {
                    float t = fx_rand01();
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
                    float mean = r->var_scale_a;
                    float sigma = r->var_scale_b;
                    float z = fx_rand_normal01();
                    scale_mul = mean + sigma * z;
                    if (scale_mul <= 0.01f)
                        scale_mul = 0.01f;
                }
                g_vfx_parts[pi].scale = base_scale * scale_mul;
                g_vfx_parts[pi].color_rgba =
                    g_vfx_inst[i].ov_color_rgba ? g_vfx_inst[i].ov_color_rgba : 0xFFFFFFFFu;
                g_vfx_parts[pi].age_ms = 0;
                /* Lifetime variation */
                float life_ms = (float) r->p_lifetime_ms;
                if (r->var_life_mode == ROGUE_VFX_DIST_UNIFORM)
                {
                    float t = fx_rand01();
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
                    float mean = r->var_life_a;
                    float sigma = r->var_life_b;
                    float mul = mean + sigma * fx_rand_normal01();
                    if (mul <= 0.01f)
                        mul = 0.01f;
                    life_ms = life_ms * mul;
                }
                if (life_ms < 1.0f)
                    life_ms = 1.0f; /* minimum 1 ms */
                g_vfx_parts[pi].lifetime_ms = (uint32_t) life_ms;
            }
        }
        /* Age/expire all particles */
        vfx_particles_update(dt);
    }
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
    r->comp_mode = chain_mode ? 1 : 2; /* CHAIN:1, PARALLEL:2 */
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
    int written = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && written < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_scales[written++] = g_vfx_parts[i].scale;
    }
    return written;
}

int rogue_vfx_particles_collect_colors(uint32_t* out_rgba, int max)
{
    if (!out_rgba || max <= 0)
        return 0;
    int written = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && written < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_rgba[written++] = g_vfx_parts[i].color_rgba;
    }
    return written;
}

int rogue_vfx_particles_collect_lifetimes(uint32_t* out_ms, int max)
{
    if (!out_ms || max <= 0)
        return 0;
    int written = 0;
    for (int i = 0; i < ROGUE_VFX_PART_CAP && written < max; ++i)
    {
        if (!g_vfx_parts[i].active)
            continue;
        out_ms[written++] = g_vfx_parts[i].lifetime_ms;
    }
    return written;
}
