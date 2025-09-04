/**
 * @file world_gen_dungeon_loot.c
 * @brief Phase 8: Loot & Reward orchestration for dungeon generation (chests + materials).
 */

#include "world_gen.h"
#include <stdlib.h>
#include <string.h>

/* Helper: compute degree (number of incident edges) for each room */
static void compute_degrees(const RogueDungeonGraph* g, int* out_deg)
{
    memset(out_deg, 0, sizeof(int) * (size_t) g->room_count);
    for (int i = 0; i < g->edge_count; ++i)
    {
        const RogueDungeonEdge* e = &g->edges[i];
        if (e->a >= 0 && e->a < g->room_count)
            out_deg[e->a]++;
        if (e->b >= 0 && e->b < g->room_count)
            out_deg[e->b]++;
    }
}

/* Helper: choose an interior floor tile inside a room rectangle */
static int pick_room_interior_xy(RogueWorldGenContext* ctx, const RogueTileMap* map,
                                 const RogueDungeonRoom* r, int* out_x, int* out_y)
{
    if (!r || r->w < 3 || r->h < 3)
        return 0;
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        int x = r->x + 1 + (int) (rogue_worldgen_rand_u32(&ctx->micro_rng) % (unsigned) (r->w - 2));
        int y = r->y + 1 + (int) (rogue_worldgen_rand_u32(&ctx->micro_rng) % (unsigned) (r->h - 2));
        if (x < 0 || y < 0 || x >= map->width || y >= map->height)
            continue;
        int idx = y * map->width + x;
        if (map->tiles[idx] == ROGUE_TILE_DUNGEON_FLOOR)
        {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

/* Tier distribution by depth: simple ramp 0..3 capped, biased toward mid tiers */
static int sample_reward_tier(RogueWorldGenContext* ctx, int depth)
{
    int base = depth / 3; /* every 3 depth -> +1 tier */
    if (base > 3)
        base = 3;
    /* jitter: 60% base, 30% base-1, 10% base+1 within bounds */
    unsigned int r = rogue_worldgen_rand_u32(&ctx->micro_rng) % 100u;
    if (r < 60)
        return base;
    if (r < 90)
        return (base > 0) ? base - 1 : 0;
    return (base < 3) ? base + 1 : 3;
}

/* Helper: attempt to place an upgrade marker (14) adjacent to (cx,cy) without
 * overwriting an existing chest tier marker (10..13). Returns 1 on success. */
static int try_place_upgrade_adjacent(RogueTileMap* io_map, int cx, int cy)
{
    /* Check 4-neighbors, then diagonals */
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    for (int k = 0; k < 8; ++k)
    {
        int nx = cx + dx[k], ny = cy + dy[k];
        if (nx < 0 || ny < 0 || nx >= io_map->width || ny >= io_map->height)
            continue;
        if (rogue_tilemap_get(io_map, nx, ny) == ROGUE_TILE_DUNGEON_FLOOR &&
            rogue_tilemap_get_deco(io_map, nx, ny) == 0)
        {
            rogue_tilemap_set_deco(io_map, nx, ny, 14);
            return 1;
        }
    }
    return 0;
}

int rogue_dungeon_place_chests(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                               const RogueDungeonGraph* graph, int target_chests,
                               RogueDungeonChestPlacement* out_array, int max_out,
                               int* out_upgrade_count)
{
    if (out_upgrade_count)
        *out_upgrade_count = 0;
    if (!ctx || !io_map || !graph || graph->room_count <= 0 || target_chests <= 0)
        return 0;
    int placed = 0;
    int* deg = (int*) calloc((size_t) graph->room_count, sizeof(int));
    if (!deg)
        return 0;
    compute_degrees(graph, deg);

    /* Precompute BFS distance from room 0 to all rooms as a proxy for depth */
    int* dist = (int*) malloc(sizeof(int) * (size_t) graph->room_count);
    int* q = (int*) malloc(sizeof(int) * (size_t) graph->room_count);
    if (!dist || !q)
    {
        free(deg);
        if (dist)
            free(dist);
        if (q)
            free(q);
        return 0;
    }
    for (int i = 0; i < graph->room_count; ++i)
        dist[i] = -1;
    int qs = 0, qe = 0;
    dist[0] = 0;
    q[qe++] = 0;
    while (qs < qe)
    {
        int u = q[qs++];
        for (int e = 0; e < graph->edge_count; ++e)
        {
            const RogueDungeonEdge* ed = &graph->edges[e];
            int v = -1;
            if (ed->a == u)
                v = ed->b;
            else if (ed->b == u)
                v = ed->a;
            if (v >= 0 && dist[v] < 0)
            {
                dist[v] = dist[u] + 1;
                q[qe++] = v;
            }
        }
    }

    /* Pass 1: reserve for dead-ends (deg==1) and milestone rooms (treasure/elite/puzzle) */
    int milestone_mask =
        ROGUE_DUNGEON_ROOM_TREASURE | ROGUE_DUNGEON_ROOM_ELITE | ROGUE_DUNGEON_ROOM_PUZZLE;
    for (int pass = 0; pass < 2 && placed < target_chests; ++pass)
    {
        for (int i = 0; i < graph->room_count && placed < target_chests; ++i)
        {
            const RogueDungeonRoom* r = &graph->rooms[i];
            int is_dead_end = (deg[i] == 1);
            int is_milestone = (r->tag & milestone_mask) != 0;
            if ((pass == 0 && (is_dead_end || is_milestone)) || (pass == 1 && !is_dead_end))
            {
                int x = 0, y = 0;
                if (!pick_room_interior_xy(ctx, io_map, r, &x, &y))
                    continue;
                int tier = sample_reward_tier(ctx, (dist[i] >= 0 ? dist[i] : 0));
                /* Write overlay deco code: 10..13 represent tiers 0..3 */
                if (io_map->overlay_deco && io_map->overlay_magic == 0xDEC00EAU)
                    rogue_tilemap_set_deco(io_map, x, y, (unsigned char) (10 + tier));
                if (out_array && placed < max_out)
                    out_array[placed] = (RogueDungeonChestPlacement){x, y, tier, 0};
                placed++;
            }
        }
    }

    /* Upgrade guarantee: if chests exist, stamp an upgrade marker (overlay 14) NEAR a chest in
     * the deepest room by BFS depth, without overwriting the chest's tier marker (10..13).
     * We try to place code 14 on an adjacent empty overlay tile that is also a dungeon floor. */
    if (placed > 0)
    {
        int best = 0, bestd = -1;
        for (int i = 0; i < graph->room_count; ++i)
            if (dist[i] > bestd)
            {
                bestd = dist[i];
                best = i;
            }
        int marked = 0;
        if (io_map->overlay_deco && io_map->overlay_magic == 0xDEC00EAU)
        {
            /* Pass A: deepest room – find any chest tile and place 14 adjacent */
            for (int y = graph->rooms[best].y + 1;
                 y < graph->rooms[best].y + graph->rooms[best].h - 1 && !marked; ++y)
                for (int x = graph->rooms[best].x + 1;
                     x < graph->rooms[best].x + graph->rooms[best].w - 1 && !marked; ++x)
                {
                    unsigned char d = rogue_tilemap_get_deco(io_map, x, y);
                    if (d >= 10 && d <= 13)
                    {
                        if (try_place_upgrade_adjacent(io_map, x, y))
                        {
                            if (out_upgrade_count)
                                *out_upgrade_count = 1;
                            marked = 1;
                        }
                    }
                }
            /* Pass B: global fallback – find first chest tile anywhere and place adjacent */
            if (!marked)
            {
                for (int y = 0; y < io_map->height && !marked; ++y)
                    for (int x = 0; x < io_map->width && !marked; ++x)
                    {
                        unsigned char d = rogue_tilemap_get_deco(io_map, x, y);
                        if (d >= 10 && d <= 13)
                        {
                            if (try_place_upgrade_adjacent(io_map, x, y))
                            {
                                if (out_upgrade_count)
                                    *out_upgrade_count = 1;
                                marked = 1;
                            }
                        }
                    }
            }
        }
    }

    free(q);
    free(dist);
    free(deg);
    return placed;
}

int rogue_dungeon_seed_material_nodes(RogueWorldGenContext* ctx, RogueTileMap* io_map,
                                      const RogueDungeonGraph* graph, int max_nodes)
{
    if (!ctx || !io_map || !graph || max_nodes <= 0)
        return 0;
    int placed = 0;
    for (int i = 0; i < graph->room_count && placed < max_nodes; ++i)
    {
        const RogueDungeonRoom* r = &graph->rooms[i];
        /* Bias placement toward treasure or secret rooms */
        if (!(r->tag & ROGUE_DUNGEON_ROOM_TREASURE) && !r->secret)
            continue;
        /* small chance (depth weighted) */
        unsigned int roll = rogue_worldgen_rand_u32(&ctx->micro_rng) % 100u;
        int chance = 20; /* 20% base chance */
        if ((int) roll >= chance)
            continue;
        int x = 0, y = 0;
        if (!pick_room_interior_xy(ctx, io_map, r, &x, &y))
            continue;
        if (io_map->overlay_deco && io_map->overlay_magic == 0xDEC00EAU)
            rogue_tilemap_set_deco(io_map, x, y, 50); /* material node marker */
        placed++;
    }
    return placed;
}
