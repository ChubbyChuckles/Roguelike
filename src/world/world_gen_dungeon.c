/**
 * @file world_gen_dungeon.c
 * @brief Dungeon generation system for procedural level creation.
 * @details This module implements a graph-based dungeon generator that creates room-and-corridor
 * layouts with thematic tagging, key/lock mechanics, traps, and secret areas.
 */

/* Phase 7: Dungeon Generator Implementation */
#include "world_gen.h"
#include "world_gen_dungeon_kernel.h"
#include "world_gen_dungeon_taxonomy.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Generates a random integer in a range using the provided RNG channel.
 * @param ch Pointer to the RNG channel.
 * @param lo Lower bound (inclusive).
 * @param hi Upper bound (inclusive).
 * @return Random integer in the range [lo, hi].
 */
static int rng_range(RogueRngChannel* ch, int lo, int hi)
{
    if (hi <= lo)
        return lo;
    unsigned int r = rogue_worldgen_rand_u32(ch);
    return lo + (int) (r % (unsigned int) (hi - lo + 1));
}

/**
 * @brief Generates a dungeon graph with rooms and connections.
 * @param ctx Pointer to the world generation context.
 * @param target_rooms Target number of rooms to generate.
 * @param loop_percent Percentage of extra loops to add (0-100).
 * @param out_graph Pointer to the output dungeon graph.
 * @return true on success, false on failure.
 * @details Creates a graph of interconnected rooms using a minimum spanning tree approach
 * with additional loops for variety. Rooms are placed without overlap and tagged thematically.
 */
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

/* ---- Phase 1: Extended kernel with DSL and constraints ---- */

static int edge_exists(const RogueDungeonGraph* g, int a, int b)
{
    for (int i = 0; i < g->edge_count; ++i)
    {
        int x = g->edges[i].a, y = g->edges[i].b;
        if ((x == a && y == b) || (x == b && y == a))
            return 1;
    }
    return 0;
}

static void compute_degrees(const RogueDungeonGraph* g, int* deg /* length room_count */)
{
    memset(deg, 0, sizeof(int) * (size_t) g->room_count);
    for (int i = 0; i < g->edge_count; ++i)
    {
        deg[g->edges[i].a]++;
        deg[g->edges[i].b]++;
    }
}

static int try_add_edge(RogueDungeonGraph* g, int a, int b, int loop_flag,
                        const int* max_deg /* nullable */, int* deg /* nullable */)
{
    if (a == b)
        return 0;
    if (edge_exists(g, a, b))
        return 0;
    if (max_deg)
    {
        int da = deg ? deg[a] : 0;
        int db = deg ? deg[b] : 0;
        if (*max_deg > 0 && (da + 1 > *max_deg || db + 1 > *max_deg))
            return 0;
    }
    g->edges = (RogueDungeonEdge*) realloc(g->edges,
                                           sizeof(RogueDungeonEdge) * (size_t) (g->edge_count + 1));
    if (!g->edges)
        return 0;
    g->edges[g->edge_count++] = (RogueDungeonEdge){a, b, loop_flag ? 1 : 0};
    if (deg)
    {
        deg[a]++;
        deg[b]++;
    }
    return 1;
}

/* Build simple abstract layouts from a tiny grammar.
   L(n): linear chain with n nodes
   H(k): hub + k leaves (total k+1 nodes)
   B(n,b): base chain length n, with b extra leaves attached to interior nodes (total n+b nodes)
*/
/* Manual, portable parser for spec strings like "L(10)", "H(8)", "B(8,4)" to avoid MSVC
 * C4996 warnings from sscanf. Returns 1 on success and fills out_type, out_n1 and optionally
 * out_n2 (when has_n2==1). */
