#include "world/world_gen.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

/* Helper to create a temporary pack directory with minimal files */
static void write_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    assert(f);
    fwrite(text, 1, strlen(text), f);
    fclose(f);
}

int main(void)
{
    /* create temp directory (platform-specific). We'll use relative path 'temp_pack_phase13' */
#ifdef _WIN32
    _mkdir("temp_pack_phase13");
#else
    mkdir("temp_pack_phase13", 0700);
#endif
    write_file("temp_pack_phase13/pack.meta", "schema_version=1\n");
    /* Minimal biome descriptor file (CFG format adhered to by existing parser assumptions) */
    write_file("temp_pack_phase13/example.biome.cfg", "name=Example\n"
                                                      "tile_weight_GRASS=1.0\n"
                                                      "vegetation_density=0.2\n"
                                                      "decoration_density=0.1\n"
                                                      "ambient_color=100,120,140\n"
                                                      "music_track=calm_theme\n"
                                                      "allow_structures=1\n"
                                                      "allow_weather=1\n");

    char err[256];
    RogueDescriptorPackMeta meta;
    int r = rogue_pack_load_dir("temp_pack_phase13", 1, &meta, err, sizeof(err));
    assert(r == ROGUE_PACK_LOAD_OK);
    assert(meta.schema_version == 1);
    assert(rogue_pack_active_schema_version() == 1);

    /* Validate active pack */
    assert(rogue_pack_validate_active() == 1);

    /* Summary string */
    char summary[128];
    rogue_pack_summary(summary, sizeof(summary));
    assert(strstr(summary, "schema=1"));

    /* Attempt invalid load (missing meta) to ensure rejection */
#ifdef _WIN32
    _mkdir("temp_pack_phase13_bad");
#else
    mkdir("temp_pack_phase13_bad", 0700);
#endif
    write_file("temp_pack_phase13_bad/example2.biome.cfg", "name=Bad\n");
    int rbad = rogue_pack_load_dir("temp_pack_phase13_bad", 1, NULL, err, sizeof(err));
    assert(rbad != ROGUE_PACK_LOAD_OK);

    /* Hot reload with a second valid biome file (still schema 1) */
    write_file("temp_pack_phase13/second.biome.cfg", "name=Second\n"
                                                     "tile_weight_GRASS=1.0\n"
                                                     "vegetation_density=0.1\n"
                                                     "decoration_density=0.05\n"
                                                     "ambient_color=90,90,90\n"
                                                     "music_track=second_theme\n"
                                                     "allow_structures=1\n"
                                                     "allow_weather=1\n");
    r = rogue_pack_load_dir("temp_pack_phase13", 1, &meta, err, sizeof(err));
    assert(r == ROGUE_PACK_LOAD_OK);
    assert(rogue_pack_active_schema_version() == 1);

    rogue_pack_clear();
    assert(rogue_pack_active_schema_version() == 0);
    printf("phase13 modding tests passed\n");
    return 0;
}
