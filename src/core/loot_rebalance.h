/* Tooling Phase 21.4: Batch rarity rebalance helper.
   Computes multiplicative scale factors target/current per rarity (0..4) and JSON export. */
#ifndef ROGUE_LOOT_REBALANCE_H
#define ROGUE_LOOT_REBALANCE_H
int rogue_rarity_rebalance_scales(const int current[5], const int target[5], float out_scale[5]);
int rogue_rarity_rebalance_export_json(const float scales[5], char* buf, int cap);
#endif
