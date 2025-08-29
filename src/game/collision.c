/**
 * @file collision.c
 * @brief Implementation of collision resolution between gameplay entities.
 *
 * This module provides collision detection and resolution functions for maintaining
 * proper separation between gameplay actors such as enemies and the player.
 * Currently focuses on preventing overlap between enemies and the player character.
 *
 * Key features:
 * - Enemy-player separation enforcement
 * - Distance-based collision resolution
 * - Smooth pushing mechanics to maintain minimum distances
 */

#include "collision.h"
#include "../core/app/app_state.h"
#include "../entities/enemy.h"
#include "../entities/player.h"
#include <math.h>

/** @brief Minimum separation radius between enemy center and player center (tiles). */
static const float ROGUE_ENEMY_PLAYER_MIN_DIST = 0.30f;

/**
 * @brief Resolves collision between an enemy and the player by pushing the enemy away.
 *
 * This function maintains a minimum separation distance between enemies and the player.
 * When an enemy gets too close to the player (within ROGUE_ENEMY_PLAYER_MIN_DIST),
 * it calculates the necessary push vector to restore the minimum distance and applies
 * it to the enemy's position.
 *
 * The collision resolution uses a smooth, distance-based approach that:
 * - Calculates the vector from player to enemy
 * - Normalizes the vector for direction
 * - Computes the required push distance
 * - Applies the push to move the enemy away
 *
 * Special handling is included for edge cases where the enemy and player are
 * at exactly the same position (fallback to arbitrary direction).
 *
 * @param e Pointer to the enemy entity to resolve collision for
 *
 * @note This function does nothing if the enemy pointer is NULL or the enemy is not alive
 * @note The push is applied immediately to the enemy's position
 * @note No collision resolution is performed if the enemy is already at safe distance
 */
void rogue_collision_resolve_enemy_player(struct RogueEnemy* e)
{
    if (!e || !e->alive)
        return;
    float dx = e->base.pos.x - g_app.player.base.pos.x;
    float dy = e->base.pos.y - g_app.player.base.pos.y;
    float d2 = dx * dx + dy * dy;
    float min2 = ROGUE_ENEMY_PLAYER_MIN_DIST * ROGUE_ENEMY_PLAYER_MIN_DIST;
    if (d2 < min2)
    {
        float d = (float) sqrt(d2);
        if (d < 1e-5f)
        {
            dx = 1.0f;
            dy = 0.0f;
            d = 1.0f;
        }
        float push = (ROGUE_ENEMY_PLAYER_MIN_DIST - d);
        dx /= d;
        dy /= d;
        e->base.pos.x += dx * push;
        e->base.pos.y += dy * push;
    }
}
