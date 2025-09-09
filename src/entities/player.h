/**
 * @file player.h  
 * @brief Player entity definition and management system.
 *
 * Defines the comprehensive player character structure containing all stats,
 * resources, combat properties, crowd control states, and progression data.
 * The player system includes complex mechanics like action points, multiple
 * resource types, combat stances, crowd control effects, and advanced
 * combat features like guard/parry systems and lock-on targeting.
 *
 * @author [Your Name]
 * @date September 2025  
 * @version 1.0
 */

/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ROGUE_ENTITIES_PLAYER_H
#define ROGUE_ENTITIES_PLAYER_H

#include "entity.h"

/**
 * @brief Comprehensive player character structure.
 *
 * Contains all data needed to represent the player character including
 * base entity properties, core stats, derived stats, multiple resource
 * pools, combat mechanics, crowd control states, equipment, and 
 * progression data. This structure serves as the central hub for all
 * player-related game systems.
 */
typedef struct RoguePlayer
{
    RogueEntity base; ///< Base entity properties (position, movement, etc.)
    
    /* Team and faction system */
    unsigned char team_id; ///< Team/faction ID for friendly-fire filtering (0=player faction)
    
    /* Core health and mana */
    int health;     ///< Current health points
    int max_health; ///< Maximum health points (derived from stats)
    int mana;       ///< Current mana points  
    int max_mana;   ///< Maximum mana points (derived from stats)
    
    /* Action Point system */
    int action_points;     ///< Current action point pool
    int max_action_points; ///< Maximum action point capacity
    
    /* Animation and facing */
    int facing;      ///< Facing direction (0=down, 1=left, 2=right, 3=up)
    float anim_time; ///< Current animation time
    int anim_frame;  ///< Current animation frame index
    
    /* Character progression */
    int level;                        ///< Current character level
    int xp;                          ///< Experience points toward next level
    int xp_to_next;                  ///< XP needed to reach next level
    unsigned long long xp_total_accum; ///< Total lifetime XP accumulated (64-bit for overflow safety)
    
    /* Core attribute stats */
    int strength;     ///< Strength attribute (affects physical damage, carry capacity)
    int dexterity;    ///< Dexterity attribute (affects accuracy, crit chance)
    int vitality;     ///< Vitality attribute (affects health, stamina)
    int intelligence; ///< Intelligence attribute (affects mana, spell damage)
    
    /* Critical hit system */
    int crit_chance;  ///< Additional flat critical hit chance percentage (0-100)
    int crit_damage;  ///< Critical damage bonus percentage over 100 (e.g., 50 = 1.5x damage)
    int crit_rating;  ///< Critical hit rating (converted to crit chance)
    
    /* Advanced combat ratings */
    int haste_rating;     ///< Haste rating (affects attack and cast speed)
    int avoidance_rating; ///< Avoidance rating (affects passive evasion chance)
    
    /* Damage mitigation and penetration */
    int armor;           ///< Flat physical damage mitigation
    int resist_physical; ///< Physical damage resistance percentage (0-90)
    int resist_fire;     ///< Fire damage resistance percentage (0-90)
    int resist_frost;    ///< Frost damage resistance percentage (0-90)
    int resist_arcane;   ///< Arcane damage resistance percentage (0-90)
    int resist_bleed;    ///< Bleed damage resistance (placeholder)
    int resist_poison;   ///< Poison damage resistance (placeholder)
    int pen_flat;        ///< Flat armor penetration value
    int pen_percent;     ///< Percentage armor penetration (0-100)
    
    /* Guard and poise system */
    float guard_meter;     ///< Current guard meter (depletes while blocking)
    float guard_meter_max; ///< Maximum guard meter capacity
    float poise;           ///< Current poise (stability buffer against stagger)
    float poise_max;       ///< Maximum poise capacity
    
    /* Encumbrance system */
    float encumbrance;          ///< Current total equipped weight
    float encumbrance_capacity; ///< Weight capacity before overload penalties
    int encumbrance_tier;       ///< Encumbrance tier (0=light, 1=medium, 2=heavy, 3=overloaded)
    
    /* Guard and parry mechanics */
    int guarding;                  ///< 1 if currently holding guard stance
    float guard_active_time_ms;    ///< Time spent in current guard state
    float perfect_guard_window_ms; ///< Time window for perfect guard (configurable)
    float poise_regen_delay_ms;    ///< Delay before poise regeneration begins after damage
    
    /* Hit reactions and invulnerability frames */
    int reaction_type;       ///< Current reaction (0=none, 1=flinch, 2=stagger, 3=knockdown, 4=launch)
    float reaction_timer_ms; ///< Remaining time locked in current reaction
    float iframes_ms;        ///< Remaining invulnerability frame duration
    
    /* Crowd control effects */
    float cc_stun_ms;   ///< Remaining stun duration (cannot move or attack)
    float cc_root_ms;   ///< Remaining root duration (cannot move, can attack)
    float cc_slow_ms;   ///< Remaining slow effect duration
    float cc_slow_pct;  ///< Movement speed reduction fraction while slowed (0-1)
    float cc_disarm_ms; ///< Remaining disarm duration (cannot attack, can move)
    
    /* Reaction cancel system and directional influence */
    float reaction_total_ms;      ///< Original full duration of current reaction
    int reaction_canceled_early;  ///< 1 if player canceled reaction early
    float reaction_di_accum_x;    ///< Accumulated directional influence X offset
    float reaction_di_accum_y;    ///< Accumulated directional influence Y offset
    float reaction_di_max;        ///< Maximum directional influence radius for current reaction
    
    /* Lock-on targeting system */
    unsigned char lock_on_active;     ///< 1 if currently locked onto a target
    int lock_on_target_index;         ///< Index of currently targeted enemy
    float lock_on_radius;             ///< Target acquisition radius in tiles
    float lock_on_switch_cooldown_ms; ///< Cooldown to prevent rapid target switching
    
    /* Riposte system */
    float riposte_ms; ///< Time window after successful parry to perform riposte attack
    
    /* Weapon and combat stance system */
    int equipped_weapon_id; ///< ID of currently equipped weapon (-1 if none)
    int combat_stance;      ///< Combat stance (0=balanced, 1=aggressive, 2=defensive)
    
    /* Weapon infusion system */
    int weapon_infusion; ///< Weapon infusion type (0=none, 1=fire, 2=frost, 3=arcane, 4=bleed, 5=poison)
    
    /* Extended resource pools */
    int stamina;     ///< Current stamina (for melee actions, separate from AP)
    int max_stamina; ///< Maximum stamina capacity
    int energy;      ///< Current energy (rogue-like resource)
    int max_energy;  ///< Maximum energy capacity
    int heat;        ///< Current heat level (builds with certain skills)
    int max_heat;    ///< Maximum heat capacity before overheating
    int focus;       ///< Current focus (caster concentration resource)
    int max_focus;   ///< Maximum focus capacity
    int combo_points; ///< Current combo points (0-5, mirrored from combat system)
} RoguePlayer;

/**
 * @brief Initialize a player structure with default values.
 *
 * Sets up initial player state with starting stats, resources, and
 * default values for all player systems. Should be called when
 * creating a new player character.
 *
 * @param p Pointer to the player structure to initialize
 */
void rogue_player_init(RoguePlayer* p);

/**
 * @brief Recalculate all derived player statistics.
 *
 * Updates derived stats like max health, max mana, damage bonuses,
 * and other computed values based on current base stats, equipment,
 * and temporary modifiers. Should be called whenever base stats
 * or equipment changes.
 *
 * @param p Pointer to the player whose stats should be recalculated
 */
void rogue_player_recalc_derived(RoguePlayer* p);
#endif
