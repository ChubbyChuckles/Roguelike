/* Phase 7: Dungeon Generator Implementation */
#include "world_gen.h"
#include <stdlib.h>
#include <string.h>

static int rng_range(RogueRngChannel* ch, int lo, int hi)
{
    if (hi <= lo)
        return lo;
    unsigned int r = rogue_worldgen_rand_u32(ch);
    return lo + (int) (r % (unsigned int) (hi - lo + 1));
}

bool rogue_dungeon_generate_graph(RogueWorldGenContext* ctx, int target_rooms, int loop_percent,
                                  RogueDungeonGraph* out_graph)
{
    if (!ctx || !out_graph || target_rooms <= 0)
        return false;
    if (loop_percent < 0)
        loop_percent = 0;
    if (loop_percent > 100)
        loop_percent = 100;
    int max_rooms = target_rooms;
    RogueDungeonRoom* rooms =
        (RogueDungeonRoom*) calloc((size_t) max_rooms, sizeof(RogueDungeonRoom));
    if (!rooms)
        return false;
    int room_count = 0;
    int attempts = max_rooms * 10;
    while (room_count < max_rooms && attempts-- > 0)
    {
        int w = rng_range(&ctx->micro_rng, 4, 10);
        int h = rng_range(&ctx->micro_rng, 4, 9);
        int x = rng_range(&ctx->micro_rng, 2, 200 - w - 2); /* generic space */
        int y = rng_range(&ctx->micro_rng, 2, 200 - h - 2);
        /* Overlap rejection */
        int overlap = 0;
        for (int i = 0; i < room_count; i++)
        {
            RogueDungeonRoom* r = &rooms[i];
            if (!(x + w <= r->x || r->x + r->w <= x || y + h <= r->y || r->y + r->h <= y))
            {
                overlap = 1;
                break;
            }
        }
        if (overlap)
            continue;
        rooms[room_count] = (RogueDungeonRoom){room_count, x, y, w, h, 0, 0};
        room_count++;
    }
    if (room_count == 0)
    {
        free(rooms);
        return false;
    }
    /* Minimum spanning tree style chain (simple nearest neighbor) */
    RogueDungeonEdge* edges =
        (RogueDungeonEdge*) calloc((size_t) room_count * 4, sizeof(RogueDungeonEdge));
    if (!edges)
    {
        free(rooms);
        return false;
    }
    int edge_count = 0;
    int* connected = (int*) calloc((size_t) room_count, sizeof(int));
    if (!connected)
    {
        free(edges);
        free(rooms);
        return false;
    }
    connected[0] = 1;
    int connected_count = 1;
    while (connected_count < room_count)
    {
        int best_a = -1, best_b = -1;
        int best_d = 1 << 30;
        for (int a = 0; a < room_count; a++)
            if (connected[a])
            {
                for (int b = 0; b < room_count; b++)
                    if (!connected[b])
                    {
                        int dx = (rooms[a].x + rooms[a].w / 2) - (rooms[b].x + rooms[b].w / 2);
                        int dy = (rooms[a].y + rooms[a].h / 2) - (rooms[b].y + rooms[b].h / 2);
                        int d = dx * dx + dy * dy;
                        if (d < best_d)
                        {
                            best_d = d;
                            best_a = a;
                            best_b = b;
                        }
                    }
            }
        if (best_a < 0)
            break;
        edges[edge_count++] = (RogueDungeonEdge){best_a, best_b, 0};
        connected[best_b] = 1;
        connected_count++;
    }
    /* Add extra loops */
    int desired_loops = (room_count * loop_percent) / 100;
    int loops = 0;
    int loop_attempts = room_count * 5;
    while (loops < desired_loops && loop_attempts-- > 0)
    {
        int a = rng_range(&ctx->micro_rng, 0, room_count - 1);
        int b = rng_range(&ctx->micro_rng, 0, room_count - 1);
        if (a == b)
            continue; /* ensure no duplicate */
        int dup = 0;
        for (int i = 0; i < edge_count; i++)
        {
            if ((edges[i].a == a && edges[i].b == b) || (edges[i].a == b && edges[i].b == a))
            {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;
        edges[edge_count++] = (RogueDungeonEdge){a, b, 1};
        loops++;
    }
    free(connected);
    /* Thematic tagging (treasure / elite / puzzle) deterministic based on RNG & room properties */
    if (room_count > 0)
    {
        /* Always tag largest room as treasure */
        int largest = -1;
        int largest_area = 0;
        for (int i = 0; i < room_count; i++)
        {
            int area = rooms[i].w * rooms[i].h;
            if (area > largest_area)
            {
                largest_area = area;
                largest = i;
            }
        }
        if (largest >= 0)
            rooms[largest].tag |= ROGUE_DUNGEON_ROOM_TREASURE;
        /* Tag up to 2 elite rooms: farthest from start (room 0) by center distance */
        int start_cx = rooms[0].x + rooms[0].w / 2, start_cy = rooms[0].y + rooms[0].h / 2;
        for (int pass = 0; pass < 2; pass++)
        {
            int best = -1;
            int best_d = -1;
            for (int i = 1; i < room_count; i++)
            {
                if (rooms[i].tag & ROGUE_DUNGEON_ROOM_ELITE)
                    continue;
                int cx = rooms[i].x + rooms[i].w / 2;
                int cy = rooms[i].y + rooms[i].h / 2;
                int dx = cx - start_cx;
                int dy = cy - start_cy;
                int d = dx * dx + dy * dy;
                if (d > best_d)
                {
                    best_d = d;
                    best = i;
                }
            }
            if (best > 0)
                rooms[best].tag |= ROGUE_DUNGEON_ROOM_ELITE;
        }
        /* Tag small isolated style puzzle rooms: area below median and degree==1 in MST portion */
        int* degree = (int*) calloc((size_t) room_count, sizeof(int));
        if (degree)
        {
            for (int e = 0; e < edge_count; e++)
            {
                degree[edges[e].a]++;
                degree[edges[e].b]++;
            }
            int areasum = 0;
            for (int i = 0; i < room_count; i++)
                areasum += rooms[i].w * rooms[i].h;
            int avg_area = areasum / room_count;
            for (int i = 1; i < room_count; i++)
            {
                int area = rooms[i].w * rooms[i].h;
                if (area < avg_area && degree[i] == 1 &&
                    !(rooms[i].tag & (ROGUE_DUNGEON_ROOM_TREASURE | ROGUE_DUNGEON_ROOM_ELITE)))
                {
                    rooms[i].tag |= ROGUE_DUNGEON_ROOM_PUZZLE;
                }
            }
            free(degree);
        }
    }
    out_graph->rooms = rooms;
    out_graph->room_count = room_count;
    out_graph->edges = edges;
    out_graph->edge_count = edge_count;
    return true;
}

void rogue_dungeon_free_graph(RogueDungeonGraph* g)
{
    if (!g)
        return;
    free(g->rooms);
    free(g->edges);
    g->rooms = NULL;
    g->edges = NULL;
    g->room_count = g->edge_count = 0;
}

/* Carve rooms then connect centers with corridors (L-shaped) */
int rogue_dungeon_carve_into_map(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                 const RogueDungeonGraph* graph, int ox, int oy, int w, int h)
{
    (void) ctx;
    if (!io_map || !graph)
        return 0;
    int carved = 0;
    for (int i = 0; i < graph->room_count; i++)
    {
        const RogueDungeonRoom* r = &graph->rooms[i];
        if (r->x < ox || r->y < oy || r->x + r->w > ox + w || r->y + r->h > oy + h)
            continue;
        for (int y = r->y; y < r->y + r->h; y++)
            for (int x = r->x; x < r->x + r->w; x++)
            {
                int idx = y * io_map->width + x;
                if (x == r->x || y == r->y || x == r->x + r->w - 1 || y == r->y + r->h - 1)
                    io_map->tiles[idx] = ROGUE_TILE_DUNGEON_WALL;
                else
                {
                    io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                    carved++;
                }
            }
    }
    for (int e = 0; e < graph->edge_count; e++)
    {
        const RogueDungeonEdge* edge = &graph->edges[e];
        const RogueDungeonRoom* A = &graph->rooms[edge->a];
        const RogueDungeonRoom* B = &graph->rooms[edge->b];
        int ax = A->x + A->w / 2, ay = A->y + A->h / 2;
        int bx = B->x + B->w / 2, by = B->y + B->h / 2;
        int x = ax, y = ay;
        while (x != bx)
        {
            int idx = y * io_map->width + x;
            if (io_map->tiles[idx] != ROGUE_TILE_DUNGEON_WALL)
            {
                io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                carved++;
            }
            x += (bx > ax) ? 1 : -1;
        }
        while (y != by)
        {
            int idx = y * io_map->width + x;
            if (io_map->tiles[idx] != ROGUE_TILE_DUNGEON_WALL)
            {
                io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                carved++;
            }
            y += (by > ay) ? 1 : -1;
        }
    }
    return carved;
}

int rogue_dungeon_place_keys_and_locks(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                       const RogueDungeonGraph* graph)
{
    if (!ctx || !io_map || !graph)
        return 0;
    int locked = 0;
    int rooms_for_locks = graph->room_count / 4;
    for (int i = 1; i < graph->room_count && locked < rooms_for_locks; i++)
    {
        RogueDungeonRoom* r = &graph->rooms[i];
        unsigned int rv = rogue_worldgen_rand_u32(&ctx->micro_rng);
        if ((rv & 3) == 0)
        { /* lock entrance of room i */
            int x = r->x + r->w / 2;
            int y = r->y;
            int idx = y * io_map->width + x;
            io_map->tiles[idx] = ROGUE_TILE_DUNGEON_LOCKED_DOOR;
            locked++; /* place key in an earlier room */
            int key_room = (int) (rv % i);
            RogueDungeonRoom* kr = &graph->rooms[key_room];
            int kx = kr->x + kr->w / 2;
            int ky = kr->y + kr->h / 2;
            io_map->tiles[ky * io_map->width + kx] = ROGUE_TILE_DUNGEON_KEY;
        }
    }
    return locked;
}

int rogue_dungeon_place_traps_and_secrets(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                          const RogueDungeonGraph* graph, int target_traps,
                                          double secret_room_chance)
{
    if (!ctx || !io_map || !graph)
        return 0;
    if (secret_room_chance < 0)
        secret_room_chance = 0;
    if (secret_room_chance > 1)
        secret_room_chance = 1;
    int traps = 0;
    for (int i = 0; i < graph->room_count; i++)
    {
        RogueDungeonRoom* r = &graph->rooms[i];
        if (rogue_worldgen_rand_norm(&ctx->micro_rng) < secret_room_chance && !r->secret &&
            r->w >= 5 && r->h >= 5)
        {
            r->secret = 1; /* convert one wall to secret door */
            int sx = r->x + r->w / 2;
            int sy = r->y;
            io_map->tiles[sy * io_map->width + sx] = ROGUE_TILE_DUNGEON_SECRET_DOOR;
        }
        if (traps < target_traps)
        {
            int tx = r->x + 2;
            int ty = r->y + 2;
            if (tx < r->x + r->w - 1 && ty < r->y + r->h - 1)
            {
                io_map->tiles[ty * io_map->width + tx] = ROGUE_TILE_DUNGEON_TRAP;
                traps++;
            }
        }
    }
    return traps;
}

int rogue_dungeon_validate_reachability(const RogueDungeonGraph* graph)
{
    if (!graph || graph->room_count == 0)
        return 0;
    int n = graph->room_count;
    unsigned char* vis = (unsigned char*) calloc((size_t) n, 1);
    if (!vis)
        return 0;
    int stack_cap = n;
    int* stack = (int*) malloc(sizeof(int) * (size_t) stack_cap);
    int sp = 0;
    stack[sp++] = 0;
    vis[0] = 1;
    while (sp)
    {
        int cur = stack[--sp];
        for (int e = 0; e < graph->edge_count; e++)
        {
            RogueDungeonEdge edge = graph->edges[e];
            if (edge.a == cur)
            {
                if (!vis[edge.b])
                {
                    vis[edge.b] = 1;
                    stack[sp++] = edge.b;
                }
            }
            else if (edge.b == cur)
            {
                if (!vis[edge.a])
                {
                    vis[edge.a] = 1;
                    stack[sp++] = edge.a;
                }
            }
        }
    }
    int reachable = 0;
    for (int i = 0; i < n; i++)
        if (vis[i])
            reachable++;
    free(vis);
    free(stack);
    return reachable;
}

double rogue_dungeon_loop_ratio(const RogueDungeonGraph* graph)
{
    if (!graph || graph->edge_count == 0)
        return 0.0;
    int loops = 0;
    for (int i = 0; i < graph->edge_count; i++)
        if (graph->edges[i].loop)
            loops++;
    return (double) loops / (double) graph->edge_count;
}

int rogue_dungeon_secret_room_count(const RogueDungeonGraph* graph)
{
    if (!graph)
        return 0;
    int c = 0;
    for (int i = 0; i < graph->room_count; i++)
        if (graph->rooms[i].secret)
            c++;
    return c;
}
