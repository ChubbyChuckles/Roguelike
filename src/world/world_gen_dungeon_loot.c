/**
 * @file world_gen_dungeon_loot.c
 * @brief Phase 8: Loot & Reward orchestration for dungeon generation (chests + materials).
 */

#include "../util/loadout_optimizer.h"
#include "world_gen.h"
/* Loot systems: item instances & base definitions used by upgrade heuristic */
#include "../core/loot/loot_instances.h"
#include "../core/loot/loot_item_defs.h"
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

/* Tier distribution by depth with EV-normalized weighting.
 * base tier = clamp(depth/3, 0..3).
 * We sample around base with probabilities skewed by bump_count (0..2):
 *   start p_low=0.30 (base-1), p_mid=0.60 (base), p_high=0.10 (base+1)
 *   for each bump, shift 0.15 from low to high (bounded), keeping mid as remainder.
 * This yields modest EV increases for optional/challenge rooms without exceeding caps. */
static int sample_reward_tier_weighted(RogueWorldGenContext* ctx, int depth, int bump_count)
{
    if (bump_count < 0)
        bump_count = 0;
    if (bump_count > 2)
        bump_count = 2;

    int base = depth / 3; /* every 3 depth -> +1 tier */
    if (base < 0)
        base = 0;
    if (base > 3)
        base = 3;

    /* use basis points (0..1000) to avoid float */
    int p_low = 300;  /* base-1 */
    int p_high = 100; /* base+1 */
    for (int i = 0; i < bump_count; ++i)
    {
        int delta = 150;    /* 0.15 */
        int min_low = 50;   /* keep at least 0.05 */
        int max_high = 500; /* cap at 0.50 */
        if (p_low - delta < min_low)
            delta = p_low - min_low;
        if (p_high + delta > max_high)
            delta = max_high - p_high;
        if (delta > 0)
        {
            p_low -= delta;
            p_high += delta;
        }
    }
    int p_mid = 1000 - (p_low + p_high);
    if (p_mid < 0)
        p_mid = 0; /* should not happen but clamp */

    unsigned int r = rogue_worldgen_rand_u32(&ctx->micro_rng) % 1000u;
    if (r < (unsigned) p_low)
        return (base > 0) ? base - 1 : 0;
    if (r < (unsigned) (p_low + p_mid))
        return base;
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

/* ---- Phase 8.3: Smart drop coupling – upgrade heuristic & test override ---- */
static int g_upgrade_possible_override = -1; /* -1 none, 0 force no, 1 force yes */
void rogue_dungeon_set_upgrade_possible_override(int value) { g_upgrade_possible_override = value; }

int rogue_dungeon_upgrade_possible(void)
{
    if (g_upgrade_possible_override == 0)
        return 0;
    if (g_upgrade_possible_override == 1)
        return 1;
    /* Heuristic: if any equipped slot has an instance with rarity below EPIC (3) and
       there exists at least one active item definition of the same broad category with
       strictly higher base stat proxy (base_damage_max for weapons, base_armor for armor),
       then an upgrade is possible. Keep light and side-effect free. */
    RogueLoadoutSnapshot snap;
    if (rogue_loadout_snapshot(&snap) != 0)
        return -1;

    /* Scan equipped items to see if any slot is maxed at high rarity. If all equipped are EPIC
       or empty, still allow upgrades if empty slots exist. */
    int any_empty_slot = 0;
    int any_below_epic = 0;
    for (int s = 0; s < ROGUE_EQUIP_SLOT_COUNT; ++s)
    {
        int inst = snap.inst_indices[s];
        if (inst < 0)
        {
            any_empty_slot = 1;
            continue;
        }
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        const RogueItemDef* def_cur = rogue_item_def_at(it->def_index);
        if (!def_cur)
            continue;
        if (def_cur->rarity < 3)
            any_below_epic = 1;
    }

    /* If fully kitted with EPIC everywhere, treat as top-tier -> no guarantee. */
    if (!any_below_epic && !any_empty_slot)
        return 0;

    /* Look for strictly better candidate by category for any equipped piece below EPIC. */
    for (int s = 0; s < ROGUE_EQUIP_SLOT_COUNT; ++s)
    {
        int inst = snap.inst_indices[s];
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        const RogueItemDef* def_cur = (it ? rogue_item_def_at(it->def_index) : NULL);
        int cur_cat = def_cur ? def_cur->category : -1;
        int cur_damage = def_cur ? def_cur->base_damage_max : 0;
        int cur_armor = def_cur ? def_cur->base_armor : 0;

        /* Iterate item defs registry for a strictly better item in same broad category */
        int total_defs = rogue_item_defs_count();
        for (int di = 0; di < total_defs; ++di)
        {
            const RogueItemDef* d = rogue_item_def_at(di);
            if (!d)
                continue;
            if (cur_cat == -1)
            {
                /* Empty slot can accept a category item -> treat as upgrade potential */
                if (d->category == ROGUE_ITEM_WEAPON || d->category == ROGUE_ITEM_ARMOR)
                    return 1;
                continue;
            }
            if (d->category != cur_cat)
                continue;
            if (d->rarity > (def_cur ? def_cur->rarity : -1))
                return 1;
            /* Also allow stat increase at same rarity as upgrade */
            if (cur_cat == ROGUE_ITEM_WEAPON && d->base_damage_max > cur_damage)
                return 1;
            if (cur_cat == ROGUE_ITEM_ARMOR && d->base_armor > cur_armor)
                return 1;
        }
    }
    return 0;
}

/* Plan smart-drop contents for a chosen upgrade chest placement (index k in out_array).
 * Populates planned_def_index and planned_rarity conservatively using the current equipment
 * snapshot. No-op if out_array is NULL or k out of bounds. */
static void rogue__plan_upgrade_for_chest(RogueDungeonChestPlacement* out_array, int k)
{
    if (!out_array || k < 0)
        return;
    RogueDungeonChestPlacement* cp = &out_array[k];
    /* Only plan once */
    if (cp->planned_def_index >= 0 && cp->planned_rarity >= 0)
        return;
    RogueLoadoutSnapshot snap;
    if (rogue_loadout_snapshot(&snap) != 0)
        return;

    const RogueItemDef* cur_def = NULL;
    int cur_rarity = 0;
    int focus_slot = -1;
    /* Prefer weapon if equipped, else an armor below EPIC */
    for (int s = 0; s < ROGUE_EQUIP_SLOT_COUNT; ++s)
    {
        int inst = snap.inst_indices[s];
        if (inst < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        const RogueItemDef* curd = rogue_item_def_at(it->def_index);
        if (!curd)
            continue;
        if (curd->category == ROGUE_ITEM_WEAPON)
        {
            focus_slot = s;
            cur_def = curd;
            cur_rarity = curd->rarity;
            break;
        }
    }
    if (focus_slot < 0)
    {
        for (int s = 0; s < ROGUE_EQUIP_SLOT_COUNT; ++s)
        {
            int inst = snap.inst_indices[s];
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            const RogueItemDef* slot_def = (it ? rogue_item_def_at(it->def_index) : NULL);
            if (slot_def && slot_def->category == ROGUE_ITEM_ARMOR && slot_def->rarity < 3)
            {
                focus_slot = s;
                cur_def = slot_def;
                cur_rarity = slot_def->rarity;
                break;
            }
        }
    }

    int want_cat = -1;
    int stat_floor = 0;
    if (focus_slot >= 0 && cur_def)
    {
        want_cat = cur_def->category;
        stat_floor =
            (want_cat == ROGUE_ITEM_WEAPON) ? cur_def->base_damage_max : cur_def->base_armor;
    }
    if (want_cat < 0)
    {
        /* Fallback: prefer weapons if available, else armor; chest tier nudges rarity only */
        int total_defs0 = rogue_item_defs_count();
        int any_weapon = 0, any_armor = 0;
        for (int di = 0; di < total_defs0; ++di)
        {
            const RogueItemDef* d = rogue_item_def_at(di);
            if (!d)
                continue;
            if (d->category == ROGUE_ITEM_WEAPON)
                any_weapon = 1;
            else if (d->category == ROGUE_ITEM_ARMOR)
                any_armor = 1;
        }
        if (cp->tier >= 2)
            want_cat = any_weapon ? ROGUE_ITEM_WEAPON : (any_armor ? ROGUE_ITEM_ARMOR : -1);
        else
            want_cat = any_armor ? ROGUE_ITEM_ARMOR : (any_weapon ? ROGUE_ITEM_WEAPON : -1);
        stat_floor = 0;
        cur_rarity = 1;
    }

    int best_def = -1;
    int best_score = -1;
    int total_defs = rogue_item_defs_count();
    for (int di = 0; di < total_defs; ++di)
    {
        const RogueItemDef* cand = rogue_item_def_at(di);
        if (!cand || cand->category != want_cat)
            continue;
        int stat = (want_cat == ROGUE_ITEM_WEAPON) ? cand->base_damage_max : cand->base_armor;
        if (stat <= stat_floor)
            continue;
        int rar = cand->rarity;
        if (rar > 3)
            rar = 3;
        int score = rar * 1000 + (stat - stat_floor);
        if (score > best_score)
        {
            best_score = score;
            best_def = di;
        }
    }
    /* Retry in alternate category if none found */
    if (best_def < 0)
    {
        int alt_cat = (want_cat == ROGUE_ITEM_WEAPON) ? ROGUE_ITEM_ARMOR : ROGUE_ITEM_WEAPON;
        for (int di = 0; di < total_defs; ++di)
        {
            const RogueItemDef* cand = rogue_item_def_at(di);
            if (!cand || cand->category != alt_cat)
                continue;
            int stat = (alt_cat == ROGUE_ITEM_WEAPON) ? cand->base_damage_max : cand->base_armor;
            if (stat <= 0)
                continue;
            int rar = cand->rarity;
            if (rar > 3)
                rar = 3;
            int score = rar * 1000 + stat;
            if (score > best_score)
            {
                best_score = score;
                best_def = di;
                want_cat = alt_cat;
                stat_floor = 0;
            }
        }
    }
    if (best_def >= 0)
    {
        cp->planned_def_index = best_def;
        int pr = cur_rarity;
        if (pr < 1)
            pr = 1;
        if (pr > 3)
            pr = 3;
        pr += (cp->tier >= 2) ? 1 : 0;
        if (pr > 3)
            pr = 3;
        cp->planned_rarity = pr;
    }
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
    int placed_out = 0; /* number of valid entries written to out_array (<= max_out) */
    int* deg = (int*) calloc((size_t) graph->room_count, sizeof(int));
    if (!deg)
        return 0;
    compute_degrees(graph, deg);
    /* Build BFS arrays to approximate a main path (0 -> deepest) for optional-branch weighting. */
    int* dist = (int*) malloc(sizeof(int) * (size_t) graph->room_count);
    int* parent = (int*) malloc(sizeof(int) * (size_t) graph->room_count);
    int* q = (int*) malloc(sizeof(int) * (size_t) graph->room_count);
    if (!dist || !parent || !q)
    {
        free(deg);
        if (dist)
            free(dist);
        if (parent)
            free(parent);
        if (q)
            free(q);
        return 0;
    }
    for (int i = 0; i < graph->room_count; ++i)
    {
        dist[i] = -1;
        parent[i] = -1;
    }
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
            /* Guard against malformed edges referencing out-of-range rooms */
            if (v >= 0 && v < graph->room_count && dist[v] < 0)
            {
                dist[v] = dist[u] + 1;
                parent[v] = u;
                if (qe < graph->room_count)
                    q[qe++] = v;
            }
        }
    }

    /* Compute an approximate critical path from start (0) to deepest room to identify
     * optional branches. */
    int deepest = 0, deepest_d = -1;
    for (int i = 0; i < graph->room_count; ++i)
        if (dist[i] > deepest_d)
        {
            deepest_d = dist[i];
            deepest = i;
        }
    int* on_path = (int*) calloc((size_t) graph->room_count, sizeof(int));
    if (on_path)
    {
        int cur = deepest;
        while (cur >= 0)
        {
            on_path[cur] = 1;
            if (cur == 0)
                break;
            cur = parent[cur];
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
                int bump_count = 0;
                if (on_path && !on_path[i])
                    bump_count++;
                if ((r->tag & (ROGUE_DUNGEON_ROOM_ELITE | ROGUE_DUNGEON_ROOM_PUZZLE)) != 0)
                    bump_count++;
                int tier =
                    sample_reward_tier_weighted(ctx, (dist[i] >= 0 ? dist[i] : 0), bump_count);
                /* Write overlay deco code: 10..13 represent tiers 0..3 */
                if (io_map->overlay_deco && io_map->overlay_magic == 0xDEC00EAU)
                    rogue_tilemap_set_deco(io_map, x, y, (unsigned char) (10 + tier));
                if (out_array && placed_out < max_out)
                {
                    out_array[placed_out++] =
                        (RogueDungeonChestPlacement){x, y, tier, 0, i, -1, -1};
                }
                placed++;
            }
        }
    }

    /* Upgrade guarantee: if chests exist, stamp an upgrade marker (overlay 14) NEAR a chest in
     * the deepest room by BFS depth, without overwriting the chest's tier marker (10..13).
     * We try to place code 14 on an adjacent empty overlay tile that is also a dungeon floor. */
    if (placed > 0)
    {
        int best = deepest;
        int marked = 0;
        if (io_map->overlay_deco && io_map->overlay_magic == 0xDEC00EAU)
        {
            /* Check upgrade-possible heuristic; if not possible, skip guarantee marker */
            int can_upgrade = rogue_dungeon_upgrade_possible();
            if (can_upgrade <= 0)
            {
                /* Leave out_upgrade_count as previously set (likely 0) and skip marker. */
                goto SKIP_UPGRADE_MARKER;
            }
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
                            /* Mark is_upgrade for the matching chest in out_array if provided */
                            if (out_array && placed_out > 0)
                            {
                                for (int k = 0; k < placed_out; ++k)
                                {
                                    if (out_array[k].x == x && out_array[k].y == y)
                                    {
                                        out_array[k].is_upgrade = 1;
                                        /* Smart-drop planning */
                                        rogue__plan_upgrade_for_chest(out_array, k);
                                        break;
                                    }
                                }
                            }
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
                                if (out_array && placed_out > 0)
                                {
                                    for (int k = 0; k < placed_out; ++k)
                                    {
                                        if (out_array[k].x == x && out_array[k].y == y)
                                        {
                                            out_array[k].is_upgrade = 1;
                                            /* Ensure planned contents are set in fallback too */
                                            rogue__plan_upgrade_for_chest(out_array, k);
                                            break;
                                        }
                                    }
                                }
                                marked = 1;
                            }
                        }
                    }
            }
        }
    SKIP_UPGRADE_MARKER:;
    }

    free(q);
    free(parent);
    free(dist);
    free(deg);
    if (on_path)
        free(on_path);
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

int rogue_dungeon_debug_sample_reward_tier(int depth, int bump_count, int reps, int out_counts[4])
{
    if (!out_counts || reps <= 0)
        return -1;
    out_counts[0] = out_counts[1] = out_counts[2] = out_counts[3] = 0;

    /* Build a minimal worldgen context with a deterministic seed; use micro_rng channel. */
    RogueWorldGenConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.seed = 424242; /* fixed seed for deterministic test sampling */
    cfg.width = 16;
    cfg.height = 16;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    for (int i = 0; i < reps; ++i)
    {
        int t = sample_reward_tier_weighted(&ctx, depth, bump_count);
        if (t < 0)
            t = 0;
        if (t > 3)
            t = 3;
        out_counts[t]++;
    }

    rogue_worldgen_context_shutdown(&ctx);
    return 0;
}
