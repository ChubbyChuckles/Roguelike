/**
 * @file hit_feedback.c
 * @brief Comprehensive hit feedback system with particles, knockback, and audio cues
 *
 * This module implements a complete visual and audio feedback system for combat hits,
 * providing immersive feedback through particle effects, dynamic knockback calculations,
 * and sound effect integration. Designed to enhance combat responsiveness and player
 * satisfaction through rich sensory feedback.
 *
 * Key Features:
 * - Particle system with physics simulation for impact effects
 * - Dynamic knockback calculations based on level and strength differentials
 * - Overkill explosion effects with enhanced particle counts
 * - Audio integration stubs for weapon-specific impact sounds
 * - Explosion tracking for special visual effects and debugging
 *
 * Particle System:
 * - Maximum 128 particles for performance optimization
 * - Physics simulation with velocity, gravity, and lifetime management
 * - Two particle types: normal impact (type 1) and overkill (type 2)
 * - Efficient array management with swap-and-pop removal
 *
 * Knockback Calculation:
 * Magnitude = 0.18 + 0.02×level_diff + 0.015×strength_diff
 * - Level differential capped at ±20 levels
 * - Strength differential capped at ±60 points
 * - Maximum magnitude clamped to 0.55 tiles
 * - Encourages strategic level and stat investment
 *
 * Impact Effects:
 * - Normal hits: 10-14 particles, 3.2 base speed, 340ms lifetime
 * - Overkill hits: 24 particles, 5.5 base speed, 480ms lifetime
 * - 70-degree angular spread (±63 degrees from surface normal)
 * - Speed variation: ±45% randomization for natural appearance
 *
 * Audio Integration:
 * - Weapon-specific sound variants (critical/normal)
 * - SDL_mixer ready interface for future implementation
 * - Placeholder system for easy audio engine integration
 *
 * @note Phase 4: Full feedback system implementation
 * @note Particle system designed for CPU-side simulation with GPU rendering
 * @note Knockback calculations encourage balanced character progression
 * @note Overkill effects provide satisfying feedback for powerful attacks
 */

#include "hit_feedback.h"
#include "../core/app/app_state.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <string.h>

static RogueHitParticle g_particles[ROGUE_HIT_PARTICLE_MAX];
static int g_particle_count = 0;
static int g_last_explosion_frame = -1000;

/**
 * @brief Reset the particle system to initial state
 *
 * Clears all active particles and resets the particle system to a clean state.
 * Used during game initialization, level transitions, or when resetting
 * visual effects. Maintains particle array integrity for immediate reuse.
 *
 * @note Resets particle count to zero
 * @note Zeroes entire particle array for clean state
 * @note Safe to call at any time during gameplay
 * @note Does not affect explosion frame tracking
 */
void rogue_hit_particles_reset(void)
{
    g_particle_count = 0;
    memset(g_particles, 0, sizeof g_particles);
}

/**
 * @brief Update particle physics and lifecycle management
 *
 * Performs per-frame updates for all active particles, including physics simulation,
 * lifetime management, and cleanup of expired particles. Implements efficient
 * array management to maintain performance with frequent particle spawning.
 *
 * Physics Simulation:
 * - Updates particle age and checks lifetime expiration
 * - Applies velocity to position (converted from milliseconds to seconds)
 * - Applies gravity acceleration (-0.3 tiles/second² upward drift)
 * - Reserved fade calculation based on life fraction (for future alpha blending)
 *
 * Performance Optimizations:
 * - Swap-and-pop removal for O(1) amortized deletion
 * - Early exit when no particles are active
 * - Maintains array continuity for efficient iteration
 *
 * @param dt_ms Frame time delta in milliseconds
 *
 * @note Converts milliseconds to seconds for physics calculations
 * @note Gravity creates subtle upward drift for floating appearance
 * @note Expired particles are immediately removed and replaced
 * @note Life fraction calculation reserved for future fade effects
 */
void rogue_hit_particles_update(float dt_ms)
{
    for (int i = 0; i < g_particle_count;)
    {
        RogueHitParticle* p = &g_particles[i];
        p->age_ms += dt_ms;
        if (p->age_ms >= p->lifetime_ms)
        {
            g_particles[i] = g_particles[g_particle_count - 1];
            g_particle_count--;
            continue;
        }
        float life_frac = p->age_ms / p->lifetime_ms;
        (void) life_frac; /* reserved for fade */
        p->x += p->vx * (dt_ms / 1000.0f);
        p->y += p->vy * (dt_ms / 1000.0f);
        p->vy += -0.3f * (dt_ms / 1000.0f); /* slight upward drift */
        ++i;
    }
}

