#include "world_gen.h"
#include "world_gen_dungeon_puzzle.h"
#include <stdio.h>
#include <string.h>

static void assert_true(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "ASSERT FAILED: %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    /* 6.1: JSON template loader */
    const char* js =
        "{\n  \"logic_type\": \"timed_switch\",\n  \"param0\": 3,\n  \"min_skill_req\": 1\n}";
    RoguePuzzleTemplateDesc d;
    char err[64];
    int ok = rogue_puzzle_template_load_json_text(js, &d, err, sizeof err);
    assert_true(ok == 1, "puzzle template json load");
    assert_true(strcmp(d.logic_type, "timed_switch") == 0, "logic_type parsed");
    assert_true(d.param0 == 3 && d.min_skill_req == 1, "params parsed");

    /* 6.2/6.4: traversal placement + watchdog */
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 1337;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    /* Build a tiny graph with 3 rooms, tag 1 as PUZZLE and 2 as TREASURE */
    RogueDungeonGraph g = {0};
    g.room_count = 3;
    g.rooms = (RogueDungeonRoom*) calloc(3, sizeof(RogueDungeonRoom));
    g.rooms[0] = (RogueDungeonRoom){0, 10, 10, 6, 4, 0, 0};
    g.rooms[1] = (RogueDungeonRoom){1, 30, 10, 6, 4, ROGUE_DUNGEON_ROOM_PUZZLE, 0};
    g.rooms[2] = (RogueDungeonRoom){2, 50, 10, 8, 6, ROGUE_DUNGEON_ROOM_TREASURE, 0};
    g.edge_count = 2;
    g.edges = (RogueDungeonEdge*) calloc(2, sizeof(RogueDungeonEdge));
    g.edges[0] = (RogueDungeonEdge){0, 1, 0};
    g.edges[1] = (RogueDungeonEdge){1, 2, 0};

    RogueTileMap map;
    assert_true(rogue_tilemap_init(&map, 80, 40), "tilemap init");

    RogueAssistToggles assist = {0};
    int placed = rogue_dungeon_place_traversal(&ctx, &map, &g, &assist);
    assert_true(placed == 2, "two traversal markers placed");

    /* Validate overlay codes at the centers */
    int cx1 = g.rooms[1].x + g.rooms[1].w / 2;
    int cy1 = g.rooms[1].y + g.rooms[1].h / 2;
    int cx2 = g.rooms[2].x + g.rooms[2].w / 2;
    int cy2 = g.rooms[2].y + g.rooms[2].h / 2;
    unsigned char c1 = rogue_tilemap_get_deco(&map, cx1, cy1);
    unsigned char c2 = rogue_tilemap_get_deco(&map, cx2, cy2);
    assert_true(c1 == 200, "jump glyph at puzzle room center");
    assert_true(c2 == 201, "timed door at treasure room center");

    /* Soft-lock watchdog should report OK (connected chain) */
    assert_true(rogue_dungeon_softlock_watchdog(&g) == 1, "softlock watchdog OK");

    rogue_tilemap_free(&map);
    free(g.rooms);
    free(g.edges);
    rogue_worldgen_context_shutdown(&ctx);
    printf("OK test_dungeon_phase6_puzzle_traversal\n");
    return 0;
}
