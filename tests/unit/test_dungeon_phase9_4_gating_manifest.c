#include "../../src/world/world_gen.h"
#include <stdio.h>
#include <stdlib.h> /* calloc, free */
#include <string.h>

static int build_tiny_graph(RogueDungeonGraph* g)
{
    memset(g, 0, sizeof *g);
    g->room_count = 3;
    g->rooms = (RogueDungeonRoom*) calloc((size_t) g->room_count, sizeof(RogueDungeonRoom));
    if (!g->rooms)
        return 0;
    g->rooms[0] =
        (RogueDungeonRoom){.id = 0, .x = 0, .y = 0, .w = 5, .h = 5, .tag = 0, .secret = 0};
    g->rooms[1] = (RogueDungeonRoom){
        .id = 1, .x = 10, .y = 0, .w = 5, .h = 5, .tag = ROGUE_DUNGEON_ROOM_PUZZLE, .secret = 0};
    g->rooms[2] = (RogueDungeonRoom){
        .id = 2, .x = 20, .y = 0, .w = 5, .h = 5, .tag = ROGUE_DUNGEON_ROOM_TREASURE, .secret = 1};
    return 1;
}

int main(void)
{
    RogueDungeonGraph g;
    if (!build_tiny_graph(&g))
        return 1;
    const char* out = "build/analytics/gating_manifest_test.json";
    if (!rogue_dungeon_export_gating_manifest(out, &g))
        return 2;
    /* quick read-back to assert content mentions expected capabilities */
    FILE* f = fopen(out, "rb");
    if (!f)
        return 3;
    char buf[512];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (strstr(buf, "\"puzzle\"") == NULL)
        return 4;
    if (strstr(buf, "\"timed_door\"") == NULL)
        return 5;
    if (strstr(buf, "\"secret_passage\"") == NULL)
        return 6;
    free(g.rooms);
    return 0;
}
