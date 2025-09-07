#include "../../src/world/tilemap.h"
#include "../../src/world/world_gen_room_templates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_empty_map(RogueTileMap* m, int w, int h)
{
    m->width = w;
    m->height = h;
    m->tiles = (unsigned char*) malloc((size_t) w * h);
    memset(m->tiles, ROGUE_TILE_EMPTY, (size_t) w * h);
}

static int count_tile(const RogueTileMap* m, unsigned char t)
{
    int c = 0;
    int n = m->width * m->height;
    for (int i = 0; i < n; ++i)
        if (m->tiles[i] == t)
            c++;
    return c;
}

int test_dungeon_phase2_room_templates(void)
{
    /* 5x5 simple room with exits on each side */
    const char* rows[] = {
        "#####", "E...D", "#...#", "L...S", "#####",
    };
    RogueRoomTemplate t;
    if (rogue_room_template_from_ascii(rows, 5, 101, ROGUE_ROOM_SMALL, &t) != 0)
    {
        fprintf(stderr, "template parse failed\n");
        return 1;
    }

    RogueTileMap map;
    make_empty_map(&map, 20, 20);
    /* Stamp without rotation */
    int w0 = rogue_dungeon_stamp_template(&map, 2, 2, &t, 0, 0);
    if (w0 <= 0)
    {
        fprintf(stderr, "stamp failed\n");
        return 1;
    }
    /* Bounds: ensure only inside region changed */
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
        {
            if (x >= 2 && x < 7 && y >= 2 && y < 7)
                continue;
            if (map.tiles[y * 20 + x] != ROGUE_TILE_EMPTY)
            {
                fprintf(stderr, "OOB write at %d,%d\n", x, y);
                return 1;
            }
        }
    /* Exit marker row had 'E' and 'D' which become floor and door */
    if (map.tiles[(2 + 1) * 20 + (2 + 0)] != ROGUE_TILE_DUNGEON_FLOOR)
    {
        fprintf(stderr, "exit as floor\n");
        return 1;
    }
    if (map.tiles[(2 + 1) * 20 + (2 + 4)] != ROGUE_TILE_DUNGEON_DOOR)
    {
        fprintf(stderr, "door missing\n");
        return 1;
    }

    /* Rotation determinism: 90 then 270 back to orientation equivalence in counts */
    RogueTileMap m90;
    make_empty_map(&m90, 20, 20);
    RogueTileMap m270;
    make_empty_map(&m270, 20, 20);
    (void) rogue_dungeon_stamp_template(&m90, 2, 2, &t, 90, 0);
    (void) rogue_dungeon_stamp_template(&m270, 2, 2, &t, 270, 0);
    if (count_tile(&m90, ROGUE_TILE_DUNGEON_WALL) != count_tile(&m270, ROGUE_TILE_DUNGEON_WALL))
    {
        fprintf(stderr, "wall count mismatch 90 vs 270\n");
        return 1;
    }

    /* Exit coordinates after transform computed consistently */
    RogueRoomExit exits[16];
    int ec0 = rogue_room_template_compute_exits(&t, 0, 0, exits, 16);
    int ec1 = rogue_room_template_compute_exits(&t, 90, 0, exits, 16);
    if (ec0 <= 0 || ec1 <= 0)
    {
        fprintf(stderr, "exit compute failed\n");
        return 1;
    }

    rogue_room_template_free(&t);
    free(map.tiles);
    free(m90.tiles);
    free(m270.tiles);
    return 0;
}

int main(void)
{
    int rc = test_dungeon_phase2_room_templates();
    if (rc == 0)
    {
        printf("room templates phase2: ok\n");
    }
    return rc;
}
