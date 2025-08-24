#include "buffs.h"
#include "../audio_vfx/effects.h" /* Phase 5.4: buff gain/expire cues */
#include "../core/app/app_state.h"
#include <string.h>

/* Simple fixed-size buff list */
#define ROGUE_MAX_ACTIVE_BUFFS 16
static RogueBuff g_buffs[ROGUE_MAX_ACTIVE_BUFFS];
static double g_min_reapply_interval_ms = 50.0; /* default dampening window */

void rogue_buffs_init(void) { memset(g_buffs, 0, sizeof g_buffs); }

void rogue_buffs_update(double now_ms)
{
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && now_ms >= g_buffs[i].end_ms)
        {
            /* FX: buff expire cue before deactivating */
            char key[48];
            snprintf(key, sizeof key, "buff/%d/expire", (int) g_buffs[i].type);
            rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
            g_buffs[i].active = 0;
        }
}

int rogue_buffs_apply(RogueBuffType type, int magnitude, double duration_ms, double now_ms,
                      RogueBuffStackRule rule, int snapshot)
{
    if (magnitude <= 0 || duration_ms <= 0)
        return 0;
    if (now_ms < 0)
        now_ms = 0;
    if (rule < 0 || rule > ROGUE_BUFF_STACK_ADD)
        rule = ROGUE_BUFF_STACK_ADD;
    /* dampening */
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type)
        {
            if (now_ms - g_buffs[i].last_apply_ms < g_min_reapply_interval_ms)
                return 0;
            break;
        }
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type)
        {
            RogueBuff* b = &g_buffs[i];
            b->last_apply_ms = now_ms;
            switch (rule)
            {
            case ROGUE_BUFF_STACK_UNIQUE:
                return 0; /* ignore */
            case ROGUE_BUFF_STACK_REFRESH:
                if (magnitude > b->magnitude)
                    b->magnitude = magnitude;
                b->end_ms = now_ms + duration_ms;
                return 1;
            case ROGUE_BUFF_STACK_EXTEND:
                b->end_ms += duration_ms;
                if (b->end_ms < now_ms + duration_ms)
                    b->end_ms = now_ms + duration_ms;
                if (b->magnitude < magnitude)
                    b->magnitude = magnitude;
                return 1;
            case ROGUE_BUFF_STACK_ADD:
            default:
                b->magnitude += magnitude;
                if (b->magnitude > 999)
                    b->magnitude = 999;
                if (now_ms + duration_ms > b->end_ms)
                    b->end_ms = now_ms + duration_ms;
                return 1;
            }
        }
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (!g_buffs[i].active)
        {
            g_buffs[i].active = 1;
            g_buffs[i].type = type;
            g_buffs[i].magnitude = magnitude;
            g_buffs[i].end_ms = now_ms + duration_ms;
            g_buffs[i].snapshot = snapshot ? 1 : 0;
            g_buffs[i].stack_rule = rule;
            g_buffs[i].last_apply_ms = now_ms;
            /* FX: buff gain cue */
            {
                char key[48];
                snprintf(key, sizeof key, "buff/%d/gain", (int) type);
                rogue_fx_trigger_event(key, g_app.player.base.pos.x, g_app.player.base.pos.y);
            }
            return 1;
        }
    return 0;
}

int rogue_buffs_strength_bonus(void)
{
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == ROGUE_BUFF_STAT_STRENGTH)
            total += g_buffs[i].magnitude;
    return total;
}
void rogue_buffs_set_dampening(double min_interval_ms)
{
    if (min_interval_ms < 0)
        min_interval_ms = 0;
    g_min_reapply_interval_ms = min_interval_ms;
}

int rogue_buffs_get_total(RogueBuffType type)
{
    int total = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active && g_buffs[i].type == type)
            total += g_buffs[i].magnitude;
    return total;
}

int rogue_buffs_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active)
            c++;
    return c;
}
int rogue_buffs_get_active(int index, RogueBuff* out)
{
    if (!out)
        return 0;
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS; i++)
        if (g_buffs[i].active)
        {
            if (c == index)
            {
                *out = g_buffs[i];
                return 1;
            }
            c++;
        }
    return 0;
}
int rogue_buffs_snapshot(RogueBuff* out, int max, double now_ms)
{
    if (!out || max <= 0)
        return 0;
    int c = 0;
    for (int i = 0; i < ROGUE_MAX_ACTIVE_BUFFS && c < max; i++)
    {
        if (g_buffs[i].active)
        {
            if (now_ms >= g_buffs[i].end_ms)
            {
                g_buffs[i].active = 0;
                continue;
            }
            out[c++] = g_buffs[i];
        }
    }
    return c;
}
