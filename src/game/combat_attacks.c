#include "game/combat_attacks.h"
#include <math.h>
#include <stddef.h>

/* Static in-memory attack definition table.
   For now we author a tiny chain per archetype (light: 3, heavy:2, thrust:2, ranged:2, spell:1).
   Active windows are simplistic single continuous windows; future versions may add multi-hit
   splits. */

typedef struct RogueAttackChain
{
    const RogueAttackDef* defs;
    int count;
} RogueAttackChain;

static const RogueAttackDef g_light_chain[] = {
    {0,
     "light_1",
     ROGUE_WEAPON_LIGHT,
     0,
     110,
     70,
     120,
     14,
     10,
     5,
     0,
     0.34f,
     0.06f,
     0.00f,
     1,
     {{0, 70, ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF | ROGUE_CANCEL_ON_BLOCK, 1.0f, 0, 0, -1,
       -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF | ROGUE_CANCEL_ON_BLOCK),
     0.50f,
     1.0f,
     0.0f},
    {1,
     "light_2",
     ROGUE_WEAPON_LIGHT,
     1,
     95,
     65,
     115,
     12,
     12,
     6,
     0,
     0.36f,
     0.07f,
     0.00f,
     1,
     {{0, 65, ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.45f,
     1.2f,
     0.0f},
    /* light_3: split into two sub windows for multi-hit demonstration */
    {2,
     "light_3",
     ROGUE_WEAPON_LIGHT,
     2,
     105,
     75,
     140,
     16,
     15,
     8,
     0,
     0.39f,
     0.08f,
     0.00f,
     2,
     {{0, 36, ROGUE_CANCEL_ON_HIT, 0.40f, 0.0f, 0.0f, -1, -1},
      {36, 75, ROGUE_CANCEL_ON_HIT, 0.90f, 0.05f, 0.02f, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.40f,
     1.5f,
     0.0f},
};
static const RogueAttackDef g_heavy_chain[] = {
    {0,
     "heavy_1",
     ROGUE_WEAPON_HEAVY,
     0,
     170,
     90,
     180,
     24,
     28,
     14,
     0,
     0.45f,
     0.05f,
     0.00f,
     1,
     {{0, 90, ROGUE_CANCEL_ON_HIT, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT),
     0.65f,
     3.5f,
     0.0f},
    /* heavy_2: three staggered impact pulses (windows 0 & 1 overlap from 40-50ms to exercise
       stacking) */
    {1,
     "heavy_2",
     ROGUE_WEAPON_HEAVY,
     1,
     190,
     105,
     200,
     28,
     35,
     18,
     0,
     0.50f,
     0.05f,
     0.00f,
     3,
     {{0, 50, ROGUE_CANCEL_ON_HIT, 0.55f, 0.0f, 0.00f, -1, -1},
      {40, 80, ROGUE_CANCEL_ON_HIT | ROGUE_WINDOW_HYPER_ARMOR, 0.85f, 0.03f, 0.00f, -1, -1},
      {80, 105, ROGUE_CANCEL_ON_HIT, 1.25f, 0.06f, 0.02f, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT),
     0.65f,
     4.0f,
     0.0f},
};
static const RogueAttackDef g_thrust_chain[] = {
    {0,
     "thrust_1",
     ROGUE_WEAPON_THRUST,
     0,
     120,
     55,
     110,
     13,
     12,
     7,
     0,
     0.14f,
     0.33f,
     0.00f,
     1,
     {{0, 55, 0, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.45f,
     0.8f,
     0.0f},
    {1,
     "thrust_2",
     ROGUE_WEAPON_THRUST,
     1,
     125,
     60,
     115,
     14,
     14,
     8,
     0,
     0.15f,
     0.35f,
     0.00f,
     1,
     {{0, 60, 0, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.45f,
     0.9f,
     0.0f},
};
static const RogueAttackDef g_ranged_chain[] = {
    {0,
     "ranged_1",
     ROGUE_WEAPON_RANGED,
     0,
     140,
     40,
     100,
     10,
     0,
     4,
     0,
     0.05f,
     0.30f,
     0.00f,
     1,
     {{0, 40, 0, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.35f,
     0.0f,
     0.6f},
    {1,
     "ranged_2",
     ROGUE_WEAPON_RANGED,
     1,
     150,
     50,
     110,
     12,
     0,
     5,
     0,
     0.05f,
     0.34f,
     0.00f,
     1,
     {{0, 50, 0, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.35f,
     0.0f,
     0.7f},
};
static const RogueAttackDef g_spell_chain[] = {
    {0,
     "spell_1",
     ROGUE_WEAPON_SPELL_FOCUS,
     0,
     160,
     60,
     140,
     16,
     0,
     9,
     3,
     0.00f,
     0.00f,
     0.40f,
     1,
     {{0, 60, 0, 1.0f, 0, 0, -1, -1}},
     0.0f,
     (ROGUE_CANCEL_ON_HIT | ROGUE_CANCEL_ON_WHIFF),
     0.40f,
     0.0f,
     1.8f},
};

static const RogueAttackChain g_chains[ROGUE_WEAPON_ARCHETYPE_COUNT] = {
    {g_light_chain, (int) (sizeof g_light_chain / sizeof g_light_chain[0])},
    {g_heavy_chain, (int) (sizeof g_heavy_chain / sizeof g_heavy_chain[0])},
    {g_thrust_chain, (int) (sizeof g_thrust_chain / sizeof g_thrust_chain[0])},
    {g_ranged_chain, (int) (sizeof g_ranged_chain / sizeof g_ranged_chain[0])},
    {g_spell_chain, (int) (sizeof g_spell_chain / sizeof g_spell_chain[0])},
};

const RogueAttackDef* rogue_attack_get(RogueWeaponArchetype arch, int chain_index)
{
    if (arch < 0 || arch >= ROGUE_WEAPON_ARCHETYPE_COUNT)
        return NULL;
    const RogueAttackChain* ch = &g_chains[arch];
    if (ch->count == 0)
        return NULL;
    if (chain_index < 0)
        chain_index = 0;
    if (chain_index >= ch->count)
        chain_index = ch->count - 1; /* clamp */
    return &ch->defs[chain_index];
}

int rogue_attack_chain_length(RogueWeaponArchetype arch)
{
    if (arch < 0 || arch >= ROGUE_WEAPON_ARCHETYPE_COUNT)
        return 0;
    return g_chains[arch].count;
}

/* --- Frame data cache (Phase 1A.1) --------------------------------------------------------- */
typedef struct FrameCacheEntry
{
    const RogueAttackDef* def;
    unsigned char wi;
    short sf, ef;
    unsigned char valid;
} FrameCacheEntry;
static FrameCacheEntry
    g_frame_cache[64]; /* supports up to 64 total windows across all authored attacks */
static int g_frame_cache_initialized = 0;

static void rogue_attack_build_frame_cache(void)
{
    if (g_frame_cache_initialized)
        return;
    g_frame_cache_initialized = 1;
    int cursor = 0;
    for (int arch = 0; arch < ROGUE_WEAPON_ARCHETYPE_COUNT; ++arch)
    {
        int len = rogue_attack_chain_length((RogueWeaponArchetype) arch);
        for (int ci = 0; ci < len; ++ci)
        {
            const RogueAttackDef* def = rogue_attack_get((RogueWeaponArchetype) arch, ci);
            if (!def)
                continue;
            for (int wi = 0; wi < def->num_windows &&
                             cursor < (int) (sizeof(g_frame_cache) / sizeof(g_frame_cache[0]));
                 ++wi)
            {
                const RogueAttackWindow* w = &def->windows[wi];
                float active = (def->active_ms > 0 ? def->active_ms : 1.0f);
                int sf = (int) floorf((w->start_ms / active) * 8.0f);
                int ef = (int) floorf(((w->end_ms > w->start_ms ? w->end_ms : w->start_ms + 0.01f) /
                                       active) *
                                      8.0f) -
                         1;
                if (sf < 0)
                    sf = 0;
                if (sf > 7)
                    sf = 7;
                if (ef < sf)
                    ef = sf;
                if (ef > 7)
                    ef = 7;
                g_frame_cache[cursor].def = def;
                g_frame_cache[cursor].wi = (unsigned char) wi;
                g_frame_cache[cursor].sf = (short) sf;
                g_frame_cache[cursor].ef = (short) ef;
                g_frame_cache[cursor].valid = 1;
                cursor++;
            }
        }
    }
}

static FrameCacheEntry* rogue_attack_find_frame_cache(const RogueAttackDef* def, int wi)
{
    for (size_t i = 0; i < sizeof(g_frame_cache) / sizeof(g_frame_cache[0]); ++i)
    {
        if (g_frame_cache[i].valid && g_frame_cache[i].def == def &&
            g_frame_cache[i].wi == (unsigned char) wi)
            return &g_frame_cache[i];
    }
    return NULL;
}

int rogue_attack_window_frame_span(const RogueAttackDef* def, int window_index,
                                   int* out_start_frame, int* out_end_frame)
{
    if (!def)
        return 0;
    if (window_index < 0 || window_index >= def->num_windows)
        return 0;
    if (!g_frame_cache_initialized)
        rogue_attack_build_frame_cache();
    FrameCacheEntry* e = rogue_attack_find_frame_cache(def, window_index);
    if (!e)
    { /* fallback compute on the fly (should not normally happen unless cache overflow) */
        const RogueAttackWindow* w = &def->windows[window_index];
        float active = (def->active_ms > 0 ? def->active_ms : 1.0f);
        int sf = (int) floorf((w->start_ms / active) * 8.0f);
        int ef =
            (int) floorf(((w->end_ms > w->start_ms ? w->end_ms : w->start_ms + 0.01f) / active) *
                         8.0f) -
            1;
        if (sf < 0)
            sf = 0;
        if (sf > 7)
            sf = 7;
        if (ef < sf)
            ef = sf;
        if (ef > 7)
            ef = 7;
        if (out_start_frame)
            *out_start_frame = sf;
        if (out_end_frame)
            *out_end_frame = ef;
        return 1;
    }
    if (out_start_frame)
        *out_start_frame = e->sf;
    if (out_end_frame)
        *out_end_frame = e->ef;
    return 1;
}
