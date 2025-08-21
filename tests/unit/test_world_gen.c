#include "world/world_gen.h"
#include <stdio.h>

static int count_type(const RogueTileMap* m, unsigned char t)
{
    int c = 0;
    for (int y = 0; y < m->height; y++)
        for (int x = 0; x < m->width; x++)
            if (m->tiles[y * m->width + x] == t)
                c++;
    return c;
}

int main(void)
{
    RogueWorldGenConfig cfg = {.seed = 1234u,
                               .width = 64,
                               .height = 48,
                               .biome_regions = 8,
                               .cave_iterations = 3,
                               .cave_fill_chance = 0.45,
                               .river_attempts = 2};
    RogueTileMap map;
    if (!rogue_world_generate(&map, &cfg))
    {
        printf("gen fail\n");
        return 1;
    }
    int water = count_type(&map, ROGUE_TILE_WATER);
    int river = count_type(&map, ROGUE_TILE_RIVER);
    int cave_floor = count_type(&map, ROGUE_TILE_CAVE_FLOOR);
    if (water == 0)
    {
        printf("no water\n");
        return 1;
    }
    if (river == 0)
    {
        printf("no river\n");
        return 1;
    }
    if (cave_floor == 0)
    {
        printf("no caves\n");
        return 1;
    }
    rogue_tilemap_free(&map);
    return 0;
}
