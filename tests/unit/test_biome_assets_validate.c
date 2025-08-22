#include "world/world_gen_biome_assets_validate.h"
#include <assert.h>

static char ok_json[] = "{\n"
                        " \"Garden\": [ { \"name\": \"GRASS\", \"image\": "
                        "\"../assets/biomes/garden/assets/tiles.png\", \"tx\": 0, \"ty\": 0 } ]\n"
                        "}\n";

static char bad_json[] = "{\n"
                         " \"Nowhere\": [ { \"name\": \"GRASS\", \"image\": "
                         "\"assets/biomes/nonexistent/tiles.png\", \"tx\": 0, \"ty\": 0 } ]\n"
                         "}\n";

int main(void)
{
    char err[128];
    int ok = rogue_biome_assets_validate_from_json(ok_json, err, sizeof err);
    assert(ok == 1);
    int ok2 = rogue_biome_assets_validate_from_json(bad_json, err, sizeof err);
    assert(ok2 == 0);
    (void) err;
    return 0;
}
