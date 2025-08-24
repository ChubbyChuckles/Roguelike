/* Vendor System Phase 0 (0.3,0.5): Item value model & tests support.
 * Formula: base_value * slot_factor(category) * rarity_multiplier * (1 + p/(p+20)) * (0.4 +
 * 0.6*durability_fraction) Clamped to [1,100000000]. Diminishing returns on affix power via
 * p/(p+20).
 */
#ifndef ROGUE_ECON_VALUE_H
#define ROGUE_ECON_VALUE_H

#include "../loot/loot_item_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int rogue_econ_item_value(int def_index, int rarity, int affix_power_raw,
                              float durability_fraction);
    int rogue_econ_rarity_multiplier(int rarity);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ECON_VALUE_H */
