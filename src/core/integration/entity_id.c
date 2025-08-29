/**
 * @file entity_id.c
 * @brief Implementation of the unified entity ID system for the roguelike game.
 *
 * This module provides a robust entity identification system that generates unique,
 * type-safe, and validatable 64-bit entity IDs. The system supports multiple entity
 * types (Player, Enemy, Item, World) with per-type sequence numbering and integrity
 * checking via checksums.
 *
 * Key features:
 * - Type-safe ID generation with configurable entity types
 * - Monotonically increasing sequence numbers per type
 * - Built-in checksum validation for corruption detection
 * - Entity registration and lookup for pointer management
 * - Serialization/deserialization for persistence
 * - Memory leak detection and statistics
 *
 * ID Format (64-bit):
 * [8 bits type][48 bits sequence][8 bits checksum]
 *
 * @author Rogue Game Development Team
 * @date 2025
 * @version 1.0
 *
 * @note This implementation is part of Phase 4.1 of the roguelike game development.
 * @see entity_id.h for public API declarations
 */

#include "entity_id.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Sequence counters for each entity type.
 *
 * Maintains monotonically increasing counters for each entity type to ensure
 * unique sequence numbers within each type. Used during ID generation.
 */
static uint64_t s_seq[ROGUE_ENTITY_MAX_TYPE];

#define ENTITY_TRACK_CAP 8192

/**
 * @brief Internal structure for tracking entity ID to pointer mappings.
 *
 * Used to maintain a registry of active entities for lookup and memory
 * management purposes. Provides O(n) lookup where n is the number of
 * tracked entities.
 */
typedef struct
{
    RogueEntityId id; /**< The entity ID being tracked */
    void* ptr;        /**< Pointer to the entity object */
} RogueEntityTrack;

/**
 * @brief Array of tracked entity mappings.
 *
 * Fixed-size array for entity registration and lookup. Uses linear search
 * for simplicity and acceptable performance with typical entity counts.
 */
static RogueEntityTrack s_track[ENTITY_TRACK_CAP];

/**
 * @brief Current number of tracked entities.
 *
 * Tracks the number of active entity registrations in the s_track array.
 * Used for bounds checking and iteration.
 */
static int s_track_count = 0;

/**
 * @brief Counter for potential memory leaks.
 *
 * Tracks entities that may have been leaked (not properly released).
 * Used for debugging and memory analysis.
 */
static int s_leaked = 0;

/**
 * @brief Calculates a simple 8-bit checksum for integrity validation.
 *
 * Computes an XOR-based checksum across all bytes of the 64-bit value.
 * Used for basic corruption detection in entity IDs, not for cryptographic
 * security purposes.
 *
 * @param v The 64-bit value to checksum
 * @return 8-bit checksum value
 *
 * @note This is a simple, fast checksum suitable for error detection but not security
 */
static uint8_t checksum64(uint64_t v)
{
    uint8_t c = 0;
    for (int i = 0; i < 8; i++)
    {
        c ^= (uint8_t) ((v >> (i * 8)) & 0xFF);
    }
    return c;
}

/**
 * @brief Generates a new unique entity ID for the specified type.
 *
 * Creates a new entity ID with the given type, incrementing the sequence
 * counter for that type. The ID includes a checksum for integrity validation.
 * Returns 0 if the type is invalid or the sequence counter would overflow.
 *
 * @param type The entity type for which to generate an ID
 * @return New entity ID, or 0 on failure (invalid type or sequence exhaustion)
 *
 * @note Sequence numbers are 48-bit, allowing ~281 trillion IDs per type
 * @note Thread-safety: Currently single-threaded; upgrade to atomics if needed
 */
RogueEntityId rogue_entity_id_generate(RogueEntityType type)
{
    if (type < 0 || type >= ROGUE_ENTITY_MAX_TYPE)
        return 0;
    uint64_t seq = ++s_seq[type];
    if (seq >= ((uint64_t) 1 << 48))
        return 0; /* exhausted */
    uint64_t raw = ((uint64_t) type << 56) | (seq << 8);
    uint8_t cs = checksum64(raw);
    raw |= (uint64_t) cs;
    return raw;
}