static int parse_grammar_spec(const char* spec, char* out_type, int* out_n1, int* out_n2,
                              int* out_has_n2)
{
    if (!spec || !out_type || !out_n1 || !out_has_n2)
        return 0;
    const char* s = spec;
    while (*s && isspace((unsigned char) *s))
        ++s;
    char t = *s;
    if (t != 'L' && t != 'H' && t != 'B')
        return 0;
    ++s;
    while (*s && isspace((unsigned char) *s))
        ++s;
    if (*s != '(')
        return 0;
    ++s;
    while (*s && isspace((unsigned char) *s))
        ++s;
    /* parse first integer */
    int sign = 1;
    if (*s == '+')
        ++s;
    else if (*s == '-')
    {
        sign = -1;
        ++s;
    }
    long v1 = 0;
    int any = 0;
    while (*s && isdigit((unsigned char) *s))
    {
        any = 1;
        v1 = v1 * 10 + (*s - '0');
        ++s;
    }
    if (!any)
        return 0;
    v1 *= sign;
    while (*s && isspace((unsigned char) *s))
        ++s;
    int has_n2 = 0;
    long v2 = 0;
    if (*s == ',')
    {
        ++s;
        while (*s && isspace((unsigned char) *s))
            ++s;
        sign = 1;
        if (*s == '+')
            ++s;
        else if (*s == '-')
        {
            sign = -1;
            ++s;
        }
        any = 0;
        while (*s && isdigit((unsigned char) *s))
        {
            any = 1;
            v2 = v2 * 10 + (*s - '0');
            ++s;
        }
        if (!any)
            return 0;
        v2 *= sign;
        has_n2 = 1;
    }
    while (*s && isspace((unsigned char) *s))
        ++s;
    if (*s == ')')
        ++s;
    /* success */
    *out_type = t;
    *out_n1 = (int) v1;
    if (out_n2)
        *out_n2 = (int) v2;
    *out_has_n2 = has_n2;
    return 1;
}
int rogue_dungeon_generate_from_grammar(RogueWorldGenContext* ctx, const char* spec,
                                        RogueDungeonGraph* out_graph)
{
    (void) ctx;
    if (!spec || !out_graph)
        return 0;
    int n1 = 0, n2 = 0;
    char type = 0;
    int has_n2 = 0;
    if (!parse_grammar_spec(spec, &type, &n1, &n2, &has_n2))
        return 0;
    if (type != 'L' && type != 'H' && type != 'B')
        return 0;
    if (type == 'L' && n1 < 2)
        n1 = 2;
    if (type == 'H' && n1 < 1)
        n1 = 1;
    if (type == 'B')
    {
        if (n1 < 2)
            n1 = 2;
        if (!has_n2)
            n2 = 0;
        if (n2 < 0)
            n2 = 0;
    }
    /* Allocate rooms conservatively */
    int total = (type == 'L') ? n1 : (type == 'H') ? (n1 + 1) : (n1 + n2);
    RogueDungeonRoom* rooms = (RogueDungeonRoom*) calloc((size_t) total, sizeof(RogueDungeonRoom));
    RogueDungeonEdge* edges =
        (RogueDungeonEdge*) calloc((size_t) (total * 3), sizeof(RogueDungeonEdge));
    if (!rooms || !edges)
    {
        free(rooms);
        free(edges);
        return 0;
    }
    int rc = 0, ec = 0;
    int spacing = 8; /* tile spacing to avoid overlap */
    int base_w = 6, base_h = 5;
    if (type == 'L')
    {
        for (int i = 0; i < n1; ++i)
        {
            rooms[rc] = (RogueDungeonRoom){rc, 10 + i * spacing, 20, base_w, base_h, 0, 0};
            if (i > 0)
                edges[ec++] = (RogueDungeonEdge){i - 1, i, 0};
            rc++;
        }
    }
    else if (type == 'H')
    {
        /* center hub at index 0, leaves 1..k */
        rooms[rc++] = (RogueDungeonRoom){0, 40, 40, base_w + 2, base_h + 2, 0, 0};
        int k = n1;
        for (int i = 0; i < k; ++i)
        {
            int angle = i; /* integer placeholder just to vary positions */
            int dx = (i % 2 == 0) ? spacing : -spacing;
            int dy = (angle % 3 == 0) ? spacing : -spacing;
            rooms[rc] = (RogueDungeonRoom){rc, 40 + dx, 40 + dy, base_w, base_h, 0, 0};
            edges[ec++] = (RogueDungeonEdge){0, rc, 0};
            rc++;
        }
    }
    else /* B */
    {
        int chain = n1, branches = n2;
        /* Build chain 0..chain-1 horizontally */
        for (int i = 0; i < chain; ++i)
        {
            rooms[rc] = (RogueDungeonRoom){rc, 10 + i * spacing, 60, base_w, base_h, 0, 0};
            if (i > 0)
                edges[ec++] = (RogueDungeonEdge){i - 1, i, 0};
            rc++;
        }
        /* Attach leaves to interior nodes */
        int attached = 0;
        for (int i = 1; i < chain - 1 && attached < branches; ++i)
        {
            rooms[rc] =
                (RogueDungeonRoom){rc, 10 + i * spacing, 60 + spacing, base_w, base_h, 0, 0};
            edges[ec++] = (RogueDungeonEdge){i, rc, 1};
            rc++;
            attached++;
        }
        /* If still remaining, stack above */
        for (int i = 1; i < chain - 1 && attached < branches; ++i)
        {
            rooms[rc] =
                (RogueDungeonRoom){rc, 10 + i * spacing, 60 - spacing, base_w, base_h, 0, 0};
            edges[ec++] = (RogueDungeonEdge){i, rc, 1};
            rc++;
            attached++;
        }
    }
    /* Minimal tagging: largest room as treasure */
    int largest = -1, largest_area = 0;
    for (int i = 0; i < rc; ++i)
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

    out_graph->rooms = rooms;
    out_graph->room_count = rc;
    out_graph->edges = edges;
    out_graph->edge_count = ec;
    return 1;
}

