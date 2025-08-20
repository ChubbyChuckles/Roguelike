/* Minimal mastery XP track for Phase 6.1
 * Provides per-skill mastery XP accumulation and simple rank calculation.
 * This is intentionally small and well-tested; it can be expanded later.
 */
#ifndef ROGUE_PROGRESSION_MASTERY_H
#define ROGUE_PROGRESSION_MASTERY_H

#include <stdint.h>

int rogue_progression_mastery_init(void);
void rogue_progression_mastery_shutdown(void);

/* Add XP (can be fractional in callers, stored as double internally). Returns new total XP. */
double rogue_progression_mastery_add_xp(int skill_id, double xp);

/* Get mastery XP total for a skill. */
double rogue_progression_mastery_get_xp(int skill_id);

/* Get integer mastery rank for a skill (0..). */
int rogue_progression_mastery_get_rank(int skill_id);

/* Helper: threshold for next rank for given current rank. */
double rogue_progression_mastery_threshold_for_rank(int rank);

#endif /* ROGUE_PROGRESSION_MASTERY_H */
/* Progression Mastery System (Phase 6)
 * Per-skill mastery XP & rank tracking with optional decay / plateau mechanic
 * Minor passive ring unlock points awarded for distinct skills reaching a threshold rank.
 */
#ifndef ROGUE_PROGRESSION_MASTERY_H
#define ROGUE_PROGRESSION_MASTERY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" { 
#endif

/* Initialize mastery system for up to max_skills skill ids (0..max_skills-1). */
int rogue_mastery_init(int max_skills, int enable_decay);
void rogue_mastery_shutdown(void);

/* Add mastery XP for a skill (skill usage event). Timestamp used for decay bookkeeping. */
int rogue_mastery_add_xp(int skill_id, unsigned int xp, unsigned int timestamp_ms);

/* Advance time (ms) to process decay; call periodically (e.g., frame step). */
void rogue_mastery_update(unsigned int elapsed_ms);

/* Query rank & xp. */
unsigned short rogue_mastery_rank(int skill_id);
unsigned long long rogue_mastery_xp(int skill_id);
unsigned long long rogue_mastery_xp_to_next(int skill_id);

/* Count of distinct skills that have reached the minor ring threshold rank (unlock points). */
int rogue_mastery_minor_ring_points(void);

/* Toggle decay (1=enabled,0=disabled). */
void rogue_mastery_set_decay(int enabled);

#ifdef __cplusplus
}
#endif
#endif
