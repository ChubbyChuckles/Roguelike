#include "effects.h"
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
            /* TODO: route to VFX subsystem when implemented */
        }
    }
    int processed = g_read->count;
    g_read->count = 0;
    return processed;
}

uint32_t rogue_fx_get_frame_digest(void) { return g_frame_digest; }
