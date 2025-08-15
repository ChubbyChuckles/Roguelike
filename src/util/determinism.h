/* determinism.h - Phase M4.2/M4.3 deterministic hash & replay helpers */
#ifndef ROGUE_UTIL_DETERMINISM_H
#define ROGUE_UTIL_DETERMINISM_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* FNV-1a 64-bit hash helper */
uint64_t rogue_fnv1a64(const void* data, size_t len, uint64_t seed);

/* Compute a deterministic hash summarizing a sequence of RogueDamageEvent entries. */
struct RogueDamageEvent; /* forward */
uint64_t rogue_damage_events_hash(const struct RogueDamageEvent* ev, int count);

/* Serialize events to a simple text golden master format (one per line). */
int rogue_damage_events_write_text(const char* path, const struct RogueDamageEvent* ev, int count);
/* Load events from text; returns number loaded (<=max_out). */
int rogue_damage_events_load_text(const char* path, struct RogueDamageEvent* out, int max_out);

#ifdef __cplusplus
}
#endif
#endif
