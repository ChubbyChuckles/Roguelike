#include "world/world_gen.h"
#include "world/world_gen_resource_json.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char* kResourcesJson =
    "[\n"
    " { \"id\": \"copper\", \"rarity\": 0, \"tool_tier\": 0, \"yield_min\": 1, \"yield_max\": 3, "
    "\"biomes\": [\"Plains\", \"Forest\"] },\n"
    " { \"id\": \"iron\", \"rarity\": 1, \"tool_tier\": 1, \"yield_min\": 2, \"yield_max\": 4, "
    "\"biomes\": [\"Mountain\"] }\n"
    "]\n";

int main(void)
{
    /* Clear registry and load */
    rogue_resource_clear_registry();
    char err[128];
    int n = rogue_resource_defs_load_json_text(kResourcesJson, err, sizeof err);
    assert(n == 2);
    assert(rogue_resource_registry_count() == 2);
    /* Simple world and placement sanity */
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 1234u;
    cfg.width = 32;
    cfg.height = 32;
    RogueWorldGenContext ctx;
    memset(&ctx, 0, sizeof ctx);
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap map;
    map.width = 32;
    map.height = 32;
    map.tiles = (unsigned char*) malloc((size_t) map.width * (size_t) map.height);
    assert(map.tiles != NULL);
    /* set a small mountain patch and plains elsewhere */
    for (int i = 0; i < map.width * map.height; i++)
        map.tiles[i] =
            (unsigned char) ((i % map.width) < 8 ? ROGUE_TILE_MOUNTAIN : ROGUE_TILE_GRASS);
    RogueResourceNodePlacement out[128];
    int placed = rogue_resource_generate(&cfg, &ctx, &map, out, 128, 32, 3, 3);
    assert(placed >= 1);
    int upgrades = rogue_resource_upgrade_count(out, placed);
    assert(upgrades >= 0);
    free(map.tiles);
    rogue_worldgen_context_shutdown(&ctx);
    return 0;
}
