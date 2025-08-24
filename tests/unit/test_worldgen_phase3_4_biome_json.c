#include "../../src/world/world_gen_biome_desc.h"
#include "../../src/world/world_gen_biome_json.h"
#include <assert.h>
#include <stdio.h>

static const char* kBiomesJson = "[\n"
                                 " {\n"
                                 "  \"name\": \"Plains\",\n"
                                 "  \"music\": \"light\",\n"
                                 "  \"vegetation_density\": 0.35,\n"
                                 "  \"decoration_density\": 0.15,\n"
                                 "  \"ambient_color\": [80,90,100],\n"
                                 "  \"allow_structures\": 1,\n"
                                 "  \"allow_weather\": 1,\n"
                                 "  \"tile_grass\": 4,\n"
                                 "  \"tile_forest\": 1\n"
                                 " },\n"
                                 " {\n"
                                 "  \"name\": \"Forest\",\n"
                                 "  \"music\": \"mood\",\n"
                                 "  \"vegetation_density\": 0.6,\n"
                                 "  \"decoration_density\": 0.4,\n"
                                 "  \"ambient_color\": [60,80,70],\n"
                                 "  \"allow_structures\": 1,\n"
                                 "  \"allow_weather\": 1,\n"
                                 "  \"tile_grass\": 1,\n"
                                 "  \"tile_forest\": 5\n"
                                 " }\n"
                                 "]\n";

static const char* kTransitionsJson = "{\n"
                                      " \"Plains\": [\"Forest\"],\n"
                                      " \"Forest\": [\"Plains\"]\n"
                                      "}\n";

static const char* kEncountersJson = "{\n"
                                     " \"Plains\": [\"wolf\", \"boar\"],\n"
                                     " \"Forest\": [\"bear\"]\n"
                                     "}\n";

int main(void)
{
    RogueBiomeRegistry reg;
    rogue_biome_registry_init(&reg);
    char err[128];
    int n = rogue_biome_registry_load_json_text(&reg, kBiomesJson, err, sizeof err);
    assert(n == 2);
    assert(reg.count == 2);
    /* Validate densities and tiles normalized */
    for (int i = 0; i < reg.count; i++)
    {
        const RogueBiomeDescriptor* d = &reg.biomes[i];
        assert(d->tile_weight_count >= 1);
        float sum = 0.0f;
        for (int t = 0; t < ROGUE_TILE_MAX; t++)
            sum += d->tile_weights[t];
        assert(sum > 0.99f && sum < 1.01f);
    }
    int ok = rogue_biome_registry_validate_balance(&reg, 0.0f, 1.0f, 0.0f, 1.0f, err, sizeof err);
    assert(ok == 1);
    unsigned char mat[16];
    int m = rogue_biome_build_transition_matrix(&reg, kTransitionsJson, mat, (int) sizeof mat, err,
                                                sizeof err);
    assert(m == 1);
    assert(mat[0 * reg.count + 1] == 1);
    assert(mat[1 * reg.count + 0] == 1);
    ok = rogue_biome_validate_encounter_tables(&reg, kEncountersJson, err, sizeof err);
    assert(ok == 1);
    rogue_biome_registry_free(&reg);
    return 0;
}
