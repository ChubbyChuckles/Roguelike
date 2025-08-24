#include "entity_id.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple atomic-like counters (single-threaded tests; upgrade to atomics later) */
static uint64_t s_seq[ROGUE_ENTITY_MAX_TYPE];

#define ENTITY_TRACK_CAP 8192
typedef struct
{
    RogueEntityId id;
    void* ptr;
} RogueEntityTrack;
static RogueEntityTrack s_track[ENTITY_TRACK_CAP];
static int s_track_count = 0;
static int s_leaked = 0;

static uint8_t checksum64(uint64_t v)
{
    uint8_t c = 0;
    for (int i = 0; i < 8; i++)
    {
        c ^= (uint8_t) ((v >> (i * 8)) & 0xFF);
    }
    return c;
}

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

RogueEntityType rogue_entity_id_type(RogueEntityId id)
{
    return (RogueEntityType) ((id >> 56) & 0xFF);
}
uint64_t rogue_entity_id_sequence(RogueEntityId id) { return (id >> 8) & 0xFFFFFFFFFFFFULL; }
uint8_t rogue_entity_id_checksum(RogueEntityId id) { return (uint8_t) (id & 0xFF); }

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

static int track_find(RogueEntityId id)
{
    for (int i = 0; i < s_track_count; i++)
    {
        if (s_track[i].id == id)
            return i;
    }
    return -1;
}

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

void* rogue_entity_lookup(RogueEntityId id)
{
    int i = track_find(id);
    if (i < 0)
        return NULL;
    return s_track[i].ptr;
}

int rogue_entity_release(RogueEntityId id)
{
    int i = track_find(id);
    if (i < 0)
        return -1; /* compact */
    s_track[i] = s_track[s_track_count - 1];
    s_track_count--;
    return 0;
}

int rogue_entity_id_serialize(RogueEntityId id, char* buf, int buf_sz)
{
    if (!buf || buf_sz < 20)
        return -1; /* hex */
    int n = snprintf(buf, buf_sz, "%016" PRIX64, (uint64_t) id);
    return (n > 0 && n < buf_sz) ? n : -1;
}

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

void rogue_entity_dump_stats(void)
{
    fprintf(stderr, "ENTITY_ID stats: tracked=%d leaked=%d\n", s_track_count, s_leaked);
}
