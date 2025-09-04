#include "world/tilemap.h"
#include "world/world_gen_room_templates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_empty_map(RogueTileMap* m, int w, int h)
{
    if (!rogue_tilemap_init(m, w, h))
    {
        m->width = w;
        m->height = h;
        m->tiles = (unsigned char*) malloc((size_t) w * h);
        memset(m->tiles, ROGUE_TILE_EMPTY, (size_t) w * h);
        m->overlay_deco = (unsigned char*) calloc((size_t) w * h, 1);
    }
}

static int run_deco_layering_test(void)
{
    const char* json = "{\n"
                       "  \"id\": 301,\n"
                       "  \"cls\": \"small\",\n"
                       "  \"grid\": [\n"
                       "    \"####\",\n"
                       "    \"#..#\",\n"
                       "    \"#..#\",\n"
                       "    \"####\"\n"
                       "  ],\n"
                       "  \"deco\": [\n"
                       "    {\"x\":1,\"y\":1,\"kind\":\"pillar\"},\n"
                       "    {\"x\":2,\"y\":1,\"kind\":\"banner\"}\n"
                       "  ]\n"
                       "}";
    RogueRoomTemplate t;
    char err[128];
    if (!rogue_room_template_load_json_text(json, &t, err, sizeof err))
    {
        fprintf(stderr, "json load failed: %s\n", err);
        return 1;
    }
    RogueTileMap map;
    make_empty_map(&map, 10, 10);
    (void) rogue_dungeon_stamp_template(&map, 3, 3, &t, 0, 0);
    /* Base tile must remain floor under deco at (1,1) and (2,1) local -> (4,4) and (5,4) global */
    if (rogue_tilemap_get(&map, 4, 4) != ROGUE_TILE_DUNGEON_FLOOR)
    {
        fprintf(stderr, "base tile under deco not floor at 4,4\n");
        return 1;
    }
    if (rogue_tilemap_get_deco(&map, 4, 4) != 1 /* pillar code */)
    {
        fprintf(stderr, "pillar deco code missing at 4,4 (got %u)\n",
                rogue_tilemap_get_deco(&map, 4, 4));
        return 1;
    }
    if (rogue_tilemap_overlay_blocks(&map, 4, 4) != 1)
    {
        fprintf(stderr, "pillar should block overlay at 4,4\n");
        return 1;
    }
    if (rogue_tilemap_get_deco(&map, 5, 4) == 0)
    {
        fprintf(stderr, "banner deco code missing at 5,4\n");
        return 1;
    }
    if (rogue_tilemap_overlay_blocks(&map, 5, 4) != 0)
    {
        fprintf(stderr, "banner should not block\n");
        return 1;
    }
    rogue_room_template_free(&t);
    rogue_tilemap_free(&map);
    return 0;
}

int main(void)
{
    int rc = run_deco_layering_test();
    if (rc == 0)
        printf("dungeon deco layering: ok\n");
    return rc;
}