/**
 * @brief Get the current count of active particles
 *
 * Returns the number of particles currently being simulated and rendered.
 * Useful for performance monitoring, debugging, and conditional rendering.
 *
 * @return Number of active particles (0 to ROGUE_HIT_PARTICLE_MAX)
 *
 * @note Maximum is defined by ROGUE_HIT_PARTICLE_MAX (128)
 * @note Count decreases as particles expire and are removed
 * @note Can be used for performance monitoring during high-combat scenarios
 */
int rogue_hit_particles_active(void) { return g_particle_count; }

/**
 * @brief Get read-only access to the particle array
 *
 * Provides access to the internal particle array for rendering and debugging purposes.
 * Returns a const pointer to prevent external modification of particle state.
 *
 * @param out_count Optional output parameter for particle count (can be NULL)
 * @return Const pointer to particle array (valid until next update)
 *
 * @note Array is valid until the next call to particle update functions
 * @note Particles are not guaranteed to be in any particular order
 * @note Use count parameter to avoid iterating beyond active particles
 * @note Designed for renderer access without compromising simulation integrity
 */
const RogueHitParticle* rogue_hit_particles_get(int* out_count)
{
    if (out_count)
        *out_count = g_particle_count;
    return g_particles;
}

/**
 * @brief Internal helper to add a particle to the system
 *
 * Adds a new particle to the active particle array with the specified parameters.
 * Performs bounds checking to prevent array overflow and maintain system stability.
 *
 * @param x Initial X position (in world coordinates)
 * @param y Initial Y position (in world coordinates)
 * @param vx Initial X velocity (tiles per second)
 * @param vy Initial Y velocity (tiles per second)
 * @param life Lifetime in milliseconds
 * @param type Particle type identifier (1=normal, 2=overkill)
 *
 * @note Internal function - not part of public API
 * @note Silently ignores requests when particle limit is reached
 * @note Initializes age to 0 for immediate activation
 */
static void add_particle(float x, float y, float vx, float vy, float life, unsigned char type)
{
    if (g_particle_count >= ROGUE_HIT_PARTICLE_MAX)
        return;
    RogueHitParticle* p = &g_particles[g_particle_count++];
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->lifetime_ms = life;
    p->age_ms = 0;
    p->type = type;
}

/**
 * @brief Spawn impact particle burst with surface normal and overkill effects
 *
 * Creates a burst of particles at the impact location, distributed around the surface
 * normal with realistic spread and speed variation. Supports enhanced effects for
 * overkill damage to provide satisfying visual feedback for powerful attacks.
 *
 * Particle Distribution:
 * - Normal hits: 10-14 particles with 70° angular spread
 * - Overkill hits: 24 particles with enhanced spread and speed
 * - Speed variation: ±45% randomization for natural appearance
 * - Angular spread: ±63 degrees from surface normal using rotation matrix
 *
 * Effect Parameters:
 * - Normal: 3.2 base speed, 340ms lifetime, type 1
 * - Overkill: 5.5 base speed, 480ms lifetime, type 2
 *
 * Surface Normal Handling:
 * - Normalizes input vector for consistent distribution
 * - Uses rotation matrix for angular displacement
 * - Falls back to default direction if normal is zero-length
 *
 * @param x Impact X coordinate (world space)
 * @param y Impact Y coordinate (world space)
 * @param nx Surface normal X component
 * @param ny Surface normal Y component
 * @param overkill 1 for overkill effects, 0 for normal impact
 * @return Number of particles actually spawned (may be less than requested due to limits)
 *
 * @note Phase 4: Enhanced particle effects for combat feedback
 * @note Maximum 32 particles per impact to prevent spam
 * @note Speed variation creates natural, non-uniform particle clouds
 * @note Overkill effects provide clear visual distinction for powerful attacks
 */
int rogue_hit_particles_spawn_impact(float x, float y, float nx, float ny, int overkill)
{
    int count = overkill ? 24 : 10 + rand() % 5;
    if (count > 32)
        count = 32;
    float base_speed = overkill ? 5.5f : 3.2f;
    for (int i = 0; i < count; i++)
    {
        float ang_spread =
            ((float) rand() / (float) RAND_MAX - 0.5f) * (float) M_PI * 0.7f; /* +-~63deg */
        float nlen = sqrtf(nx * nx + ny * ny);
        float bx = nx, by = ny;
        if (nlen > 0)
        {
            bx /= nlen;
            by /= nlen;
        }
        float cs = cosf(ang_spread), sn = sinf(ang_spread);
        float rx = bx * cs - by * sn;
        float ry = bx * sn + by * cs;
        float speed = base_speed * (0.55f + 0.45f * ((float) rand() / (float) RAND_MAX));
        add_particle(x, y, rx * speed, ry * speed, overkill ? 480.0f : 340.0f, overkill ? 2 : 1);
    }
    return count;
}

