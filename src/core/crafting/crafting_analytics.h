/* Crafting & Gathering Phase 11 Analytics & Telemetry
 * Implements Phase 11 roadmap tasks:
 * 11.1 Gathering session metrics (nodes/hour, rare proc rate)
 * 11.2 Craft outcome distribution (quality histogram, success/failure ratio)
 * 11.3 Enhancement risk realized vs expected variance monitor
 * 11.4 Material flow (acquire -> consume -> leftover) counters (simplified)
 * 11.5 JSON export for dashboards
 * 11.6 Drift alert (quality output deviation) simplified thresholding
 * 11.7 Tests verify variance & drift boundaries
 */
#ifndef ROGUE_CRAFTING_ANALYTICS_H
#define ROGUE_CRAFTING_ANALYTICS_H
#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize / reset analytics state */
    void rogue_craft_analytics_reset(void);

    /* Gathering session events */
    void rogue_craft_analytics_on_node_harvest(int node_def_index, int material_def_index, int qty,
                                               int rare_flag, unsigned int now_ms);

    /* Craft outcome event: crafted item with resulting material quality bias (0..100) and
     * success/failure flag (e.g., enhancement success) */
    void rogue_craft_analytics_on_craft(int recipe_index, int quality_out, int success_flag);

    /* Enhancement attempt outcome: expected fracture risk vs actual (success=1/0) */
    void rogue_craft_analytics_on_enhancement(float expected_risk, int success_flag);

    /* Material flow: acquisition & consumption tracking (def indexes refer to item defs mapped to
     * materials). */
    void rogue_craft_analytics_material_acquire(int material_def_index, int qty);
    void rogue_craft_analytics_material_consume(int material_def_index, int qty);

    /* Periodic drift evaluation: returns 1 if drift alert triggered (and internally latches) */
    int rogue_craft_analytics_check_quality_drift(void);

    /* Export JSON snapshot into provided buffer (returns chars written, excludes null). */
    int rogue_craft_analytics_export_json(char* buf, int cap);

    /* Metrics query helpers */
    float rogue_craft_analytics_nodes_per_hour(unsigned int now_ms);
    float rogue_craft_analytics_rare_proc_rate(void);
    float rogue_craft_analytics_enhance_risk_variance(
        void); /* difference between expected risk mean and observed failure mean */

#ifdef __cplusplus
}
#endif
#endif
