#include "../game/combat.h"
#include "../util/log.h"
#include "effects.h"
#include "fx_internal.h"
#include <stdlib.h>
#include <string.h>

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

/* Damage hook observer id (-1 if not bound) */
static int g_damage_observer_id = -1;

/* -------- Phase 9: minimal replay recording -------- */
typedef struct RecordedEv
{
    RogueEffectEvent ev;
} RecordedEv;

#define ROGUE_FX_REPLAY_CAP 2048
static RecordedEv g_fx_record[ROGUE_FX_REPLAY_CAP];
static int g_fx_record_count = 0;
static int g_fx_recording = 0;
/* Replay storage for playback */
static RecordedEv g_fx_replay_seq[ROGUE_FX_REPLAY_CAP];
static int g_fx_replay_count = 0;

/* Expose frame index internally for music layer seeding and other systems that may need it */
uint32_t rogue_fx_internal_current_frame(void) { return g_frame_index; }

static uint32_t rotl32(uint32_t x, int r) { return (x << r) | (x >> (32 - r)); }
static uint32_t digest_event32(const RogueEffectEvent* e)
{
    uint32_t h = 0x85EBCA6Bu;
    h ^= (uint32_t) e->type * 0x9E3779B9u;
    h = rotl32(h, 5);
    h ^= (uint32_t) e->priority * 0x85EBCA6Bu;
    h = rotl32(h, 7);
    uint32_t rep = (uint32_t) (e->repeats == 0 ? 1 : e->repeats);
    h ^= rep * 0xC2B2AE35u;
    h = rotl32(h, 9);
    const unsigned char* p = (const unsigned char*) e->id;
    for (size_t i = 0; i < sizeof e->id; ++i)
    {
        h ^= (uint32_t) p[i] + (uint32_t) (i * 131u);
        h *= 0x27D4EB2Du;
    }
    return h;
}

void rogue_fx_frame_begin(uint32_t frame_index)
{
    g_frame_index = frame_index;
    g_seq_counter = 0;
    g_frame_digest = 0xC001C0DEu ^ frame_index;
    g_write->count = 0;
}

void rogue_fx_frame_end(void)
{
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
    if (g_fx_recording && g_fx_record_count < ROGUE_FX_REPLAY_CAP)
        g_fx_record[g_fx_record_count++].ev = *out;
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
        return 1;
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

/* Dispatcher helpers provided by other modules */
extern void rogue_audio_dispatch_play_event(const RogueEffectEvent* e);
extern void rogue_vfx_dispatch_spawn_event(const RogueEffectEvent* e);

/* Simple stable sort by (priority, id, seq) */
static int cmp_ev(const void* a, const void* b)
{
    const RogueEffectEvent* ea = (const RogueEffectEvent*) a;
    const RogueEffectEvent* eb = (const RogueEffectEvent*) b;
    if (ea->emit_frame != eb->emit_frame)
        return (ea->emit_frame < eb->emit_frame) ? -1 : 1;
    if (ea->priority != eb->priority)
        return (int) ea->priority - (int) eb->priority;
    int idcmp = strncmp(ea->id, eb->id, sizeof ea->id);
    if (idcmp != 0)
        return idcmp;
    if (ea->seq < eb->seq)
        return -1;
    if (ea->seq > eb->seq)
        return 1;
    return 0;
}

int rogue_fx_dispatch_process(void)
{
    if (g_read->count <= 0)
        return 0;
    qsort(g_read->ev, (size_t) g_read->count, sizeof g_read->ev[0], cmp_ev);
    /* Frame compaction */
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
        e->seq = (uint32_t) i;
        g_frame_digest ^= digest_event32(e);
        if (e->type == ROGUE_FX_AUDIO_PLAY)
            rogue_audio_dispatch_play_event(e);
        else if (e->type == ROGUE_FX_VFX_SPAWN)
            rogue_vfx_dispatch_spawn_event(e);
    }
    int processed = g_read->count;
    g_read->count = 0;
    return processed;
}

uint32_t rogue_fx_get_frame_digest(void) { return g_frame_digest; }

/* -------- Phase 9: Replay & hashing -------- */
void rogue_fx_replay_begin_record(void)
{
    g_fx_record_count = 0;
    g_fx_recording = 1;
}
int rogue_fx_replay_is_recording(void) { return g_fx_recording; }
int rogue_fx_replay_end_record(RogueEffectEvent* out, int max)
{
    g_fx_recording = 0;
    int n = g_fx_record_count;
    if (out && max > 0)
    {
        int w = (n < max) ? n : max;
        for (int i = 0; i < w; ++i)
            out[i] = g_fx_record[i].ev;
    }
    return n;
}

void rogue_fx_replay_load(const RogueEffectEvent* ev, int count)
{
    g_fx_replay_count = 0;
    if (!ev || count <= 0)
        return;
    if (count > ROGUE_FX_REPLAY_CAP)
        count = ROGUE_FX_REPLAY_CAP;
    for (int i = 0; i < count; ++i)
        g_fx_replay_seq[i].ev = ev[i];
    g_fx_replay_count = count;
}
int rogue_fx_replay_enqueue_frame(uint32_t frame_index)
{
    int enq = 0;
    for (int i = 0; i < g_fx_replay_count; ++i)
    {
        if (g_fx_replay_seq[i].ev.emit_frame == frame_index)
        {
            RogueEffectEvent e = g_fx_replay_seq[i].ev;
            if (rogue_fx_emit(&e) == 0)
                ++enq;
        }
    }
    return enq;
}
void rogue_fx_replay_clear(void) { g_fx_replay_count = 0; }

static unsigned long long g_fx_hash_accum = 1469598103934665603ull;
static const unsigned long long g_fx_hash_prime = 1099511628211ull;
void rogue_fx_hash_reset(unsigned long long seed)
{
    g_fx_hash_accum = seed ? seed : 1469598103934665603ull;
}
void rogue_fx_hash_accumulate_frame(void)
{
    unsigned int d = rogue_fx_get_frame_digest();
    g_fx_hash_accum ^= (unsigned long long) d;
    g_fx_hash_accum *= g_fx_hash_prime;
}
unsigned long long rogue_fx_hash_get(void) { return g_fx_hash_accum; }

static unsigned long long fnv1a64_bytes(const void* data, size_t n)
{
    const unsigned char* p = (const unsigned char*) data;
    unsigned long long h = 1469598103934665603ull;
    const unsigned long long prime = 1099511628211ull;
    for (size_t i = 0; i < n; ++i)
    {
        h ^= (unsigned long long) p[i];
        h *= prime;
    }
    return h;
}
unsigned long long rogue_fx_events_hash(const RogueEffectEvent* ev, int count)
{
    if (!ev || count <= 0)
        return 0ull;
    unsigned long long h = 1469598103934665603ull;
    const unsigned long long prime = 1099511628211ull;
    for (int i = 0; i < count; ++i)
    {
        h ^= fnv1a64_bytes(&ev[i], sizeof ev[i]);
        h *= prime;
    }
    return h;
}
