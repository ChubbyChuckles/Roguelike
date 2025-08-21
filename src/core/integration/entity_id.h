#ifndef ROGUE_ENTITY_ID_H
#define ROGUE_ENTITY_ID_H
/* Unified Entity ID System (Phase 4.1)
 * ID Format: PREFIX_xxxxxxxx where PREFIX in {PLR, ENM, ITM, WLD}
 * Binary layout (64-bit):
 * [ 8 bits type ][ 48 bits sequence ][ 8 bits checksum ]
 * Type: 0=Player,1=Enemy,2=Item,3=World, 4+=Reserved
 * Sequence: monotonically increasing per type (wrap guarded)
 * Checksum: simple xor of bytes (for quick validation, not cryptographic)
 */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RogueEntityType {
    ROGUE_ENTITY_PLAYER = 0,
    ROGUE_ENTITY_ENEMY  = 1,
    ROGUE_ENTITY_ITEM   = 2,
    ROGUE_ENTITY_WORLD  = 3,
    ROGUE_ENTITY_MAX_TYPE = 4
} RogueEntityType;

/* Public opaque 64-bit ID */
typedef uint64_t RogueEntityId;

/* Generate a new ID for given type; returns 0 on failure (exhausted) */
RogueEntityId rogue_entity_id_generate(RogueEntityType type);

/* Decode helpers */
RogueEntityType rogue_entity_id_type(RogueEntityId id);
uint64_t rogue_entity_id_sequence(RogueEntityId id);
uint8_t rogue_entity_id_checksum(RogueEntityId id);

/* Validation (checksum + type range + nonzero) */
bool rogue_entity_id_validate(RogueEntityId id);

/* Lookup & tracking (Phase 4.1.3 / 4.1.5) */
int rogue_entity_register(RogueEntityId id, void* ptr);
void* rogue_entity_lookup(RogueEntityId id);
int rogue_entity_release(RogueEntityId id);

/* Persistence hooks (Phase 4.1.4): serialize/deserialize single id */
int rogue_entity_id_serialize(RogueEntityId id, char* buf, int buf_sz); /* returns bytes written or -1 */
int rogue_entity_id_parse(const char* str, RogueEntityId* out_id);      /* returns 0 on success */

/* Debug tools (Phase 4.1.6) */
void rogue_entity_dump_stats(void);

#ifdef __cplusplus
}
#endif
#endif