/**
 * @brief Extracts the entity type from an entity ID.
 *
 * Decodes the 8-bit type field from the most significant bits of the ID.
 *
 * @param id The entity ID to decode
 * @return The entity type encoded in the ID
 */
RogueEntityType rogue_entity_id_type(RogueEntityId id)
{
    return (RogueEntityType) ((id >> 56) & 0xFF);
}

/**
 * @brief Extracts the sequence number from an entity ID.
 *
 * Decodes the 48-bit sequence field from the middle bits of the ID.
 *
 * @param id The entity ID to decode
 * @return The sequence number encoded in the ID
 */
uint64_t rogue_entity_id_sequence(RogueEntityId id) { return (id >> 8) & 0xFFFFFFFFFFFFULL; }

/**
 * @brief Extracts the checksum from an entity ID.
 *
 * Decodes the 8-bit checksum field from the least significant bits of the ID.
 *
 * @param id The entity ID to decode
 * @return The checksum encoded in the ID
 */
uint8_t rogue_entity_id_checksum(RogueEntityId id) { return (uint8_t) (id & 0xFF); }

/**
 * @brief Validates the integrity of an entity ID.
 *
 * Performs comprehensive validation including:
 * - Non-zero ID check
 * - Valid entity type range
 * - Checksum verification for corruption detection
 *
 * @param id The entity ID to validate
 * @return true if the ID is valid, false otherwise
 *
 * @note This function provides basic integrity checking but is not cryptographically secure
 */
bool rogue_entity_id_validate(RogueEntityId id)
{
    if (id == 0)
        return false;
    RogueEntityType t = rogue_entity_id_type(id);
    if (t < 0 || t >= ROGUE_ENTITY_MAX_TYPE)
        return false;
    uint64_t base = id & ~0xFFULL;
    return checksum64(base) == rogue_entity_id_checksum(id);
}

/**
 * @brief Finds the index of a tracked entity in the tracking array.
 *
 * Performs linear search through the entity tracking array to locate
 * the specified entity ID. Returns the array index if found.
 *
 * @param id The entity ID to search for
 * @return Array index of the entity, or -1 if not found
 *
 * @note O(n) search complexity where n is the number of tracked entities
 */
static int track_find(RogueEntityId id)
{
    for (int i = 0; i < s_track_count; i++)
    {
        if (s_track[i].id == id)
            return i;
    }
    return -1;
}

/**
 * @brief Registers an entity for tracking and lookup.
 *
 * Associates an entity ID with a pointer for later retrieval via lookup.
 * Validates the ID and ensures it's not already registered. The tracking
 * array has a fixed capacity that cannot be exceeded.
 *
 * @param id The entity ID to register (must be valid)
 * @param ptr Pointer to the entity object (must not be NULL)
 * @return 0 on success, negative error code on failure:
 *         -1: invalid ID or NULL pointer
 *         -2: entity already registered
 *         -3: tracking capacity exceeded
 *
 * @note Registration is required before the entity can be looked up
 * @see rogue_entity_lookup() for retrieval
 * @see rogue_entity_release() for unregistration
 */
int rogue_entity_register(RogueEntityId id, void* ptr)
{
    if (!rogue_entity_id_validate(id) || !ptr)
        return -1;
    if (track_find(id) >= 0)
        return -2; /* already */
    if (s_track_count >= ENTITY_TRACK_CAP)
        return -3;
    s_track[s_track_count].id = id;
    s_track[s_track_count].ptr = ptr;
    s_track_count++;
    return 0;
}

/**
 * @brief Looks up an entity pointer by its ID.
 *
 * Retrieves the pointer associated with the given entity ID from the
 * tracking registry. Returns NULL if the entity is not registered.
 *
 * @param id The entity ID to look up
 * @return Pointer to the entity object, or NULL if not found
 *
 * @note The entity must have been previously registered
 * @see rogue_entity_register() for registration
 */
void* rogue_entity_lookup(RogueEntityId id)
{
    int i = track_find(id);
    if (i < 0)
        return NULL;
    return s_track[i].ptr;
}

