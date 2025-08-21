/* Equipment stat aggregation (14.2 initial) */
#ifndef ROGUE_EQUIPMENT_STATS_H
#define ROGUE_EQUIPMENT_STATS_H

#include "entities/player.h"

/* Recalculate player stats contributed by equipped items & affixes.
 * Phase 2 layering model: this function no longer mutates the player struct directly; instead it
 * populates g_player_stat_cache.affix_* fields (strength/dexterity/vitality/intelligence +
 * armor_flat) by summing affix flat values across all equipped item instances. Base weapon damage
 * and armor base_armor remain estimated in stat_cache.c derived computations. Future phases will
 * extend this pass for implicits, gems, set bonuses, runewords, and conditional buffs feeding
 * distinct cache layers.
 */
void rogue_equipment_apply_stat_bonuses(RoguePlayer* p);

#endif