static void fill_default_params(RogueDungeonGenParams* p)
{
    if (p->loop_percent <= 0)
        p->loop_percent = 15;
    if (p->arch < 0 || p->arch >= ROGUE_DUNGEON_ARCHETYPE_MAX)
        p->arch = ROGUE_DUNGEON_ARCH_BRANCHING;
}

/* Heuristic archetype builders (simple fallbacks) */
static int build_archetype_layout(RogueWorldGenContext* ctx, const RogueDungeonGenParams* p,
                                  RogueDungeonGraph* g)
{
    (void) ctx;
    if (p->arch == ROGUE_DUNGEON_ARCH_LINEAR)
    {
        char buf[32];
        snprintf(buf, sizeof buf, "L(%d)", p->target_rooms);
        return rogue_dungeon_generate_from_grammar(ctx, buf, g);
    }
    else if (p->arch == ROGUE_DUNGEON_ARCH_HUB)
    {
        char buf[32];
        int leaves = (p->target_rooms > 0) ? (p->target_rooms - 1) : 4;
        if (leaves < 1)
            leaves = 1;
        snprintf(buf, sizeof buf, "H(%d)", leaves);
        return rogue_dungeon_generate_from_grammar(ctx, buf, g);
    }
    /* default: call baseline random generator */
    return rogue_dungeon_generate_graph((RogueWorldGenContext*) ctx, p->target_rooms,
                                        p->loop_percent, g);
}

bool rogue_dungeon_generate_graph_ex(RogueWorldGenContext* ctx, const RogueDungeonGenParams* in,
                                     RogueDungeonGraph* out_graph)
{
    if (!ctx || !in || !out_graph || in->target_rooms <= 0)
        return false;
    RogueDungeonGenParams p = *in;
    fill_default_params(&p);

    /* Generate base layout */
    RogueDungeonGraph g = {0};
    int ok = 0;
    if (p.grammar && p.grammar[0])
        ok = rogue_dungeon_generate_from_grammar(ctx, p.grammar, &g);
    else
        ok = build_archetype_layout(ctx, &p, &g);
    if (!ok)
        return false;

    /* Enforce constraints: degrees and dead-ends */
    int* deg = (int*) calloc((size_t) g.room_count, sizeof(int));
    if (!deg)
    {
        rogue_dungeon_free_graph(&g);
        return false;
    }
    compute_degrees(&g, deg);
    /* Reduce dead-ends by connecting leaf pairs */
    if (p.max_deadends > 0)
    {
        int leaf_count = 0;
        for (int i = 0; i < g.room_count; ++i)
            if (deg[i] == 1)
                leaf_count++;
        for (int i = 0; i < g.room_count && leaf_count > p.max_deadends; ++i)
        {
            if (deg[i] != 1)
                continue;
            /* find another leaf j not adjacent */
            for (int j = i + 1; j < g.room_count && leaf_count > p.max_deadends; ++j)
            {
                if (deg[j] != 1)
                    continue;
                if (edge_exists(&g, i, j))
                    continue;
                if (try_add_edge(&g, i, j, 1, &p.max_branch_degree, deg))
                {
                    leaf_count -= 2;
                    break;
                }
            }
        }
    }
    /* Raise degrees below minimum by connecting to nearest (by id delta) */
    if (p.min_branch_degree > 1)
    {
        for (int i = 0; i < g.room_count; ++i)
        {
            while (deg[i] > 0 && deg[i] < p.min_branch_degree)
            {
                /* pick a target j with smallest |j-i| that's not currently connected */
                int best_j = -1;
                for (int radius = 1; radius < g.room_count; ++radius)
                {
                    int j1 = i - radius, j2 = i + radius;
                    if (j1 >= 0 && j1 != i && !edge_exists(&g, i, j1))
                        best_j = j1;
                    if (best_j < 0 && j2 < g.room_count && !edge_exists(&g, i, j2))
                        best_j = j2;
                    if (best_j >= 0)
                        break;
                }
                if (best_j < 0)
                    break;
                if (!try_add_edge(&g, i, best_j, 1, &p.max_branch_degree, deg))
                    break;
            }
        }
    }

    /* Critical path length target: simple bounded retries on baseline generator or adding leafs
       connections to stretch path if too short. */
    if (p.critical_path_target_min > 0)
    {
        int cpl = rogue_dungeon_graph_critical_path_length(&g);
        int attempts = 8;
        while (cpl >= 0 && cpl < p.critical_path_target_min && attempts-- > 0)
        {
            /* Strategy: connect two far leaves to extend chain if possible by creating a new
               bridge via an intermediate low-degree node */
            int extended = 0;
            for (int i = 0; i < g.room_count && !extended; ++i)
            {
                if (deg[i] != 1)
                    continue;
                for (int j = g.room_count - 1; j > i; --j)
                {
                    if (deg[j] != 1 || edge_exists(&g, i, j))
                        continue;
                    if (try_add_edge(&g, i, j, 1, &p.max_branch_degree, deg))
                    {
                        extended = 1;
                        break;
                    }
                }
            }
            if (!extended)
                break;
            cpl = rogue_dungeon_graph_critical_path_length(&g);
        }
        if (p.critical_path_target_max > 0 && cpl > p.critical_path_target_max)
        {
            /* No destructive pruning to reduce CPL; accept as a soft cap. */
            (void) 0;
        }
    }

    free(deg);
    *out_graph = g;
    return true;
}

