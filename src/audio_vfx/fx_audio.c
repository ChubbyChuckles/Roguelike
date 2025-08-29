#include "../util/log.h"
#include "effects.h"
#include "fx_internal.h"
#include <math.h>
#include <string.h>

#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

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

/* Music state */
static char g_music_state_tracks[ROGUE_MUSIC_STATE_COUNT][24];
static RogueMusicState g_music_current_state = ROGUE_MUSIC_STATE_EXPLORE;
static char g_music_active_track[24];
static char g_music_fadeout_track[24];
static float g_music_active_weight = 0.0f;
static float g_music_fadeout_weight = 0.0f;
static uint32_t g_music_fade_time_ms = 0;
static uint32_t g_music_fade_elapsed_ms = 0;
static float g_music_duck_gain = 1.0f;
static float g_music_duck_target = 1.0f;
static uint32_t g_music_duck_attack = 0, g_music_duck_hold = 0, g_music_duck_release = 0;
static uint32_t g_music_duck_elapsed = 0;
static uint32_t g_music_duck_phase_attack_end = 0, g_music_duck_phase_hold_end = 0;
static float g_music_bpm = 120.0f;
static int g_music_beats_per_bar = 4;
static float g_music_bar_time_accum_ms = 0;
static RogueMusicState g_music_pending_bar_state = (RogueMusicState) -1;
static uint32_t g_music_pending_bar_crossfade = 0;
#define ROGUE_MUSIC_MAX_LAYERS_PER_STATE 4
typedef struct MusicLayerReg
{
    char track_id[24];
    float gain;
} MusicLayerReg;
static MusicLayerReg g_music_layers[ROGUE_MUSIC_STATE_COUNT][ROGUE_MUSIC_MAX_LAYERS_PER_STATE];
static uint8_t g_music_layer_counts[ROGUE_MUSIC_STATE_COUNT];
static char g_music_active_sweetener[24];
static float g_music_active_sweetener_gain = 0.0f;

/* Environment */
static RogueAudioReverbPreset g_reverb_preset = ROGUE_AUDIO_REVERB_NONE;
static float g_reverb_target_wet = 0.0f;
static float g_reverb_wet = 0.0f;
static int g_lowpass_enabled = 0;
static float g_lowpass_strength = 0.8f;
static float g_lowpass_min_factor = 0.4f;
static int g_positional_enabled = 0;
static float g_listener_x = 0.0f, g_listener_y = 0.0f;
static float g_falloff_radius = 10.0f;

#ifdef ROGUE_HAVE_SDL_MIXER
static int g_voice_cap = 16;
#endif