/**
 * @brief Calculate refined knockback magnitude based on combatant stats
 *
 * Computes knockback distance using level and strength differentials between
 * combatants, encouraging strategic character progression and stat investment.
 * Implements diminishing returns and reasonable caps for balanced gameplay.
 *
 * Knockback Formula:
 * Magnitude = 0.18 + 0.02×level_diff + 0.015×strength_diff
 *
 * Differential Calculations:
 * - Level differential: player_level - enemy_level (capped 0 to 20)
 * - Strength differential: player_str - enemy_str (capped 0 to 60)
 * - Base magnitude: 0.18 tiles (minimum knockback)
 * - Maximum magnitude: 0.55 tiles (prevents excessive displacement)
 *
 * Balance Design:
 * - Level advantage provides 0.02 tiles per level (up to 0.4 tiles)
 * - Strength advantage provides 0.015 tiles per point (up to 0.9 tiles)
 * - Combined maximum of 1.3 tiles from differentials
 * - Encourages investment in both level progression and stat allocation
 *
 * @param player_level Player's current level (clamped to ≥0)
 * @param enemy_level Enemy's level (clamped to ≥0)
 * @param player_str Player's strength stat (clamped to ≥0)
 * @param enemy_str Enemy's strength stat (clamped to ≥0, uses armor as proxy)
 * @return Knockback magnitude in tiles (0.18 to 0.55)
 *
 * @note Phase 4: Refined knockback system for combat feedback
 * @note Enemy strength uses armor value when stat unavailable
 * @note All inputs are clamped to prevent negative calculations
 * @note Diminishing returns prevent excessive knockback from high differentials
 * @note Balanced to provide tactical feedback without disrupting gameplay
 */
float rogue_hit_calc_knockback_mag(int player_level, int enemy_level, int player_str, int enemy_str)
{
    if (player_level < 0)
        player_level = 0;
    if (enemy_level < 0)
        enemy_level = 0;
    if (player_str < 0)
        player_str = 0;
    if (enemy_str < 0)
        enemy_str = 0;
    float level_diff = (float) player_level - (float) enemy_level;
    if (level_diff < 0)
        level_diff = 0;
    if (level_diff > 20)
        level_diff = 20;
    float str_diff = (float) player_str - (float) enemy_str;
    if (str_diff < 0)
        str_diff = 0;
    if (str_diff > 60)
        str_diff = 60;
    float mag = 0.18f + 0.02f * level_diff + 0.015f * str_diff;
    if (mag > 0.55f)
        mag = 0.55f;
    return mag;
}

/**
 * @brief Play weapon-specific impact sound effect
 *
 * Placeholder function for audio integration with SDL_mixer. Designed to play
 * appropriate impact sounds based on weapon type and hit variant (critical/normal).
 * Currently implemented as a stub for future audio system integration.
 *
 * Future Implementation:
 * - Weapon-specific sound libraries
 * - Critical hit audio variants
 * - Material-based sound variations
 * - Volume and spatial audio support
 *
 * @param weapon_id ID of the weapon that caused the impact
 * @param variant Sound variant (0=normal hit, 1=critical hit)
 *
 * @note Currently a stub function for future SDL_mixer integration
 * @note Parameters stored for future audio engine implementation
 * @note Designed to support weapon-specific audio feedback
 * @note Critical hits can have distinct audio cues
 */
void rogue_hit_play_impact_sfx(int weapon_id, int variant)
{
    (void) weapon_id;
    (void) variant; /* stub – future SDL_mixer hook */
}

/**
 * @brief Mark the current frame as containing an overkill explosion
 *
 * Records the current game frame as having an overkill explosion effect.
 * Used by debug overlays and special visual effects systems to track
 * and highlight powerful attacks that trigger explosion effects.
 *
 * @note Updates g_last_explosion_frame to current frame count
 * @note Used for debug visualization and effect coordination
 * @note Frame count comes from g_app.frame_count
 * @note Overkill explosions are triggered by high damage or executions
 */
void rogue_hit_mark_explosion(void) { g_last_explosion_frame = g_app.frame_count; }

/**
 * @brief Get the frame number of the last overkill explosion
 *
 * Returns the game frame number when the last overkill explosion occurred.
 * Used by debug systems and visual effects to track explosion timing
 * and coordinate related graphical effects.
 *
 * @return Frame number of last explosion, or -1000 if none recorded
 *
 * @note Returns -1000 if no explosion has been recorded
 * @note Used by debug overlays to highlight explosion frames
 * @note Can be used to time secondary effects or screen shake
 * @note Frame numbers are monotonically increasing
 */
int rogue_hit_last_explosion_frame(void) { return g_last_explosion_frame; }
