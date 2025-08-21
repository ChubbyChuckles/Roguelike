/* loot_security.h - Phase 22 (Security / Cheat Resistance) primitives
 *
 * Features implemented (Roadmap 22.1–22.3):
 * 22.1 Hash-based verification of loot rolls
 * 22.2 Obfuscated seed mixing helper (opt-in)
 * 22.3 Basic anti-tamper detection for content config files (hash snapshot + verify)
 */
#ifndef ROGUE_LOOT_SECURITY_H
#define ROGUE_LOOT_SECURITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute a stable 32-bit verification hash for a loot roll. Pass the table index used,
 * the seed value *before* the roll (caller responsible for capturing), the number of drops
 * produced, and arrays of item definition indices, quantities, and optional rarities (may be NULL).
 * Returns FNV1a-32 hash across all parameters. Deterministic across platforms. */
uint32_t rogue_loot_roll_hash(int table_index, unsigned int seed_before,
                              int drop_count, const int* item_def_indices,
                              const int* quantities, const int* rarities);

/* Enable/disable seed obfuscation mode (default disabled for deterministic regression tests).
 * When enabled (and adopted by caller integration), callers can pre-mix the raw seed via
 * rogue_loot_obfuscate_seed before passing to the public roll APIs to make trivial prediction
 * of future rolls harder in client-authoritative environments. */
void rogue_loot_security_enable_obfuscation(int enable);
int  rogue_loot_security_obfuscation_enabled(void);

/* Obfuscate a seed with a salt (e.g., session-specific or server-provided secret fragment).
 * Provides a reversible (to parties with salt) mixed value using bit rotations + multiplicative hashing. */
unsigned int rogue_loot_obfuscate_seed(unsigned int raw_seed, unsigned int salt);

/* Snapshot combined FNV1a hash of a set of config file paths (e.g., item defs + loot tables).
 * Returns 0 on success, <0 on error (file open failure or allocation). Stores internal baseline. */
int rogue_loot_security_snapshot_files(const char* const* paths, int count);

/* Recompute combined hash for provided paths and compare with last snapshot.
 * Returns 0 if unchanged, 1 if hash differs (tamper / modification), <0 on IO error. */
int rogue_loot_security_verify_files(const char* const* paths, int count);

/* Retrieve last stored combined files hash (0 if none yet). */
uint32_t rogue_loot_security_last_files_hash(void);

/* Phase 22.4: Server authoritative mode toggle */
void rogue_loot_security_set_server_mode(int enabled);
int  rogue_loot_security_server_mode(void);
/* Verify a client-provided loot roll hash (0=match, 1=mismatch). */
int rogue_loot_server_verify(int table_index, unsigned int seed_before,
                             int drop_count, const int* item_def_indices,
                             const int* quantities, const int* rarities,
                             uint32_t reported_hash);

/* Phase 22.5: Anomaly detector (weight / high-rarity spike logging) */
void rogue_loot_anomaly_reset(void);
void rogue_loot_anomaly_config(int window_size, float baseline_high_frac, float spike_mult, int per_roll_high_threshold);
void rogue_loot_anomaly_record(int drop_count, const int* rarities);
int  rogue_loot_anomaly_flag(void); /* 1 if anomaly detected since last reset */

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_LOOT_SECURITY_H */
