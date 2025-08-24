#include "../../src/world/tilemap.h"
#include <stdio.h>

int main(void)
{
    RogueTileMap map;
    if (!rogue_tilemap_init(&map, 4, 4))
    {
        printf("init fail\n");
        return 1;
    }
    rogue_tilemap_set(&map, 1, 1, 42);
    if (rogue_tilemap_get(&map, 1, 1) != 42)
    {
        printf("get/set fail\n");
        return 1;
    }
    if (rogue_tilemap_get(&map, -1, -1) != 0)
    {
        printf("oob fail\n");
        return 1;
    }
    rogue_tilemap_free(&map);
    return 0;
}
