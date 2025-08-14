/* Equipment stat aggregation (14.2 initial) */
#ifndef ROGUE_EQUIPMENT_STATS_H
#define ROGUE_EQUIPMENT_STATS_H

#include "entities/player.h"

/* Recalculate player stats contributed by equipped items & affixes.
 * Current implementation:
 *  - Weapon base damage currently handled in combat code; here we adjust dexterity via agility affixes.
 *  - Armor base_armor not yet surfaced in player struct; placeholder for future defense stat.
 *  - Aggregates agility_flat affix values into temporary dex bonus (applied after base attributes).
 */
void rogue_equipment_apply_stat_bonuses(RoguePlayer* p);

#endif
