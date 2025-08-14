/* Global drop rate configuration layer (9.1) */
#ifndef ROGUE_LOOT_DROP_RATES_H
#define ROGUE_LOOT_DROP_RATES_H

/* Multiplicative scalar applied to final roll count (before clamping). */
void rogue_drop_rates_set_global(float scalar);
float rogue_drop_rates_get_global(void);

/* Optional per-category (RogueItemCategory) scalar; defaults 1.0f. */
void rogue_drop_rates_set_category(int category, float scalar);
float rogue_drop_rates_get_category(int category);

/* Reset to defaults (all 1.0). */
void rogue_drop_rates_reset(void);

#endif