/**
 * @brief Releases an entity from tracking.
 *
 * Removes the entity ID from the tracking registry, allowing the ID to be
 * reused and preventing further lookups. Uses swap-with-last optimization
 * for O(1) removal from the tracking array.
 *
 * @param id The entity ID to release
 * @return 0 on success, -1 if entity was not registered
 *
 * @note This function does not free the entity object itself
 * @note After release, lookups for this ID will return NULL
 */
int rogue_entity_release(RogueEntityId id)
{
    int i = track_find(id);
    if (i < 0)
        return -1; /* compact */
    s_track[i] = s_track[s_track_count - 1];
    s_track_count--;
    return 0;
}

/**
 * @brief Serializes an entity ID to a hexadecimal string.
 *
 * Converts the 64-bit entity ID to a 16-character uppercase hexadecimal
 * string representation suitable for persistence or debugging.
 *
 * @param id The entity ID to serialize
 * @param buf Output buffer to write the string (must not be NULL)
 * @param buf_sz Size of the output buffer (must be at least 17 bytes for null terminator)
 * @return Number of characters written (16), or -1 on error
 *
 * @note Output format is exactly 16 hexadecimal characters (no prefix)
 * @note Buffer must be large enough to hold the string plus null terminator
 */
int rogue_entity_id_serialize(RogueEntityId id, char* buf, int buf_sz)
{
    if (!buf || buf_sz < 20)
        return -1; /* hex */
    int n = snprintf(buf, buf_sz, "%016" PRIX64, (uint64_t) id);
    return (n > 0 && n < buf_sz) ? n : -1;
}

/**
 * @brief Parses a hexadecimal string into a 64-bit unsigned integer.
 *
 * Converts a string of hexadecimal characters to a uint64_t value.
 * Supports both uppercase and lowercase hex digits. Expects exactly
 * 16 hex characters for a complete 64-bit value.
 *
 * @param s Input string to parse (null-terminated)
 * @param out Output pointer for the parsed value
 * @return 0 on success, negative error code on failure:
 *         -1: invalid character encountered
 *         -2: too many characters (>16)
 *         -3: wrong number of characters (!=16)
 *
 * @note Only accepts valid hexadecimal characters [0-9a-fA-F]
 */
static int parse_hex64(const char* s, uint64_t* out)
{
    uint64_t v = 0;
    int count = 0;
    while (*s)
    {
        char c = *s++;
        int nyb;
        if (c >= '0' && c <= '9')
            nyb = c - '0';
        else if (c >= 'a' && c <= 'f')
            nyb = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            nyb = 10 + (c - 'A');
        else
            return -1;
        v = (v << 4) | (uint64_t) nyb;
        count++;
        if (count > 16)
            return -2;
    }
    if (count != 16)
        return -3;
    *out = v;
    return 0;
}

/**
 * @brief Parses an entity ID from a hexadecimal string.
 *
 * Converts a 16-character hexadecimal string back to an entity ID.
 * Validates the string format and performs integrity checking on the result.
 *
 * @param str Input string to parse (must be exactly 16 hex characters)
 * @param out_id Output pointer for the parsed entity ID
 * @return 0 on success, negative error code on failure:
 *         -1: NULL parameters
 *         -2: wrong string length (!=16)
 *         -3: invalid hexadecimal format
 *         -4: parsed ID fails validation
 *
 * @note String must be exactly 16 characters with no prefix or suffix
 * @note Performs full validation including checksum verification
 * @see rogue_entity_id_serialize() for the complementary serialization function
 */
int rogue_entity_id_parse(const char* str, RogueEntityId* out_id)
{
    if (!str || !out_id)
        return -1;
    if (strlen(str) != 16)
        return -2;
    uint64_t v = 0;
    if (parse_hex64(str, &v) != 0)
        return -3;
    if (!rogue_entity_id_validate(v))
        return -4;
    *out_id = v;
    return 0;
}

/**
 * @brief Dumps entity tracking statistics to stderr.
 *
 * Outputs current statistics about entity tracking including the number
 * of tracked entities and potential memory leaks. Useful for debugging
 * and monitoring entity lifecycle management.
 *
 * @note Output is sent to stderr for visibility in debug builds
 * @note Statistics include tracked count and leak detection counter
 */
void rogue_entity_dump_stats(void)
{
    fprintf(stderr, "ENTITY_ID stats: tracked=%d leaked=%d\n", s_track_count, s_leaked);
}
