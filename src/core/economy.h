/* Basic Economy Systems (10.4, 10.5, 10.6 partial) */
#ifndef ROGUE_ECONOMY_H
#define ROGUE_ECONOMY_H

#include "core/vendor.h"
#include "core/loot_item_defs.h"

void rogue_econ_reset(void);
int rogue_econ_gold(void);
int rogue_econ_add_gold(int amount); /* negative allowed to spend; clamps at 0 */

/* Reputation 0..100 (placeholder linear scaling). */
void rogue_econ_set_reputation(int rep);
int rogue_econ_get_reputation(void);

/* Compute buy price with reputation discount (simple: price * (1 - rep*0.002) floor 50% of base). */
int rogue_econ_buy_price(const RogueVendorItem* v);
/* Compute sell value (bounded: 10%..70% of vendor base price ladder). */
int rogue_econ_sell_value(const RogueVendorItem* v);

/* Attempt purchase: deduct gold, return 0 success, <0 failure (insufficient funds). */
int rogue_econ_try_buy(const RogueVendorItem* v);

/* Attempt sell: add gold (uses computed sell value). Assumes item exists, returns credited amount. */
int rogue_econ_sell(const RogueVendorItem* v);

/* Currency sinks (10.4): repair & reroll cost helpers */
int rogue_econ_repair_cost(int durability_missing, int rarity);
int rogue_econ_reroll_affix_cost(int rarity);

#endif
