/**
 * @file perception.c
 * @brief AI perception system implementation.
 *
 * This module provides comprehensive AI perception capabilities including:
 * - Line-of-sight calculations using Bresenham algorithm
 * - Vision cone testing with field-of-view and distance limits
 * - Sound event processing with hearing radius mechanics
 * - Threat accumulation and decay over time
 * - Group alert broadcasting between agents
 * - Memory system for tracking last seen positions
 *
 * The system is designed to be efficient and modular, supporting both
 * individual agent perception and coordinated group behaviors.
 */

#include "perception.h"
#include "../../game/navigation.h" /* rogue_nav_is_blocked */
#include <math.h>
#include <stddef.h>
#include <string.h>

static RoguePerceptionEventBuffer g_events;
static int (*g_blocking_fn)(int, int) = NULL; /**< Optional custom blocking function for line of sight. */

/**
 * @brief Resets the global perception event buffer.
 *
 * Clears all stored sound events from the event buffer, preparing it for
 * a new frame of perception processing.
 */
void rogue_perception_events_reset(void) { g_events.count = 0; }

/**
 * @brief Emits a sound event into the perception system.
 *
 * Adds a new sound event to the global event buffer with the specified type,
 * position, and loudness (hearing radius). Events are dropped if the buffer
 * is at capacity.
 *
 * @param type The type of sound being emitted (attack, footstep, etc.).
 * @param x The world X coordinate of the sound source.
 * @param y The world Y coordinate of the sound source.
 * @param loudness The hearing radius - agents within this distance can detect the sound.
 */
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

/**
 * @brief Gets read-only access to the current perception event buffer.
 *
 * Returns a pointer to the global event buffer containing all sound events
 * emitted during the current frame. This allows external systems to query
 * or process the events.
 *
 * @return Pointer to the read-only event buffer.
 */
const RoguePerceptionEventBuffer* rogue_perception_events_get(void) { return &g_events; }

/**
 * @brief Sets the custom blocking function for line-of-sight calculations.
 *
 * Overrides the default blocking predicate used by rogue_perception_los().
 * The function should return non-zero if the tile at (x,y) blocks vision.
 * Pass NULL to restore the default behavior using rogue_nav_is_blocked().
 *
 * @param fn Pointer to blocking function, or NULL to use default.
 */
void rogue_perception_set_blocking_fn(int (*fn)(int, int)) { g_blocking_fn = fn; }

/**
 * @brief Performs line-of-sight calculation between two world positions.
 *
 * Uses integer Bresenham line algorithm to check visibility between two points.
 * Steps through tiles containing the line segment and tests each for blocking.
 * The origin tile is not considered blocking (agents can see out of their current tile).
 *
 * @param ax World X coordinate of starting position.
 * @param ay World Y coordinate of starting position.
 * @param bx World X coordinate of target position.
 * @param by World Y coordinate of target position.
 * @return 1 if line of sight is clear, 0 if blocked.
 */
int rogue_perception_los(float ax, float ay, float bx, float by)
{
    int x0 = (int) floorf(ax);
    int y0 = (int) floorf(ay);
    int x1 = (int) floorf(bx);
    int y1 = (int) floorf(by);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;                           /* error term */
    int (*is_blocked)(int, int) = g_blocking_fn; /* default to no blocking when unset */
    /* Step first, then test for blocking to avoid treating the origin tile as blocking.
     * This matches expected LOS semantics: an agent can see out of its current tile. */
    while (!(x0 == x1 && y0 == y1))
    {
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
        if (is_blocked && is_blocked(x0, y0))
            return 0;
    }
    return 1; /* reached target without hitting block */
}

/**
 * @brief Tests if an agent can see a target within its vision cone and range.
 *
 * Performs a comprehensive visibility test that includes:
 * 1. Distance check against maximum vision range
 * 2. Field-of-view cone test using dot product with agent's facing direction
 * 3. Line-of-sight raycast to ensure no obstacles block the view
 *
 * The FOV test uses the cosine of half the FOV angle for efficient computation.
 * A target is visible if it's within range, within the vision cone, and has clear LOS.
 *
 * @param a Pointer to the perceiving agent.
 * @param target_x World X coordinate of the target.
 * @param target_y World Y coordinate of the target.
 * @param fov_deg Field of view angle in degrees (full cone angle).
 * @param max_dist Maximum vision distance.
 * @param out_dist Optional output parameter for computed distance to target.
 * @return 1 if target is visible, 0 otherwise.
 */
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

/**
 * @brief Processes all pending sound events for an agent's hearing.
 *
 * Iterates through the global sound event buffer and checks if each event
 * is within hearing range of the agent. For events the agent can hear:
 * - Increases the agent's threat level
 * - Updates the agent's last seen position to the player's current position
 * - Sets the last seen memory timer
 *
 * This function should be called each frame after sound emission but before
 * threat decay processing.
 *
 * @param a Pointer to the agent whose hearing is being processed.
 * @param player_x Current player X position (used for last seen position).
 * @param player_y Current player Y position (used for last seen position).
 * @param hearing_threat Amount of threat to add per heard sound event.
 * @param last_seen_memory_sec Duration to remember the last seen position.
 * @return Number of sound events that contributed to the agent's perception.
 */
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

/**
 * @brief Updates an agent's perception state for the current frame.
 *
 * Performs complete perception processing for one agent including:
 * 1. Vision processing: checks if player is visible and updates threat/last seen
 * 2. Threat decay: reduces threat level over time when not actively perceiving
 * 3. Memory management: ages out old last-seen position information
 *
 * This function should be called once per frame for each agent that needs
 * perception processing.
 *
 * @param a Pointer to the agent to update.
 * @param dt Time elapsed since last update in seconds.
 * @param player_x Current player world X position.
 * @param player_y Current player world Y position.
 * @param fov_deg Agent's field of view in degrees.
 * @param max_dist Maximum vision distance.
 * @param visible_threat_per_sec Threat accumulation rate when player is visible.
 * @param decay_per_sec Threat decay rate when player is not perceived.
 * @param last_seen_memory_sec How long to remember last seen position.
 */
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

/**
 * @brief Broadcasts an alert from one agent to nearby agents.
 *
 * When an agent becomes alerted, this function propagates the alert to other
 * agents within the specified radius. Alerted agents receive:
 * - Minimum threat level elevation to the baseline amount
 * - Copy of the source agent's last seen position and memory
 * - Reset of their last seen memory timer
 *
 * This enables group coordination and realistic AI behavior where agents
 * can alert each other to threats.
 *
 * @param agents Array of all agents in the scene.
 * @param agent_count Total number of agents in the array.
 * @param source_index Index of the agent initiating the broadcast.
 * @param radius Broadcast radius - agents beyond this distance are unaffected.
 * @param baseline_threat Minimum threat level to set on alerted agents.
 * @param last_seen_memory_sec Memory duration for the shared last seen position.
 */
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
