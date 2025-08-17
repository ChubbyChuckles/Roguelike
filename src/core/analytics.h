/** Simple analytics counters (Phase 7.7 persistence) */
#ifndef ROGUE_CORE_ANALYTICS_H
#define ROGUE_CORE_ANALYTICS_H
#ifdef __cplusplus
extern "C" {
#endif
void rogue_analytics_add_damage(unsigned int amount);
void rogue_analytics_add_gold(unsigned int amount);
#ifdef __cplusplus
}
#endif
#endif
