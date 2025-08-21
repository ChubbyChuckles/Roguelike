/* Full world generation orchestrator combining phases 2-10 as described in the roadmap.
 * This keeps layering order strict and uses the public phase APIs introduced during refactor.
 * If any step fails, the partially built map is freed and 0 is returned.
 */
#include "world/tilemap.h"
#include "world/world_gen.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Internal helper: free map & return 0 */
static int fail(RogueTileMap* m)
{
    if (m)
        rogue_tilemap_free(m);
    return 0;
}

int rogue_world_generate_full(RogueTileMap* out_map, const RogueWorldGenConfig* cfg)
{
    if (!out_map || !cfg)
        return 0;
    if (!rogue_tilemap_init(out_map, cfg->width, cfg->height))
        return 0;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, cfg);

    /* Phase 2: Macro layout + biomes */
    if (!rogue_world_generate_macro_layout(cfg, &ctx, out_map, NULL, NULL))
        return fail(out_map);

    /* Phase 4: Local terrain & caves & detailing */
    if (!rogue_world_generate_local_terrain(cfg, &ctx, out_map))
        return fail(out_map);
    if (!rogue_world_generate_caves_layer(cfg, &ctx, out_map))
        return fail(out_map);
    /* Lava pockets + ore veins (targets chosen heuristically) */
    rogue_world_place_lava_and_liquids(cfg, &ctx, out_map, 8);
    rogue_world_place_ore_veins(cfg, &ctx, out_map, 24, 18);

    /* Phase 5: River refinement & erosion */
    rogue_world_refine_rivers(cfg, &ctx, out_map);
    rogue_world_apply_erosion(cfg, &ctx, out_map, 1, 1);
    rogue_world_mark_bridge_hints(cfg, out_map, 2, 5);

    /* Phase 6: Structures & POIs */
    RogueStructurePlacement structures[128];
    int structure_count = rogue_world_place_structures(cfg, &ctx, out_map, structures, 128, 3);
    rogue_world_place_dungeon_entrances(cfg, &ctx, out_map, structures, structure_count,
                                        structure_count / 2 + 1);

    /* Phase 7: Single dungeon carve (example) under a reserved region if map big enough */
    if (cfg->width >= 220 && cfg->height >= 220)
    {
        RogueDungeonGraph graph;
        memset(&graph, 0, sizeof graph);
        if (rogue_dungeon_generate_graph(&ctx, 28, 25, &graph))
        {
            /* carve near center */
            int ox = (cfg->width - 220) / 2;
            int oy = (cfg->height - 220) / 2;
            if (ox < 0)
                ox = 0;
            if (oy < 0)
                oy = 0;
            rogue_dungeon_carve_into_map(&ctx, out_map, &graph, ox, oy, 220, 220);
            rogue_dungeon_place_keys_and_locks(&ctx, out_map, &graph);
            rogue_dungeon_place_traps_and_secrets(&ctx, out_map, &graph, 12, 0.12);
            rogue_dungeon_free_graph(&graph);
        }
    }

    /* Phase 8: Spawn tables (register a small baseline set) */
    rogue_spawn_clear_tables();
    {
        RogueSpawnTable plains = {
            ROGUE_TILE_GRASS, 35, {{"wolf", 40, 15}, {"boar", 30, 10}, {"stag", 20, 5}}, 3};
        rogue_spawn_register_table(&plains);
        RogueSpawnTable forest = {
            ROGUE_TILE_FOREST,
            50,
            {{"wolf", 30, 10}, {"bear", 25, 15}, {"sprite", 20, 12}, {"ent", 15, 8}},
            4};
        rogue_spawn_register_table(&forest);
        RogueSpawnTable swamp = {
            ROGUE_TILE_SWAMP, 60, {{"slime", 40, 15}, {"leech", 25, 10}, {"hag", 15, 8}}, 3};
        rogue_spawn_register_table(&swamp);
        RogueSpawnTable snow = {
            ROGUE_TILE_SNOW, 40, {{"wolf_white", 40, 15}, {"yeti", 15, 10}, {"owl", 20, 6}}, 3};
        rogue_spawn_register_table(&snow);
        RogueSpawnTable dungeon = {
            ROGUE_TILE_DUNGEON_FLOOR,
            55,
            {{"skeleton", 40, 15}, {"zombie", 30, 10}, {"lich_acolyte", 10, 5}},
            3};
        rogue_spawn_register_table(&dungeon);
    }
    RogueSpawnDensityMap dm;
    memset(&dm, 0, sizeof dm);
    rogue_spawn_build_density(out_map, &dm);
    /* Example hub suppression at player start (0,0) -> adjust later when player spawn defined */
    rogue_spawn_apply_hub_suppression(&dm, 4, 4, 6);
    /* (We intentionally do not sample spawns now; runtime systems will.) */
    rogue_spawn_free_density(&dm);

    /* Phase 9: Resource nodes baseline */
    rogue_resource_clear_registry();
    RogueResourceNodeDesc ore = {"iron_ore", 0, 0, 2, 5, (1u << ROGUE_BIOME_MOUNTAIN_BIOME)};
    rogue_resource_register(&ore);
    RogueResourceNodeDesc herb = {
        "herb", 0, 0, 1, 3, (1u << ROGUE_BIOME_PLAINS) | (1u << ROGUE_BIOME_FOREST_BIOME)};
    rogue_resource_register(&herb);
    RogueResourceNodeDesc crystal = {"crystal", 2, 1, 1, 2, (1u << ROGUE_BIOME_SNOW_BIOME)};
    rogue_resource_register(&crystal);
    RogueResourceNodePlacement resources[256];
    rogue_resource_generate(cfg, &ctx, out_map, resources, 256, 64, 4, 6);
    /* (We could stamp resource nodes into a separate layer; for now we leave placements external.)
     */

    /* Phase 10: Weather patterns baseline (a few simple) */
    rogue_weather_clear_registry();
    {
        RogueWeatherPatternDesc clear = {"clear", 600, 900, 0.0f, 0.1f, 0xFFFFFFFFu, 3.0f};
        rogue_weather_register(&clear);
        RogueWeatherPatternDesc rain = {"rain",
                                        400,
                                        700,
                                        0.3f,
                                        0.8f,
                                        (1u << ROGUE_BIOME_PLAINS) |
                                            (1u << ROGUE_BIOME_FOREST_BIOME) |
                                            (1u << ROGUE_BIOME_SWAMP_BIOME),
                                        5.0f};
        rogue_weather_register(&rain);
        RogueWeatherPatternDesc snow = {
            "snow", 500, 800, 0.2f, 0.7f, (1u << ROGUE_BIOME_SNOW_BIOME), 4.0f};
        rogue_weather_register(&snow);
        RogueWeatherPatternDesc storm = {
            "storm", 300,  500,
            0.5f,    1.0f, (1u << ROGUE_BIOME_PLAINS) | (1u << ROGUE_BIOME_FOREST_BIOME),
            1.5f};
        rogue_weather_register(&storm);
    }
    /* NOTE: runtime will initialize & update weather state via rogue_weather_init/update */

    rogue_worldgen_context_shutdown(&ctx);
    return 1;
}

