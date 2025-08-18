/* Equipment Phase 16.5: Budget Analyzer Report Generator
   Computes aggregate affix budget usage statistics across active item instances:
     - count of items analyzed
     - average budget utilization ratio (total_affix_weight / max_budget)
     - max utilization ratio and item index
     - number of over‑budget items (should be 0 ideally)
     - histogram buckets (0-25%,25-50%,50-75%,75-90%,90-100%,>100%)
   Also exposes a JSON export for tooling/CI dashboards.
*/
#ifndef ROGUE_EQUIPMENT_BUDGET_ANALYZER_H
#define ROGUE_EQUIPMENT_BUDGET_ANALYZER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueBudgetReport {
    int item_count;
    int over_budget_count;
    float avg_utilization; /* average of ratios (0..1+) */
    float max_utilization; int max_item_index; /* index of item instance with highest ratio */
    int bucket_counts[6]; /* buckets: 0: <0.25,1:<0.5,2:<0.75,3:<0.9,4:<=1.0,5:>1.0 */
} RogueBudgetReport;

void rogue_budget_analyzer_reset(void); /* clears last computed report */
void rogue_budget_analyzer_run(RogueBudgetReport* out); /* scans active instances & populates (also caches internally) */
const RogueBudgetReport* rogue_budget_analyzer_last(void); /* pointer to last report or NULL */
int rogue_budget_analyzer_export_json(char* buf,int cap); /* returns bytes or -1 */

#ifdef __cplusplus
}
#endif
#endif
