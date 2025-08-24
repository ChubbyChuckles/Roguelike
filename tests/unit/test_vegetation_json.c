#include "../../src/core/vegetation/vegetation_internal.h"
#include "../../src/core/vegetation/vegetation_json.h"
#include <assert.h>
#include <stdio.h>

static const char* kPlantsJson =
    "[\n"
    " { \"id\": \"grass1\", \"image\": \"assets/tiles/vegetation.png\", \"tx\": 1, \"ty\": 2, "
    "\"rarity\": 2 },\n"
    " { \"id\": \"grass2\", \"image\": \"assets/tiles/vegetation.png\", \"tx\": 3, \"ty\": 2, "
    "\"rarity\": 1 }\n"
    "]\n";

static const char* kTreesJson =
    "[\n"
    " { \"id\": \"oak\", \"image\": \"assets/tiles/trees.png\", \"tx\": 5, \"ty\": 6, \"tx2\": 6, "
    "\"ty2\": 7, \"rarity\": 3, \"canopy_radius\": 2 },\n"
    " { \"id\": \"pine\", \"image\": \"assets/tiles/trees.png\", \"tx\": 7, \"ty\": 6, \"rarity\": "
    "1, \"canopy_radius\": 3 }\n"
    "]\n";

int main(void)
{
    g_def_count = 0; /* reset registry */
    int before = g_def_count;
    char err[128];
    int a = rogue_vegetation_load_plants_json_text(kPlantsJson, err, sizeof err);
    assert(a == 2);
    int b = rogue_vegetation_load_trees_json_text(kTreesJson, err, sizeof err);
    assert(b == 2);
    assert(g_def_count == before + 4);
    /* Validate fields */
    assert(g_defs[0].is_tree == 0);
    assert(g_defs[1].is_tree == 0);
    assert(g_defs[2].is_tree == 1);
    assert(g_defs[3].is_tree == 1);
    assert(g_defs[2].canopy_radius >= 1);
    assert(g_defs[3].canopy_radius >= 1);
    return 0;
}
