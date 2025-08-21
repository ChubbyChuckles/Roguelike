#include "core/vendor/vendor_rng.h"
#include <string.h>

uint32_t rogue_vendor_fnv1a32(const void* data, int len)
{
    const unsigned char* p = (const unsigned char*) data;
    uint32_t h = 2166136261u;
    for (int i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t hash_str_ci(const char* s)
{
    if (!s)
        return 0;
    uint32_t h = 2166136261u;
    while (*s)
    {
        unsigned char c = (unsigned char) *s++;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char) (c - 'A' + 'a');
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

uint32_t rogue_vendor_seed_compose(uint32_t world_seed, const char* vendor_id,
                                   uint32_t refresh_epoch, RogueVendorRngStream stream)
{
    uint32_t h = world_seed ^ (refresh_epoch * 0x9E3779B9u) ^ (uint32_t) stream * 0x85EBCA6Bu;
    h ^= hash_str_ci(vendor_id);
    /* Final avalanche */
    h ^= (h >> 16);
    h *= 0x7FEB352Du;
    h ^= (h >> 15);
    h *= 0x846CA68Bu;
    h ^= (h >> 16);
    if (h == 0)
        h = 0xA136AAADu; /* avoid zero */
    return h;
}

uint32_t rogue_vendor_snapshot_hash(const int* def_indices, const int* rarities, const int* prices,
                                    int count, uint32_t world_seed, const char* vendor_id,
                                    uint32_t refresh_epoch, uint32_t price_mod_hash)
{
    uint32_t h = 2166136261u; /* FNV1a start */
    h ^= (world_seed & 0xFFu);
    h *= 16777619u;
    h ^= ((world_seed >> 8) & 0xFFu);
    h *= 16777619u;
    h ^= ((world_seed >> 16) & 0xFFu);
    h *= 16777619u;
    h ^= ((world_seed >> 24) & 0xFFu);
    h *= 16777619u;
    h ^= (refresh_epoch & 0xFFu);
    h *= 16777619u;
    h ^= ((refresh_epoch >> 8) & 0xFFu);
    h *= 16777619u;
    h ^= ((refresh_epoch >> 16) & 0xFFu);
    h *= 16777619u;
    h ^= ((refresh_epoch >> 24) & 0xFFu);
    h *= 16777619u;
    h ^= price_mod_hash;
    h *= 16777619u;
    if (vendor_id)
    {
        for (const char* s = vendor_id; *s; ++s)
        {
            unsigned char c = (unsigned char) *s;
            if (c >= 'A' && c <= 'Z')
                c = (unsigned char) (c - 'A' + 'a');
            h ^= c;
            h *= 16777619u;
        }
    }
    for (int i = 0; i < count; i++)
    {
        int vals[3];
        vals[0] = def_indices ? def_indices[i] : -1;
        vals[1] = rarities ? rarities[i] : 0;
        vals[2] = prices ? prices[i] : 0;
        h = rogue_vendor_fnv1a32(vals, sizeof vals);
    }
    /* Final mix */
    h ^= (h >> 13);
    h ^= (h >> 7);
    h ^= (h >> 17);
    return h;
}
