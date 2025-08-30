/**
 * @file determinism.c
 * @brief Phase M4.2/M4.3 deterministic hash & replay helpers for ensuring consistent game state.
 */

#include "determinism.h"
#include "../game/combat.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Computes FNV-1a 64-bit hash of arbitrary data.
 *
 * @param data Pointer to the data to hash.
 * @param len Length of the data in bytes.
 * @param seed Initial hash value (use 0 for default FNV offset basis).
 * @return The computed 64-bit hash value.
 */
uint64_t rogue_fnv1a64(const void* data, size_t len, uint64_t seed)
{
    const unsigned char* p = (const unsigned char*) data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++)
    {
        h ^= (uint64_t) p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/**
 * @brief Computes a hash of an array of damage events for determinism verification.
 *
 * @param ev Pointer to the array of RogueDamageEvent structures.
 * @param count Number of events in the array.
 * @return The computed hash value, or 0 if invalid parameters.
 */
uint64_t rogue_damage_events_hash(const struct RogueDamageEvent* ev, int count)
{
    if (!ev || count <= 0)
        return 0;
    uint64_t h = 0xcbf29ce484222325ULL;
    for (int i = 0; i < count; i++)
    {
        h = rogue_fnv1a64(&ev[i], sizeof(ev[i]), h);
    }
    return h;
}

/**
 * @brief Writes an array of damage events to a text file for replay/debugging purposes.
 *
 * @param path File path to write the events to.
 * @param ev Pointer to the array of RogueDamageEvent structures.
 * @param count Number of events to write.
 * @return 0 on success, -1 on failure.
 */
int rogue_damage_events_write_text(const char* path, const struct RogueDamageEvent* ev, int count)
{
    if (!path || !ev || count < 0)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return -1;
    for (int i = 0; i < count; i++)
    {
        fprintf(f, "%u,%u,%u,%d,%d,%d,%u\n", (unsigned) ev[i].attack_id,
                (unsigned) ev[i].damage_type, (unsigned) ev[i].crit, ev[i].raw_damage,
                ev[i].mitigated, ev[i].overkill, (unsigned) ev[i].execution);
    }
    fclose(f);
    return 0;
}

/**
 * @brief Loads damage events from a text file into an array.
 *
 * @param path File path to read the events from.
 * @param out Pointer to the output array of RogueDamageEvent structures.
 * @param max_out Maximum number of events that can be stored in the output array.
 * @return Number of events loaded on success, -1 on failure.
 */
int rogue_damage_events_load_text(const char* path, struct RogueDamageEvent* out, int max_out)
{
    if (!path || !out || max_out <= 0)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return -1;
    int n = 0;
    while (n < max_out)
    {
        unsigned attack_id = 0, dmg_type = 0, crit = 0, execution = 0;
        int raw = 0, mitig = 0, over = 0;
#if defined(_MSC_VER)
        int r = fscanf_s(f, "%u,%u,%u,%d,%d,%d,%u\n", &attack_id, &dmg_type, &crit, &raw, &mitig,
                         &over, &execution);
#else
        int r = fscanf(f, "%u,%u,%u,%d,%d,%d,%u\n", &attack_id, &dmg_type, &crit, &raw, &mitig,
                       &over, &execution);
#endif
        if (r != 7)
            break;
        /* Zero-init to ensure deterministic padding for memcmp across compilers/platforms */
        memset(&out[n], 0, sizeof out[n]);
        out[n].attack_id = (unsigned short) attack_id;
        out[n].damage_type = (unsigned char) dmg_type;
        out[n].crit = (unsigned char) crit;
        out[n].raw_damage = raw;
        out[n].mitigated = mitig;
        out[n].overkill = over;
        out[n].execution = (unsigned char) execution;
        n++;
    }
    fclose(f);
    return n;
}
