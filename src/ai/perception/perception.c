#include "perception.h"
#include "../../core/navigation.h" /* rogue_nav_is_blocked */
#include <math.h>
#include <stddef.h>
#include <string.h>

static RoguePerceptionEventBuffer g_events;
static int (*g_blocking_fn)(int, int) = NULL; /* override */

void rogue_perception_events_reset(void) { g_events.count = 0; }

void rogue_perception_emit_sound(RoguePerceptionSoundType type, float x, float y, float loudness)
{
    if (g_events.count >= ROGUE_PERCEPTION_EVENT_CAP)
        return; /* drop when full */
    g_events.events[g_events.count].type = type;
    g_events.events[g_events.count].x = x;
    g_events.events[g_events.count].y = y;
    g_events.events[g_events.count].loudness = loudness;
    g_events.count++;
}

const RoguePerceptionEventBuffer* rogue_perception_events_get(void) { return &g_events; }

void rogue_perception_set_blocking_fn(int (*fn)(int, int)) { g_blocking_fn = fn; }

/* Integer Bresenham line walk between tiles containing points a and b */
int rogue_perception_los(float ax, float ay, float bx, float by)
{
    int x0 = (int) floorf(ax);
    int y0 = (int) floorf(ay);
    int x1 = (int) floorf(bx);
    int y1 = (int) floorf(by);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* error term */
    int (*is_blocked)(int, int) = g_blocking_fn ? g_blocking_fn : rogue_nav_is_blocked;
    while (1)
    {
        if (!(x0 == x1 && y0 == y1))
        {
            if (is_blocked(x0, y0))
                return 0; /* treat origin tile as blocking if impassable */
        }
        else
        {
            return 1; /* reached target without hitting block */
        }
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

int rogue_perception_can_see(const RoguePerceptionAgent* a, float target_x, float target_y,
                             float fov_deg, float max_dist, float* out_dist)
{
    float dx = target_x - a->x;
    float dy = target_y - a->y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 > max_dist * max_dist)
        return 0;
    float dist = sqrtf(dist2);
    float ndx = (dist > 0) ? dx / dist : 0.0f;
    float ndy = (dist > 0) ? dy / dist : 0.0f;
    float dot = ndx * a->facing_x + ndy * a->facing_y;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
    float half_fov_rad = (fov_deg * 0.5f) * (float) M_PI / 180.0f;
    float cos_limit = cosf(half_fov_rad);
    if (dot < cos_limit)
        return 0;
    if (!rogue_perception_los(a->x, a->y, target_x, target_y))
        return 0;
    if (out_dist)
        *out_dist = dist;
    return 1;
}

int rogue_perception_process_hearing(RoguePerceptionAgent* a, float player_x, float player_y,
                                     float hearing_threat, float last_seen_memory_sec)
{
    int contributed = 0;
    for (uint8_t i = 0; i < g_events.count; i++)
    {
        const RoguePerceptionEvent* ev = &g_events.events[i];
        float dx = ev->x - a->x;
        float dy = ev->y - a->y;
        float r2 = ev->loudness * ev->loudness;
        float d2 = dx * dx + dy * dy;
        if (d2 <= r2)
        {
            a->threat += hearing_threat;
            a->last_seen_x = player_x;
            a->last_seen_y = player_y;
            a->has_last_seen = 1;
            a->last_seen_ttl = last_seen_memory_sec;
            contributed++;
        }
    }
    return contributed;
}

void rogue_perception_tick_agent(RoguePerceptionAgent* a, float dt, float player_x, float player_y,
                                 float fov_deg, float max_dist, float visible_threat_per_sec,
                                 float decay_per_sec, float last_seen_memory_sec)
{
    if (!a)
        return;
    /* Vision */
    if (rogue_perception_can_see(a, player_x, player_y, fov_deg, max_dist, NULL))
    {
        a->threat += visible_threat_per_sec * dt;
        a->last_seen_x = player_x;
        a->last_seen_y = player_y;
        a->has_last_seen = 1;
        a->last_seen_ttl = last_seen_memory_sec;
    }
    /* Decay & memory */
    if (a->threat > 0.0f)
    {
        a->threat -= decay_per_sec * dt;
        if (a->threat < 0.0f)
            a->threat = 0.0f;
    }
    if (a->has_last_seen)
    {
        a->last_seen_ttl -= dt;
        if (a->last_seen_ttl <= 0.0f)
        {
            a->has_last_seen = 0;
        }
    }
}

void rogue_perception_broadcast_alert(RoguePerceptionAgent* agents, int agent_count,
                                      int source_index, float radius, float baseline_threat,
                                      float last_seen_memory_sec)
{
    if (!agents || source_index < 0 || source_index >= agent_count)
        return;
    RoguePerceptionAgent* src = &agents[source_index];
    float r2 = radius * radius;
    for (int i = 0; i < agent_count; i++)
    {
        if (i == source_index)
            continue;
        RoguePerceptionAgent* a = &agents[i];
        float dx = a->x - src->x;
        float dy = a->y - src->y;
        float d2 = dx * dx + dy * dy;
        if (d2 > r2)
            continue;
        if (a->threat < baseline_threat)
        {
            a->threat = baseline_threat;
        }
        a->last_seen_x = src->last_seen_x;
        a->last_seen_y = src->last_seen_y;
        a->has_last_seen = src->has_last_seen;
        a->last_seen_ttl = last_seen_memory_sec;
    }
}
