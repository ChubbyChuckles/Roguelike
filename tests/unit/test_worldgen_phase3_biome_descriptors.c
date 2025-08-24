#include "../../src/world/world_gen_biome_desc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* VALID_CFG = "name=TemperateForest\n"
                               "music=forest_theme\n"
                               "vegetation_density=0.65\n"
                               "decoration_density=0.40\n"
                               "ambient_color=34,72,18\n"
                               "allow_structures=1\n"
                               "allow_weather=1\n"
                               "tile_grass=2.0\n"
                               "tile_forest=5.0\n"
                               "tile_water=0.3\n"
                               "tile_mountain=0.7\n"
                               "tile_swamp=0.2\n";

static const char* INVALID_CFG_NO_TILES = "name=Empty\nvegetation_density=0.2\n";

int main(void)
{
    RogueBiomeDescriptor d;
    char err[128];
    assert(rogue_biome_descriptor_parse_cfg(VALID_CFG, &d, err, sizeof err));
    assert(d.tile_weight_count > 0);
    /* weights normalized */
    double sum = 0;
    for (int i = 0; i < ROGUE_TILE_MAX; i++)
        sum += d.tile_weights[i];
    assert(sum > 0.99 && sum < 1.01);
    /* densities clamped */
    assert(d.vegetation_density >= 0 && d.vegetation_density <= 1);
    assert(d.decoration_density >= 0 && d.decoration_density <= 1);

    /* invalid: no tiles */
    assert(!rogue_biome_descriptor_parse_cfg(INVALID_CFG_NO_TILES, &d, err, sizeof err));

    /* Registry add & blend */
    RogueBiomeRegistry reg;
    rogue_biome_registry_init(&reg);
    int idx = rogue_biome_registry_add(&reg, &d);
    assert(idx == 0);
    RogueBiomeDescriptor d2 = d;
    strcpy(d2.name, "Variant");
    d2.tile_weights[ROGUE_TILE_WATER] += 0.5f; /* renormalize manually */
    double sum2 = 0;
    for (int i = 0; i < ROGUE_TILE_MAX; i++)
        sum2 += d2.tile_weights[i];
    for (int i = 0; i < ROGUE_TILE_MAX; i++)
        d2.tile_weights[i] /= (float) sum2;
    rogue_biome_registry_add(&reg, &d2);
    float blend[ROGUE_TILE_MAX];
    rogue_biome_blend_palettes(&reg.biomes[0], &reg.biomes[1], 0.5f, blend);
    double sum_blend = 0;
    for (int i = 0; i < ROGUE_TILE_MAX; i++)
        sum_blend += blend[i];
    assert(sum_blend > 0.99 && sum_blend < 1.01);
    rogue_biome_registry_free(&reg);
    printf("phase3 biome descriptor tests passed\n");
    return 0;
}