int rogue_world_find_random_spawn(const RogueTileMap* map, unsigned int seed, int* out_tx,
                                  int* out_ty)
{
    if (!map || !map->tiles || map->width <= 0 || map->height <= 0 || !out_tx || !out_ty)
        return 0;
    /* Gather candidate tiles (could be optimized; fine for moderate map sizes) */
    int w = map->width, h = map->height;
    int capacity = 0;
    for (int i = 0; i < w * h; i++)
    {
        unsigned char t = map->tiles[i];
        int walk = 0;
        switch (t)
        {
        case ROGUE_TILE_GRASS:
        case ROGUE_TILE_FOREST:
        case ROGUE_TILE_SWAMP:
        case ROGUE_TILE_SNOW:
        case ROGUE_TILE_CAVE_FLOOR:
        case ROGUE_TILE_STRUCTURE_FLOOR:
        case ROGUE_TILE_DUNGEON_FLOOR:
            walk = 1;
            break;
        default:
            break;
        }
        if (walk)
            capacity++;
    }
    if (capacity == 0)
        return 0;
    int pick = (int) (seed % (unsigned int) capacity);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            unsigned char t = map->tiles[y * w + x];
            int walk = 0;
            switch (t)
            {
            case ROGUE_TILE_GRASS:
            case ROGUE_TILE_FOREST:
            case ROGUE_TILE_SWAMP:
            case ROGUE_TILE_SNOW:
            case ROGUE_TILE_CAVE_FLOOR:
            case ROGUE_TILE_STRUCTURE_FLOOR:
            case ROGUE_TILE_DUNGEON_FLOOR:
                walk = 1;
                break;
            default:
                break;
            }
            if (!walk)
                continue;
            if (pick == 0)
            {
                *out_tx = x;
                *out_ty = y;
                return 1;
            }
            pick--;
        }
    return 0;
}
