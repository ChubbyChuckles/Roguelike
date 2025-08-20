/* Crafting & Gathering Phase 3 Material Quality & Refinement (3.1-3.5)
 * Provides per-material quality bucket ledger (0..100) and refinement API converting
 * lower-quality material units into higher-quality units with loss, failure, and
 * critical success paths. Also exposes average quality scalar for future crafting bias.
 */
#ifndef ROGUE_MATERIAL_REFINE_H
#define ROGUE_MATERIAL_REFINE_H
#ifdef __cplusplus
extern "C" { 
#endif

#include <stdint.h>

#ifndef ROGUE_MATERIAL_QUALITY_MAX
#define ROGUE_MATERIAL_QUALITY_MAX 100
#endif

/* Initialize / reset all quality buckets. */
void rogue_material_quality_reset(void);
/* Add units of a material at a specified quality (clamped 0..100). Returns 0 success. */
int rogue_material_quality_add(int material_def, int quality, int count);
/* Consume units exactly at a quality bucket; returns actually consumed or <0 on error. */
int rogue_material_quality_consume(int material_def, int quality, int count);
/* Total units (all qualities) for a material. */
int rogue_material_quality_total(int material_def);
/* Units at a particular quality. */
int rogue_material_quality_count(int material_def, int quality);
/* Average quality (0..100) across buckets or -1 if none. */
int rogue_material_quality_average(int material_def);
/* Bias scalar 0..1 (average / 100.0f) convenience for crafting systems. */
float rogue_material_quality_bias(int material_def);

/* Refinement operation: convert `consume_count` units from `from_quality` bucket into higher-quality units at `to_quality`.
 * Rules:
 *  - to_quality must be > from_quality.
 *  - Efficiency constant (70%) defines base produced units = floor(consume_count * 0.70).
 *  - Failure chance (10%): produced reduced to 25% of base (still consuming all input).
 *  - Critical success chance (5%): produced increased by +50% (rounded) and 20% of produced (rounded) escalates one extra quality tier (to_quality+1) if < max.
 *  - Production cannot be negative; min produced 0.
 *  - Updates internal buckets atomically relative to single-threaded game loop.
 * Returns 0 success, <0 error codes:
 *   -1 invalid args, -2 insufficient source, -3 no output (all lost to failure), -4 material index invalid.
 * On success writes produced units at target quality to *out_produced (excluding any overflowed extra tier units) and sets *out_crit flag (1 if critical success path taken).
 */
int rogue_material_refine(int material_def, int from_quality, int to_quality, int consume_count,
                          unsigned int* rng_state, int* out_produced, int* out_crit);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_MATERIAL_REFINE_H */
