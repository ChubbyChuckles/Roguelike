#include "core/loot/loot_rarity_adv.h"
#include <string.h>

#define RARITY_COUNT 5
static char g_spawn_sounds[RARITY_COUNT][32];
static char g_pickup_sounds[RARITY_COUNT][32]; /* Phase 19.1 */
static int g_despawn_ms[RARITY_COUNT];         /* 0 = use default */
static int g_floor = -1;
static int g_pity_counter = 0;
static int g_pity_epic_threshold = 0;      /* if >0 triggers upgrade to EPIC */
static int g_pity_legendary_threshold = 0; /* if >0 triggers upgrade to LEGENDARY */
static int g_pity_accel_enabled = 1;       /* 9.4: enable dynamic threshold acceleration */

/* Effective dynamic thresholds (computed on demand) */
static int rarity_effective_epic_threshold(void)
{
    int base = g_pity_epic_threshold;
    if (base <= 0)
        return base;
    if (!g_pity_accel_enabled)
        return base;
    /* Simple acceleration: once counter passes half the base threshold, reduce remaining
     * requirement by 25% */
    if (g_pity_counter > base / 2)
    {
        int reduced = (base * 3) / 4;
        if (reduced < 1)
            reduced = 1;
        return reduced;
    }
    return base;
}
static int rarity_effective_legendary_threshold(void)
{
    int base = g_pity_legendary_threshold;
    if (base <= 0)
        return base;
    if (!g_pity_accel_enabled)
        return base;
    if (g_pity_counter > base / 2)
    {
        int reduced = (base * 3) / 4;
        if (reduced < 1)
            reduced = 1;
        return reduced;
    }
    return base;
}

void rogue_rarity_adv_reset(void)
{
    memset(g_spawn_sounds, 0, sizeof g_spawn_sounds);
    memset(g_pickup_sounds, 0, sizeof g_pickup_sounds);
    memset(g_despawn_ms, 0, sizeof g_despawn_ms);
    g_floor = -1;
    g_pity_counter = 0;
    g_pity_epic_threshold = 0;
    g_pity_legendary_threshold = 0;
}

int rogue_rarity_set_spawn_sound(int rarity, const char* id)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return -1;
    if (!id)
    {
        g_spawn_sounds[rarity][0] = '\0';
        return 0;
    }
#if defined(_MSC_VER)
    strncpy_s(g_spawn_sounds[rarity], sizeof g_spawn_sounds[rarity], id, _TRUNCATE);
#else
    strncpy(g_spawn_sounds[rarity], id, sizeof g_spawn_sounds[rarity] - 1);
#endif
    return 0;
}
const char* rogue_rarity_get_spawn_sound(int rarity)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return NULL;
    return g_spawn_sounds[rarity][0] ? g_spawn_sounds[rarity] : NULL;
}

int rogue_rarity_set_pickup_sound(int rarity, const char* id)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return -1;
    if (!id)
    {
        g_pickup_sounds[rarity][0] = '\0';
        return 0;
    }
#ifdef _MSC_VER
    strncpy_s(g_pickup_sounds[rarity], sizeof g_pickup_sounds[rarity], id, _TRUNCATE);
#else
    strncpy(g_pickup_sounds[rarity], id, sizeof g_pickup_sounds[rarity] - 1);
    g_pickup_sounds[rarity][sizeof g_pickup_sounds[rarity] - 1] = '\0';
#endif
    return 0;
}
const char* rogue_rarity_get_pickup_sound(int rarity)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return NULL;
    return g_pickup_sounds[rarity][0] ? g_pickup_sounds[rarity] : NULL;
}

int rogue_rarity_set_despawn_ms(int rarity, int ms)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return -1;
    g_despawn_ms[rarity] = ms <= 0 ? 0 : ms;
    return 0;
}
int rogue_rarity_get_despawn_ms(int rarity)
{
    if (rarity < 0 || rarity >= RARITY_COUNT)
        return 0;
    return g_despawn_ms[rarity];
}

void rogue_rarity_set_min_floor(int rarity_floor)
{
    g_floor =
        (rarity_floor < 0) ? -1 : (rarity_floor >= RARITY_COUNT ? RARITY_COUNT - 1 : rarity_floor);
}
int rogue_rarity_get_min_floor(void) { return g_floor; }

void rogue_rarity_pity_set_thresholds(int epic_threshold, int legendary_threshold)
{
    g_pity_epic_threshold = epic_threshold;
    g_pity_legendary_threshold = legendary_threshold;
}
void rogue_rarity_pity_reset(void) { g_pity_counter = 0; }
int rogue_rarity_pity_counter(void) { return g_pity_counter; }
void rogue_rarity_pity_set_acceleration(int enabled) { g_pity_accel_enabled = enabled ? 1 : 0; }
int rogue_rarity_pity_get_effective_epic(void) { return rarity_effective_epic_threshold(); }
int rogue_rarity_pity_get_effective_legendary(void)
{
    return rarity_effective_legendary_threshold();
}

int rogue_rarity_apply_floor(int rolled, int rmin, int rmax)
{
    if (g_floor >= 0 && rolled < g_floor && g_floor >= rmin && g_floor <= rmax)
        return g_floor;
    return rolled;
}

/* Apply pity upgrade and reset counter when triggered */
int rogue_rarity_apply_pity(int rolled, int rmin, int rmax)
{
    /* Only count sub-epic rarities (0-2) for pity */
    if (rolled < 3)
        g_pity_counter++;
    else
        g_pity_counter = 0;
    int target = rolled;
    int eff_leg = rarity_effective_legendary_threshold();
    int eff_epic = rarity_effective_epic_threshold();
    if (eff_leg > 0 && g_pity_counter >= eff_leg && rmax >= 4)
    {
        target = 4;
        g_pity_counter = 0;
    }
    else if (eff_epic > 0 && g_pity_counter >= eff_epic && rmax >= 3)
    {
        target = 3;
        g_pity_counter = 0;
    }
    if (target < rmin)
        target = rmin;
    if (target > rmax)
        target = rmax;
    return target;
}
