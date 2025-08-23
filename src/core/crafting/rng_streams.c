#include "core/crafting/rng_streams.h"
#include <string.h>

/**
 * @file rng_streams.c
 * @brief Small deterministic RNG stream manager used by crafting subsystems.
 *
 * The module provides per-stream RNG state seeded from a session seed and
 * helper functions to derive chunk- and player-specific seeds. The RNG
 * algorithm uses a compact xorshift32 step and ensures non-zero states.
 */

/** @brief Session-wide seed used as the base for stream seeding. */
static unsigned int g_session_seed = 0u;

/** @brief Internal PRNG state per stream. */
static unsigned int g_stream_state[ROGUE_RNG_STREAM_COUNT];

/**
 * @brief Initialize RNG streams from a session seed.
 *
 * Each stream state is mixed from the provided session_seed and the stream
 * index using a large odd multiplier. A non-zero state is enforced to avoid
 * degenerative xorshift results.
 *
 * @param session_seed Seed value shared for the session.
 */
void rogue_rng_streams_seed(unsigned int session_seed)
{
    g_session_seed = session_seed;
    for (unsigned int i = 0; i < ROGUE_RNG_STREAM_COUNT; i++)
    {
        /* simple mix: session xor stream id * large odd */
        unsigned int s = session_seed ^ (0x9E3779B9u * (i + 1));
        if (s == 0)
            s = 0xA341316Cu; /* avoid zero state */
        g_stream_state[i] = s;
    }
}

/**
 * @brief xorshift32 PRNG step used internally.
 *
 * Ensures returned value is non-zero by substituting a constant when the
 * transform yields zero.
 */
static unsigned int xorshift32(unsigned int x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x ? x : 0xA341316C; /* keep non-zero */
}

/**
 * @brief Advance and return the next RNG value for a stream.
 *
 * @param stream Stream id (RogueRngStream enum value).
 * @return Next 32-bit RNG value for the stream, or 0 on invalid stream id.
 */
unsigned int rogue_rng_next(RogueRngStream stream)
{
    if (stream < 0 || stream >= ROGUE_RNG_STREAM_COUNT)
        return 0;
    unsigned int s = g_stream_state[stream];
    s = xorshift32(s);
    g_stream_state[stream] = s;
    return s;
}

/**
 * @brief Derive a seed from session, chunk coordinates and player level.
 *
 * The function mixes the inputs with Jenkins-like operations to produce a
 * deterministic seed suitable for seeding per-chunk RNG streams. A
 * non-zero fallback constant is returned if the mix results in zero.
 *
 * @param session_seed Session seed used as base.
 * @param world_chunk Chunk index (or coordinate packed value) to mix in.
 * @param player_level Player level to include in the mix.
 * @param stream_id Stream identifier mixed into the derived seed.
 * @return Derived non-zero 32-bit seed.
 */
unsigned int rogue_seed_derive(unsigned int session_seed, unsigned int world_chunk,
                               unsigned int player_level, unsigned int stream_id)
{
    /* mix with Jenkins-like operations */
    unsigned int h = session_seed ^ (stream_id * 0x9E3779B9u);
    h += world_chunk * 0x7FEB352Du;
    h ^= (h >> 15);
    h += player_level * 0x846CA68Bu;
    h ^= (h << 7);
    h ^= (h >> 9);
    return h ? h : 0xC2B2AE35u;
}
