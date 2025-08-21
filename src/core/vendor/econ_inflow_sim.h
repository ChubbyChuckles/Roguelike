/* Vendor System Phase 0 (0.4): Economic inflow baseline simulation.
 * Provides deterministic baseline estimates for expected material & item economic value inflow per hour
 * at a reference player performance (kills per minute) and average drops per kill.
 * Initial simple linear model; future phases can plug in richer loot table probability analytics.
 */
#ifndef ROGUE_ECON_INFLOW_SIM_H
#define ROGUE_ECON_INFLOW_SIM_H

#ifdef __cplusplus
extern "C" { 
#endif

typedef struct RogueEconInflowResult {
    double hours;
    int kills_per_min;
    double avg_item_drops_per_kill; /* includes non-material equip/consumable items */
    double avg_material_drops_per_kill; /* explicit material node / salvage adjusted estimate */
    double expected_items; /* count of non-material items */
    double expected_materials; /* count of material items */
    double expected_item_value; /* sum base economic value of non-material items */
    double expected_material_value; /* sum base economic value of materials */
    double expected_total_value; /* items + materials */
} RogueEconInflowResult;

/* Compute baseline inflow; requires item defs & material catalog loaded.
 * Parameters:
 *  kills_per_min >=0
 *  hours >0
 *  avg_item_drops_per_kill, avg_material_drops_per_kill >=0 (fractional allowed)
 * Returns 0 on failure (invalid input), 1 on success.
 */
int rogue_econ_inflow_baseline(int kills_per_min, double hours,
    double avg_item_drops_per_kill, double avg_material_drops_per_kill,
    RogueEconInflowResult* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ECON_INFLOW_SIM_H */
