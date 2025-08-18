#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_chunk_mapping(){
    RogueChunkCoord c = rogue_world_chunk_from_tile(0,0); assert(c.cx==0 && c.cy==0);
    c = rogue_world_chunk_from_tile(31,31); assert(c.cx==0 && c.cy==0);
    c = rogue_world_chunk_from_tile(32,0); assert(c.cx==1 && c.cy==0);
    c = rogue_world_chunk_from_tile(33,33); assert(c.cx==1 && c.cy==1);
    int ox, oy; rogue_world_chunk_origin(c,&ox,&oy); assert(ox==32 && oy==32);
}

static void test_rng_channel_independence(){
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(12345u, 0, 0);
    RogueWorldGenContext ctx; rogue_worldgen_context_init(&ctx,&cfg);
    /* Draw first few numbers from each channel and ensure they differ in pattern */
    unsigned int macro_seq[4], biome_seq[4], micro_seq[4];
    for(int i=0;i<4;i++){ macro_seq[i]=rogue_worldgen_rand_u32(&ctx.macro_rng); biome_seq[i]=rogue_worldgen_rand_u32(&ctx.biome_rng); micro_seq[i]=rogue_worldgen_rand_u32(&ctx.micro_rng);}    
    int identical_positions_mb=0, identical_positions_mm=0; for(int i=0;i<4;i++){ if(macro_seq[i]==biome_seq[i]) identical_positions_mb++; if(macro_seq[i]==micro_seq[i]) identical_positions_mm++; }
    assert(identical_positions_mb < 4 && identical_positions_mm < 4); /* not perfectly correlated */
    /* Re-init and ensure reproducibility */
    RogueWorldGenContext ctx2; rogue_worldgen_context_init(&ctx2,&cfg);
    for(int i=0;i<4;i++){ assert(macro_seq[i]==rogue_worldgen_rand_u32(&ctx2.macro_rng)); }
    rogue_worldgen_context_shutdown(&ctx); rogue_worldgen_context_shutdown(&ctx2);
}

static void test_hash(){
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(999u,0,0);
    cfg.width=16; cfg.height=16;
    RogueTileMap map; memset(&map,0,sizeof(map));
    if(!rogue_tilemap_init(&map,cfg.width,cfg.height)) { printf("[worldgen_phase1] tilemap_init failed %d x %d\n", cfg.width, cfg.height); assert(0); }
    printf("[worldgen_phase1] map dims %d x %d\n", map.width, map.height);
    for(int y=0;y<map.height;y++) for(int x=0;x<map.width;x++){ unsigned char v=(unsigned char)((x+y)%5); map.tiles[y*map.width+x] = v; }
    int nonzero=0; for(int i=0;i<map.width*map.height;i++) if(map.tiles[i]) { nonzero=1; break; }
    printf("[worldgen_phase1] tile pattern nonzero=%d first=%u\n", nonzero, (unsigned)map.tiles[0]);
    unsigned long long h1 = rogue_world_hash_tilemap(&map); printf("[worldgen_phase1] hash h1=%llu\n", h1);
    unsigned long long h2 = rogue_world_hash_tilemap(&map); printf("[worldgen_phase1] hash h2=%llu\n", h2); fflush(stdout); if(h1!=h2){ printf("hash mismatch stability\n"); assert(0); }
    map.tiles[0] ^= 1; unsigned long long h3 = rogue_world_hash_tilemap(&map); printf("[worldgen_phase1] hash h3(after mutate)=%llu\n", h3); fflush(stdout); if(h3==h1){ printf("hash did not change after mutation\n"); assert(0); }
    rogue_tilemap_free(&map);
}

int main(void){
    printf("[worldgen_phase1] start\n");
    test_chunk_mapping();
    printf("[worldgen_phase1] chunk mapping ok\n");
    test_rng_channel_independence();
    printf("[worldgen_phase1] rng ok\n");
    test_hash();
    printf("[worldgen_phase1] hash ok\n");
    printf("phase1 foundation tests passed\n");
    return 0;
}
