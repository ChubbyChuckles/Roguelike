/* Phase 8: Perpetual Scaling Layer
 * Provides continuous micro-node progression with sublinear aggregate growth.
 * Roadmap 8.1–8.5 implementation.
 */
#ifndef ROGUE_PROGRESSION_PERPETUAL_H
#define ROGUE_PROGRESSION_PERPETUAL_H
#ifdef __cplusplus
extern "C" {
#endif

int rogue_perpetual_init(void);
void rogue_perpetual_shutdown(void);
int rogue_perpetual_micro_nodes_allowed(int level);
int rogue_perpetual_micro_nodes_spent(void);
int rogue_perpetual_spend_node(int level);

double rogue_perpetual_raw_power(void);
double rogue_perpetual_level_scalar(int level);
void rogue_perpetual_set_global_coeff(double coeff);
double rogue_perpetual_get_global_coeff(void);
double rogue_perpetual_inflation_adjust(double median_ttk_delta);
double rogue_perpetual_effective_power(int level);
void rogue_perpetual_reset(void);

#ifdef __cplusplus
}
#endif
#endif
