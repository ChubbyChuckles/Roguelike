#include "core/crafting/rng_streams.h"
#include <string.h>

static unsigned int g_session_seed = 0u;
static unsigned int g_stream_state[ROGUE_RNG_STREAM_COUNT];

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

static unsigned int xorshift32(unsigned int x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x ? x : 0xA341316C; /* keep non-zero */
}

unsigned int rogue_rng_next(RogueRngStream stream)
{
    if (stream < 0 || stream >= ROGUE_RNG_STREAM_COUNT)
        return 0;
    unsigned int s = g_stream_state[stream];
    s = xorshift32(s);
    g_stream_state[stream] = s;
    return s;
}

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
