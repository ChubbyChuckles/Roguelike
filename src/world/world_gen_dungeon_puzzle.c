#include "world_gen_dungeon_puzzle.h"
#include <stdlib.h>
#include <string.h>

/* local helpers */
static void set_err(char* err, size_t cap, const char* msg)
{
    if (err && cap)
    {
#ifdef _MSC_VER
        strncpy_s(err, cap, msg, _TRUNCATE);
#else
        strncpy(err, msg, cap - 1);
        err[cap - 1] = '\0';
#endif
    }
}

/* Minimal JSON parser for: {"logic_type":"timed_switch","param0":3,"min_skill_req":1} */
int rogue_puzzle_template_load_json_text(const char* json_text, RoguePuzzleTemplateDesc* out,
                                         char* err, size_t err_cap)
{
    if (!json_text || !out)
    {
        set_err(err, err_cap, "invalid args");
        return 0;
    }
    memset(out, 0, sizeof *out);
    const char* s = json_text;
    const char* lt = strstr(s, "logic_type");
    if (!lt)
    {
        set_err(err, err_cap, "missing logic_type");
        return 0;
    }
    const char* q = strchr(lt, '"');
    if (!q)
    {
        set_err(err, err_cap, "parse error");
        return 0;
    }
    q = strchr(q + 1, '"');
    if (!q)
    {
        set_err(err, err_cap, "parse error");
        return 0;
    }
    const char* q2 = strchr(q + 1, '"');
    if (!q2)
    {
        set_err(err, err_cap, "parse error");
        return 0;
    }
    size_t n = (size_t) (q2 - (q + 1));
    if (n >= sizeof out->logic_type)
        n = sizeof out->logic_type - 1;
    memcpy(out->logic_type, q + 1, n);
    out->logic_type[n] = '\0';

    /* param0 (optional) */
    const char* p0 = strstr(s, "param0");
    if (p0)
        out->param0 = (int) strtol(p0 + 6, NULL, 10);
    else
        out->param0 = 0;
    const char* ms = strstr(s, "min_skill_req");
    if (ms)
        out->min_skill_req = (int) strtol(ms + 13, NULL, 10);
    else
        out->min_skill_req = 0;
    return 1;
}

/* Stamp a traversal marker into overlay_deco so headless tests can assert placements
 * deterministically. */
static void stamp_marker(RogueTileMap* m, int x, int y, RogueTraversalMarkerType t)
{
    if (!m || x < 0 || y < 0 || x >= m->width || y >= m->height)
        return;
    /* Use high codes to avoid colliding with decorative 1..3. */
    unsigned char code = 0;
    switch (t)
    {
    case ROGUE_TRAVERSAL_JUMP_GLYPH:
        code = 200;
        break;
    case ROGUE_TRAVERSAL_TIMED_DOOR:
        code = 201;
        break;
    case ROGUE_TRAVERSAL_SECRET_PASSAGE:
        code = 202;
        break;
    case ROGUE_TRAVERSAL_MOVING_PLATFORM:
        code = 203;
        break;
    default:
        return;
    }
    rogue_tilemap_set_deco(m, x, y, code);
}

int rogue_dungeon_place_traversal(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                  const RogueDungeonGraph* graph, const RogueAssistToggles* assist)
{
    (void) ctx;
    (void) assist;
    if (!io_map || !graph || graph->room_count <= 0)
        return -1;
    int placed = 0;
    for (int i = 0; i < graph->room_count; ++i)
    {
        const RogueDungeonRoom* r = &graph->rooms[i];
        int cx = r->x + r->w / 2;
        int cy = r->y + r->h / 2;
        if (r->tag & ROGUE_DUNGEON_ROOM_PUZZLE)
        {
            stamp_marker(io_map, cx, cy, ROGUE_TRAVERSAL_JUMP_GLYPH);
            placed++;
        }
        if (r->tag & ROGUE_DUNGEON_ROOM_TREASURE)
        {
            stamp_marker(io_map, cx, cy, ROGUE_TRAVERSAL_TIMED_DOOR);
            placed++;
        }
    }
    return placed;
}

int rogue_dungeon_softlock_watchdog(const RogueDungeonGraph* graph)
{
    if (!graph || graph->room_count <= 0)
        return 0;
    /* Simplified: ensure graph connectivity with a BFS from room 0 covers all rooms. */
    int n = graph->room_count;
    int* vis = (int*) calloc((size_t) n, sizeof(int));
    if (!vis)
        return 0;
    int* q = (int*) malloc((size_t) n * sizeof(int));
    if (!q)
    {
        free(vis);
        return 0;
    }
    int qs = 0, qe = 0;
    q[qe++] = 0;
    vis[0] = 1;
    while (qs < qe)
    {
        int cur = q[qs++];
        for (int e = 0; e < graph->edge_count; ++e)
        {
            int a = graph->edges[e].a, b = graph->edges[e].b;
            int nxt = (a == cur) ? b : (b == cur) ? a : -1;
            if (nxt >= 0 && nxt < n && !vis[nxt])
            {
                vis[nxt] = 1;
                q[qe++] = nxt;
            }
        }
    }
    int ok = 1;
    for (int i = 0; i < n; ++i)
        if (!vis[i])
        {
            ok = 0;
            break;
        }
    free(q);
    free(vis);
    return ok;
}
