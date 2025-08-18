/* Phase 1: Foundational Data & APIs
 * Implements chunk coordinate mapping, multi-channel deterministic RNG, and
 * a simple hashing utility for golden snapshot tests. */
#include "world/world_gen.h"
#include <string.h>

/* ---- Chunk utilities ---- */
RogueChunkCoord rogue_world_chunk_from_tile(int tx, int ty){
    RogueChunkCoord cc; cc.cx = (tx >= 0 ? tx : tx - (ROGUE_WORLD_CHUNK_SIZE-1)) / ROGUE_WORLD_CHUNK_SIZE;
    cc.cy = (ty >= 0 ? ty : ty - (ROGUE_WORLD_CHUNK_SIZE-1)) / ROGUE_WORLD_CHUNK_SIZE; return cc; }

void rogue_world_chunk_origin(RogueChunkCoord cc, int* out_tx, int* out_ty){
    if(out_tx) *out_tx = cc.cx * ROGUE_WORLD_CHUNK_SIZE;
    if(out_ty) *out_ty = cc.cy * ROGUE_WORLD_CHUNK_SIZE;
}

int rogue_world_chunk_index(RogueChunkCoord cc, int world_width_tiles){
    int chunks_per_row = (world_width_tiles + ROGUE_WORLD_CHUNK_SIZE - 1) / ROGUE_WORLD_CHUNK_SIZE;
    return cc.cy * chunks_per_row + cc.cx;
}

/* ---- RNG channels ---- */
static unsigned int rng_step(unsigned int st){ unsigned int x=st; x^=x<<13; x^=x>>17; x^=x<<5; return x?x:1u; }

void rogue_worldgen_context_init(RogueWorldGenContext* ctx, const RogueWorldGenConfig* cfg){
    if(!ctx) return; memset(ctx, 0, sizeof *ctx); ctx->cfg = cfg;
    /* Derive channel seeds by hashing base seed with distinct constants */
    unsigned int base = cfg?cfg->seed:1u;
    ctx->macro_rng.state = base ^ 0xA5A5A5A5u;
    ctx->biome_rng.state = (base + (cfg?cfg->biome_seed_offset:0u)) ^ 0x3C3C3C3Cu;
    ctx->micro_rng.state = base ^ 0xB4B4B4B4u ^ 0x9E3779B9u; /* mix golden ratio constant */
    if(!ctx->macro_rng.state) ctx->macro_rng.state = 1u;
    if(!ctx->biome_rng.state) ctx->biome_rng.state = 1u;
    if(!ctx->micro_rng.state) ctx->micro_rng.state = 1u;
}

void rogue_worldgen_context_shutdown(RogueWorldGenContext* ctx){ (void)ctx; }

unsigned int rogue_worldgen_rand_u32(RogueRngChannel* ch){ if(!ch) return 0; ch->state = rng_step(ch->state); return ch->state; }
double rogue_worldgen_rand_norm(RogueRngChannel* ch){ return (double)(rogue_worldgen_rand_u32(ch) & 0xffffffu) / (double)0xffffffu; }

/* ---- Hash utility ---- */
unsigned long long rogue_world_hash_tilemap(const RogueTileMap* map){
    if(!map || !map->tiles) return 0ULL; unsigned long long h=1469598103934665603ULL; /* FNV offset basis */
    const unsigned char* t = map->tiles; int count = map->width * map->height;
    int nonzero=0;
    for(int i=0;i<count;i++){ h ^= (unsigned long long)t[i]; h *= 1099511628211ULL; if(t[i]) nonzero=1; }
    if(!nonzero){ /* all zero tiles: mix a constant so hash != 0 sentinel after avalanche */
        h ^= 0xABCDEF1234567890ULL; h *= 1099511628211ULL;
    }
    /* Mix dimensions & a fixed tag to reduce collisions across different sizes */
    h ^= (unsigned long long)map->width; h *= 1099511628211ULL;
    h ^= (unsigned long long)map->height; h *= 1099511628211ULL;
    h ^= 0x57524C44464F554EULL; /* 'WRLDFOUN' tag */
    /* Final avalanche (xorshift64*) to reduce chance of small domain collisions */
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 29; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 32;
    if(h==0ULL){ h = 0xF1EA5EEDDEADBEEFULL; }
    return h;
}