/**
 * @brief Frees the memory allocated for a dungeon graph.
 * @param g Pointer to the dungeon graph to free.
 */
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

/**
 * @brief Carves the dungeon graph into a tile map.
 * @param ctx Pointer to the world generation context.
 * @param io_map Pointer to the tile map to modify.
 * @param graph Pointer to the dungeon graph.
 * @param ox Origin X offset.
 * @param oy Origin Y offset.
 * @param w Width of the carving area.
 * @param h Height of the carving area.
 * @return Number of floor tiles carved.
 * @details Carves rooms and L-shaped corridors into the map, placing walls and floors.
 */
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
                if (x < 0 || y < 0 || x >= io_map->width || y >= io_map->height)
                    continue;
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
            if (x >= 0 && y >= 0 && x < io_map->width && y < io_map->height)
            {
                int idx = y * io_map->width + x;
                if (io_map->tiles[idx] != ROGUE_TILE_DUNGEON_WALL)
                {
                    io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                    carved++;
                }
            }
            x += (bx > ax) ? 1 : -1;
        }
        while (y != by)
        {
            if (x >= 0 && y >= 0 && x < io_map->width && y < io_map->height)
            {
                int idx = y * io_map->width + x;
                if (io_map->tiles[idx] != ROGUE_TILE_DUNGEON_WALL)
                {
                    io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                    carved++;
                }
            }
            y += (by > ay) ? 1 : -1;
        }
    }
    return carved;
}

/**
 * @brief Places keys and locked doors in the dungeon.
 * @param ctx Pointer to the world generation context.
 * @param io_map Pointer to the tile map to modify.
 * @param graph Pointer to the dungeon graph.
 * @return Number of locks placed.
 * @details Randomly locks some room entrances and places corresponding keys in earlier rooms.
 */
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

/**
 * @brief Places traps and secret doors in the dungeon.
 * @param ctx Pointer to the world generation context.
 * @param io_map Pointer to the tile map to modify.
 * @param graph Pointer to the dungeon graph.
 * @param target_traps Target number of traps to place.
 * @param secret_room_chance Chance (0.0-1.0) to make a room secret.
 * @return Number of traps placed.
 * @details Adds traps to rooms and converts some walls to secret doors for hidden areas.
 */
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

/**
 * @brief Validates that all rooms in the dungeon are reachable.
 * @param graph Pointer to the dungeon graph.
 * @return Number of reachable rooms, or 0 on error.
 * @details Performs a graph traversal to ensure connectivity from the starting room.
 */
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

/**
 * @brief Calculates the loop ratio in the dungeon graph.
 * @param graph Pointer to the dungeon graph.
 * @return Ratio of loop edges to total edges (0.0-1.0).
 */
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

/**
 * @brief Counts the number of secret rooms in the dungeon.
 * @param graph Pointer to the dungeon graph.
 * @return Number of secret rooms.
 */
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