static int audio_reg_find(const char* id)
{
    for (int i = 0; i < g_audio_reg_count; ++i)
        if (strncmp(g_audio_reg[i].id, id, sizeof g_audio_reg[i].id) == 0)
            return i;
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
        Mix_PlayChannel(-1, g_audio_reg[idx].chunk, 0);
    }
    else
    {
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
    for (int i = 0; i < ROGUE_MUSIC_STATE_COUNT; ++i)
    {
        g_music_state_tracks[i][0] = '\0';
        g_music_layer_counts[i] = 0;
        for (int j = 0; j < ROGUE_MUSIC_MAX_LAYERS_PER_STATE; ++j)
            g_music_layers[i][j].track_id[0] = '\0';
    }
    g_music_active_track[0] = '\0';
    g_music_fadeout_track[0] = '\0';
    g_music_active_sweetener[0] = '\0';
    g_music_active_weight = 0.0f;
    g_music_fadeout_weight = 0.0f;
    g_music_fade_time_ms = g_music_fade_elapsed_ms = 0;
    g_reverb_preset = ROGUE_AUDIO_REVERB_NONE;
    g_reverb_target_wet = 0.0f;
    g_reverb_wet = 0.0f;
    g_lowpass_enabled = 0;
    g_lowpass_strength = 0.8f;
    g_lowpass_min_factor = 0.4f;
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

float rogue_audio_debug_effective_gain(const char* id, unsigned repeats, float x, float y)
{
    int idx = audio_reg_find(id);
    if (idx < 0)
        return 0.0f;
    float rep = repeats < 1 ? 1.0f : (float) repeats;
    float base = g_audio_reg[idx].base_gain * (0.7f + 0.3f * rep);
    if (base > 1.0f)
        base = 1.0f;
    float cat_gain = g_mixer_cat[g_audio_reg[idx].cat];
    float music_weight = 1.0f;
    if (g_audio_reg[idx].cat == (uint8_t) ROGUE_AUDIO_CAT_MUSIC)
    {
        if (g_music_active_track[0] || g_music_fadeout_track[0])
        {
            if (g_music_active_track[0] &&
                strncmp(g_music_active_track, g_audio_reg[idx].id, sizeof g_audio_reg[idx].id) == 0)
                music_weight = g_music_active_weight;
            else if (g_music_fadeout_track[0] && strncmp(g_music_fadeout_track, g_audio_reg[idx].id,
                                                         sizeof g_audio_reg[idx].id) == 0)
                music_weight = g_music_fadeout_weight;
            else
                music_weight = 0.0f;
        }
        else
        {
            music_weight = 1.0f;
        }
        if (g_music_active_track[0] && g_music_active_sweetener[0] &&
            strncmp(g_music_active_sweetener, g_audio_reg[idx].id, sizeof g_audio_reg[idx].id) == 0)
            music_weight = g_music_active_weight * g_music_active_sweetener_gain;
        if (!g_music_active_track[0] && !g_music_fadeout_track[0])
            music_weight = 1.0f;
        cat_gain *= g_music_duck_gain;
    }
    float attenuation = compute_attenuation(x, y);
    float lp_factor = 1.0f;
    if (g_lowpass_enabled && g_audio_reg[idx].cat != (uint8_t) ROGUE_AUDIO_CAT_MUSIC)
    {
        if (g_lowpass_min_factor < 0.0f)
            g_lowpass_min_factor = 0.0f;
        if (g_lowpass_min_factor > 1.0f)
            g_lowpass_min_factor = 1.0f;
        float hf = g_lowpass_min_factor + (1.0f - g_lowpass_min_factor) * attenuation;
        if (hf > 1.0f)
            hf = 1.0f;
        if (hf < g_lowpass_min_factor)
            hf = g_lowpass_min_factor;
        lp_factor = 1.0f - g_lowpass_strength * (1.0f - hf);
        if (lp_factor < 0.0f)
            lp_factor = 0.0f;
    }
    float eff = base * g_mixer_master * cat_gain * music_weight * attenuation * lp_factor;
    if (g_mixer_mute)
        eff = 0.0f;
    if (eff < 0.0f)
        eff = 0.0f;
    if (eff > 1.0f)
        eff = 1.0f;
    return eff;
}

int rogue_audio_music_register(RogueMusicState state, const char* track_id)
{
    if ((int) state < 0 || state >= ROGUE_MUSIC_STATE_COUNT || !track_id || !*track_id)
        return -1;
    int idx = audio_reg_find(track_id);
    if (idx < 0 || g_audio_reg[idx].cat != (uint8_t) ROGUE_AUDIO_CAT_MUSIC)
        return -2;
#if defined(_MSC_VER)
    strncpy_s(g_music_state_tracks[state], sizeof g_music_state_tracks[state], track_id, _TRUNCATE);
#else
    strncpy(g_music_state_tracks[state], track_id, sizeof g_music_state_tracks[state] - 1);
    g_music_state_tracks[state][sizeof g_music_state_tracks[state] - 1] = '\0';
#endif
    return 0;
}

static void music_begin_crossfade(const char* new_track, uint32_t crossfade_ms)
{
    if (!new_track || !*new_track)
        return;
    if (crossfade_ms == 0 || g_music_active_track[0] == '\0')
    {
#if defined(_MSC_VER)
        strncpy_s(g_music_active_track, sizeof g_music_active_track, new_track, _TRUNCATE);
#else
        strncpy(g_music_active_track, new_track, sizeof g_music_active_track - 1);
        g_music_active_track[sizeof g_music_active_track - 1] = '\0';
#endif
        g_music_fadeout_track[0] = '\0';
        g_music_active_weight = 1.0f;
        g_music_fadeout_weight = 0.0f;
        g_music_fade_time_ms = g_music_fade_elapsed_ms = 0;
        g_music_active_sweetener[0] = '\0';
        g_music_active_sweetener_gain = 0.0f;
        uint8_t layer_count = g_music_layer_counts[g_music_current_state];
        if (layer_count > 0)
        {
            uint32_t seed_snapshot = rogue_fx_internal_current_frame() ^
                                     ((uint32_t) g_music_current_state * 0x9E3779B9u) ^
                                     (uint32_t) layer_count * 0x85EBCA6Bu;
            seed_snapshot = seed_snapshot * 1664525u + 1013904223u;
            uint32_t pick = (layer_count == 1) ? 0u : (seed_snapshot % (uint32_t) layer_count);
            MusicLayerReg* lr = &g_music_layers[g_music_current_state][pick];
            if (lr->track_id[0])
            {
#if defined(_MSC_VER)
                strncpy_s(g_music_active_sweetener, sizeof g_music_active_sweetener, lr->track_id,
                          _TRUNCATE);
#else
                strncpy(g_music_active_sweetener, lr->track_id,
                        sizeof g_music_active_sweetener - 1);
                g_music_active_sweetener[sizeof g_music_active_sweetener - 1] = '\0';
#endif
                g_music_active_sweetener_gain = lr->gain;
            }
        }
        return;
    }
#if defined(_MSC_VER)
    strncpy_s(g_music_fadeout_track, sizeof g_music_fadeout_track, g_music_active_track, _TRUNCATE);
    strncpy_s(g_music_active_track, sizeof g_music_active_track, new_track, _TRUNCATE);
#else
    strncpy(g_music_fadeout_track, g_music_active_track, sizeof g_music_fadeout_track - 1);
    g_music_fadeout_track[sizeof g_music_fadeout_track - 1] = '\0';
    strncpy(g_music_active_track, new_track, sizeof g_music_active_track - 1);
    g_music_active_track[sizeof g_music_active_track - 1] = '\0';
#endif
    g_music_fade_time_ms = crossfade_ms;
    g_music_fade_elapsed_ms = 0;
    g_music_active_weight = 0.0f;
    g_music_fadeout_weight = 1.0f;
    g_music_active_sweetener[0] = '\0';
    g_music_active_sweetener_gain = 0.0f;
    uint8_t layer_count = g_music_layer_counts[g_music_current_state];
    if (layer_count > 0)
    {
        uint32_t seed_snapshot = rogue_fx_internal_current_frame() ^
                                 ((uint32_t) g_music_current_state * 0x9E3779B9u) ^
                                 (uint32_t) layer_count * 0x85EBCA6Bu;
        seed_snapshot = seed_snapshot * 1664525u + 1013904223u;
        uint32_t pick = (layer_count == 1) ? 0u : (seed_snapshot % (uint32_t) layer_count);
        MusicLayerReg* lr = &g_music_layers[g_music_current_state][pick];
        if (lr->track_id[0])
        {
#if defined(_MSC_VER)
            strncpy_s(g_music_active_sweetener, sizeof g_music_active_sweetener, lr->track_id,
                      _TRUNCATE);
#else
            strncpy(g_music_active_sweetener, lr->track_id, sizeof g_music_active_sweetener - 1);
            g_music_active_sweetener[sizeof g_music_active_sweetener - 1] = '\0';
#endif
            g_music_active_sweetener_gain = lr->gain;
        }
    }
}

int rogue_audio_music_set_state(RogueMusicState state, uint32_t crossfade_ms)
{
    if ((int) state < 0 || state >= ROGUE_MUSIC_STATE_COUNT)
        return -1;
    g_music_current_state = state;
    const char* track = g_music_state_tracks[state];
    if (!track[0])
        return -2;
    music_begin_crossfade(track, crossfade_ms);
    return 0;
}

void rogue_audio_music_update(uint32_t dt_ms)
{
    uint32_t fade_dt_ms = dt_ms;
    int fade_started_this_update = 0;
    uint32_t post_boundary_elapsed_ms = 0;
    if (g_music_bpm < 20.0f)
        g_music_bpm = 20.0f;
    if (g_music_bpm > 300.0f)
        g_music_bpm = 300.0f;
    if (g_music_beats_per_bar < 1)
        g_music_beats_per_bar = 1;
    if (g_music_beats_per_bar > 16)
        g_music_beats_per_bar = 16;
    float ms_per_beat = 60000.0f / g_music_bpm;
    float bar_ms = ms_per_beat * (float) g_music_beats_per_bar;
    g_music_bar_time_accum_ms += (float) dt_ms;
    if (g_music_bar_time_accum_ms >= bar_ms)
    {
        float over_total = g_music_bar_time_accum_ms - bar_ms;
        (void) over_total;
        g_music_bar_time_accum_ms = fmodf(g_music_bar_time_accum_ms, bar_ms);
        if (g_music_bar_time_accum_ms < 0.0f)
            g_music_bar_time_accum_ms = 0.0f;
        post_boundary_elapsed_ms = (uint32_t) (g_music_bar_time_accum_ms + 0.5f);
        if (g_music_pending_bar_state >= 0)
        {
            if (g_music_fade_time_ms == 0 || g_music_fade_elapsed_ms >= g_music_fade_time_ms)
            {
                g_music_current_state = g_music_pending_bar_state;
                const char* track = g_music_state_tracks[g_music_current_state];
                if (track[0])
                {
                    music_begin_crossfade(track, g_music_pending_bar_crossfade);
                    fade_started_this_update = 1;
                }
            }
            if (g_music_fade_time_ms == 0 || g_music_fade_elapsed_ms >= g_music_fade_time_ms)
                g_music_pending_bar_state = (RogueMusicState) -1;
        }
    }

    if (g_music_fade_time_ms > 0 && g_music_fade_elapsed_ms < g_music_fade_time_ms)
    {
        if (fade_started_this_update)
        {
            fade_dt_ms = post_boundary_elapsed_ms;
            if (fade_dt_ms > dt_ms)
                fade_dt_ms = dt_ms;
        }
        g_music_fade_elapsed_ms += fade_dt_ms;
        if (g_music_fade_elapsed_ms >= g_music_fade_time_ms)
        {
            g_music_active_weight = 1.0f;
            g_music_fadeout_weight = 0.0f;
            g_music_fadeout_track[0] = '\0';
            g_music_fade_time_ms = 0;
        }
        else
        {
            float t = (float) g_music_fade_elapsed_ms / (float) g_music_fade_time_ms;
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;
            g_music_active_weight = t;
            g_music_fadeout_weight = 1.0f - t;
        }
    }

    if (g_music_duck_attack || g_music_duck_hold || g_music_duck_release)
    {
        g_music_duck_elapsed += dt_ms;
        uint32_t e = g_music_duck_elapsed;
        if (e <= g_music_duck_phase_attack_end)
        {
            float t = (g_music_duck_attack ? (float) e / (float) g_music_duck_attack : 1.0f);
            if (t < 0.0f)
                t = 0.0f;
            if (t > 1.0f)
                t = 1.0f;
            g_music_duck_gain = 1.0f + t * (g_music_duck_target - 1.0f);
        }
        else if (e <= g_music_duck_phase_hold_end)
        {
            g_music_duck_gain = g_music_duck_target;
        }
        else
        {
            uint32_t rel_elapsed = e - g_music_duck_phase_hold_end;
            if (g_music_duck_release == 0)
                g_music_duck_gain = 1.0f;
            else
            {
                float t = (float) rel_elapsed / (float) g_music_duck_release;
                if (t < 0.0f)
                    t = 0.0f;
                if (t > 1.0f)
                    t = 1.0f;
                g_music_duck_gain = g_music_duck_target + t * (1.0f - g_music_duck_target);
            }
            if (rel_elapsed >= g_music_duck_release)
            {
                g_music_duck_attack = g_music_duck_hold = g_music_duck_release = 0;
                g_music_duck_elapsed = 0;
                g_music_duck_gain = 1.0f;
            }
        }
        if (g_music_duck_gain < 0.0f)
            g_music_duck_gain = 0.0f;
        if (g_music_duck_gain > 1.0f)
            g_music_duck_gain = 1.0f;
    }

    float target = g_reverb_target_wet;
    if (target < 0.0f)
        target = 0.0f;
    if (target > 1.0f)
        target = 1.0f;
    float diff = target - g_reverb_wet;
    float step = (dt_ms / 250.0f);
    if (step > 1.0f)
        step = 1.0f;
    g_reverb_wet += diff * step;
}

void rogue_audio_env_set_reverb_preset(RogueAudioReverbPreset preset)
{
    g_reverb_preset = preset;
    switch (preset)
    {
    case ROGUE_AUDIO_REVERB_NONE:
        g_reverb_target_wet = 0.0f;
        break;
    case ROGUE_AUDIO_REVERB_CAVE:
        g_reverb_target_wet = 0.55f;
        break;
    case ROGUE_AUDIO_REVERB_HALL:
        g_reverb_target_wet = 0.40f;
        break;
    case ROGUE_AUDIO_REVERB_CHAMBER:
        g_reverb_target_wet = 0.30f;
        break;
    default:
        g_reverb_target_wet = 0.0f;
        break;
    }
}
RogueAudioReverbPreset rogue_audio_env_get_reverb_preset(void) { return g_reverb_preset; }
float rogue_audio_env_get_reverb_wet(void) { return g_reverb_wet; }

void rogue_audio_enable_distance_lowpass(int enable) { g_lowpass_enabled = enable ? 1 : 0; }
int rogue_audio_get_distance_lowpass_enabled(void) { return g_lowpass_enabled; }
void rogue_audio_set_lowpass_params(float strength, float min_factor)
{
    if (strength < 0.0f)
        strength = 0.0f;
    if (strength > 1.0f)
        strength = 1.0f;
    if (min_factor < 0.0f)
        min_factor = 0.0f;
    if (min_factor > 1.0f)
        min_factor = 1.0f;
    g_lowpass_strength = strength;
    g_lowpass_min_factor = min_factor;
}
void rogue_audio_get_lowpass_params(float* out_strength, float* out_min_factor)
{
    if (out_strength)
        *out_strength = g_lowpass_strength;
    if (out_min_factor)
        *out_min_factor = g_lowpass_min_factor;
}
const char* rogue_audio_music_current(void)
{
    return g_music_active_track[0] ? g_music_active_track : NULL;
}
void rogue_audio_duck_music(float target_gain, uint32_t attack_ms, uint32_t hold_ms,
                            uint32_t release_ms)
{
    if (target_gain < 0.0f)
        target_gain = 0.0f;
    if (target_gain > 1.0f)
        target_gain = 1.0f;
    g_music_duck_target = target_gain;
    g_music_duck_attack = attack_ms;
    g_music_duck_hold = hold_ms;
    g_music_duck_release = release_ms;
    g_music_duck_elapsed = 0;
    g_music_duck_phase_attack_end = attack_ms;
    g_music_duck_phase_hold_end = attack_ms + hold_ms;
    if (attack_ms == 0)
        g_music_duck_gain = target_gain;
}
float rogue_audio_music_track_weight(const char* track_id)
{
    if (!track_id || !*track_id)
        return 0.0f;
    if (g_music_active_track[0] &&
        strncmp(track_id, g_music_active_track, sizeof g_music_active_track) == 0)
        return g_music_active_weight;
    if (g_music_fadeout_track[0] &&
        strncmp(track_id, g_music_fadeout_track, sizeof g_music_fadeout_track) == 0)
        return g_music_fadeout_weight;
    return 0.0f;
}
int rogue_audio_music_layer_add(RogueMusicState state, const char* sweetener_track_id, float gain)
{
    if ((int) state < 0 || state >= ROGUE_MUSIC_STATE_COUNT || !sweetener_track_id ||
        !*sweetener_track_id)
        return -1;
    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    int tidx = audio_reg_find(sweetener_track_id);
    if (tidx < 0 || g_audio_reg[tidx].cat != (uint8_t) ROGUE_AUDIO_CAT_MUSIC)
        return -2;
    uint8_t* count = &g_music_layer_counts[state];
    if (*count >= ROGUE_MUSIC_MAX_LAYERS_PER_STATE)
        return -3;
    MusicLayerReg* r = &g_music_layers[state][*count];
#if defined(_MSC_VER)
    strncpy_s(r->track_id, sizeof r->track_id, sweetener_track_id, _TRUNCATE);
#else
    strncpy(r->track_id, sweetener_track_id, sizeof r->track_id - 1);
    r->track_id[sizeof r->track_id - 1] = '\0';
#endif
    r->gain = gain;
    (*count)++;
    return 0;
}
const char* rogue_audio_music_layer_current(void)
{
    return g_music_active_sweetener[0] ? g_music_active_sweetener : NULL;
}
int rogue_audio_music_layer_count(RogueMusicState state)
{
    if ((int) state < 0 || state >= ROGUE_MUSIC_STATE_COUNT)
        return -1;
    return (int) g_music_layer_counts[state];
}
void rogue_audio_music_set_tempo(float bpm, int beats_per_bar)
{
    if (bpm < 20.0f)
        bpm = 20.0f;
    else if (bpm > 300.0f)
        bpm = 300.0f;
    if (beats_per_bar < 1)
        beats_per_bar = 1;
    else if (beats_per_bar > 16)
        beats_per_bar = 16;
    float prev_ms_per_beat = 60000.0f / g_music_bpm;
    float prev_bar_ms = prev_ms_per_beat * (float) g_music_beats_per_bar;
    float norm = 0.0f;
    if (prev_bar_ms > 1e-6f)
        norm = g_music_bar_time_accum_ms / prev_bar_ms;
    g_music_bpm = bpm;
    g_music_beats_per_bar = beats_per_bar;
    float new_ms_per_beat = 60000.0f / g_music_bpm;
    float new_bar_ms = new_ms_per_beat * (float) g_music_beats_per_bar;
    g_music_bar_time_accum_ms = norm * new_bar_ms;
    if (g_music_bar_time_accum_ms < 0)
        g_music_bar_time_accum_ms = 0;
    if (g_music_bar_time_accum_ms > new_bar_ms)
        g_music_bar_time_accum_ms = fmodf(g_music_bar_time_accum_ms, new_bar_ms);
}
int rogue_audio_music_set_state_on_next_bar(RogueMusicState state, uint32_t crossfade_ms)
{
    if ((int) state < 0 || state >= ROGUE_MUSIC_STATE_COUNT)
        return -1;
    const char* track = g_music_state_tracks[state];
    if (!track[0])
        return -2;
    g_music_pending_bar_state = state;
    g_music_pending_bar_crossfade = crossfade_ms;
    return 0;
}

/* Dispatch play helper used by bus (variant selection + SDL volume set if available) */
void rogue_audio_dispatch_play_event(const RogueEffectEvent* e)
{
    if (!e)
        return;
    /* Determine variant choice */
    int base_len = (int) strnlen(e->id, sizeof e->id);
    const char* chosen = e->id;
    int var_indices[32];
    int var_count = 0;
    for (int ridx = 0;
         ridx < g_audio_reg_count && var_count < (int) (sizeof var_indices / sizeof var_indices[0]);
         ++ridx)
    {
        if (strncmp(g_audio_reg[ridx].id, e->id, (size_t) base_len) == 0 &&
            g_audio_reg[ridx].id[base_len] == '_')
            var_indices[var_count++] = ridx;
    }
    if (var_count > 0)
    {
        uint32_t s =
            (rogue_fx_internal_current_frame() * 2654435761u) ^ e->seq ^ rogue_fx_rand_u32();
        int pick = (int) (s % (uint32_t) var_count);
        chosen = g_audio_reg[var_indices[pick]].id;
    }
#ifdef ROGUE_HAVE_SDL_MIXER
    int ci = audio_reg_find(chosen);
    if (ci >= 0)
    {
        if (!g_audio_reg[ci].chunk)
            g_audio_reg[ci].chunk = Mix_LoadWAV(g_audio_reg[ci].path);
        if (g_audio_reg[ci].chunk && !(g_voice_cap > 0 && Mix_Playing(-1) >= g_voice_cap))
        {
            float eff = rogue_audio_debug_effective_gain(
                chosen, (unsigned) (e->repeats ? e->repeats : 1), e->x, e->y);
            int vol = (int) (eff * 128.0f);
            Mix_Volume(-1, vol);
            Mix_PlayChannel(-1, g_audio_reg[ci].chunk, 0);
        }
    }
#else
    (void) chosen;
    rogue_audio_play_by_id(e->id);
#endif
}
