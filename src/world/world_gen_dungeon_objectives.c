#include "world_gen_dungeon_objectives.h"
#include "world_gen_dungeon_kernel.h"
#include <stdlib.h>
#include <string.h>

static int compute_room_depths_obj(const RogueDungeonGraph* g, int* out_depths)
{
    if (!g || g->room_count <= 0)
        return 0;
    int n = g->room_count;
    for (int i = 0; i < n; ++i)
        out_depths[i] = -1;
    int* q = (int*) malloc((size_t) n * sizeof(int));
    if (!q)
        return 0;
    int qs = 0, qe = 0;
    out_depths[0] = 0;
    q[qe++] = 0;
    while (qs < qe)
    {
        int cur = q[qs++];
        int cd = out_depths[cur];
        for (int e = 0; e < g->edge_count; ++e)
        {
            int a = g->edges[e].a, b = g->edges[e].b;
            int nxt = -1;
            if (a == cur)
                nxt = b;
            else if (b == cur)
                nxt = a;
            if (nxt >= 0 && nxt < n && out_depths[nxt] == -1)
            {
                out_depths[nxt] = cd + 1;
                q[qe++] = nxt;
            }
        }
    }
    free(q);
    /* Validate connectivity */
    for (int i = 0; i < n; ++i)
        if (out_depths[i] < 0)
            return 0;
    return 1;
}

int rogue_dungeon_build_objectives(RogueWorldGenContext* ctx, const RogueDungeonGraph* graph,
                                   int depth, RogueDungeonObjectiveStep* out, int max_steps)
{
    (void) depth;
    if (!ctx || !graph || !out || max_steps < 3 || graph->room_count <= 0)
        return 0;

    int n = graph->room_count;
    int* rdepths = (int*) malloc((size_t) n * sizeof(int));
    if (!rdepths)
        return 0;
    if (!compute_room_depths_obj(graph, rdepths))
    {
        free(rdepths);
        return 0;
    }

    /* Find shallowest non-start candidate for ACTIVATE/CLEAR */
    int shallow_idx = 0; /* start room as fallback */
    int shallow_d = rdepths[0];
    for (int i = 1; i < n; ++i)
    {
        if (rdepths[i] < shallow_d)
        {
            shallow_d = rdepths[i];
            shallow_idx = i;
        }
    }

    /* Optional mid-step: PUZZLE_COMPLETE if any puzzle-tagged room exists */
    int puzzle_idx = -1;
    for (int i = 0; i < n; ++i)
    {
        if ((graph->rooms[i].tag & ROGUE_DUNGEON_ROOM_PUZZLE) != 0)
        {
            puzzle_idx = i;
            break;
        }
    }

    /* Farthest room (by BFS depth) becomes BOSS */
    int far_idx = 0;
    int far_d = rdepths[0];
    for (int i = 1; i < n; ++i)
    {
        if (rdepths[i] > far_d)
        {
            far_d = rdepths[i];
            far_idx = i;
        }
    }

    int steps = 0;
    if (steps < max_steps)
    {
        out[steps].step_index = steps;
        out[steps].type = ROGUE_OBJ_ACTIVATE;
        out[steps].room_id = graph->rooms[shallow_idx].id;
        out[steps].param0 = 0;
        steps++;
    }
    if (puzzle_idx >= 0 && steps < max_steps)
    {
        out[steps].step_index = steps;
        out[steps].type = ROGUE_OBJ_PUZZLE_COMPLETE;
        out[steps].room_id = graph->rooms[puzzle_idx].id;
        out[steps].param0 = 0;
        steps++;
    }
    /* Mid CLEAR: pick a median-depth room */
    if (steps < max_steps)
    {
        int target_depth = (far_d > 1) ? (far_d / 2) : 1;
        int mid_idx = shallow_idx;
        int best_dist = 9999;
        for (int i = 0; i < n; ++i)
        {
            int dist = rdepths[i] > target_depth ? (rdepths[i] - target_depth)
                                                 : (target_depth - rdepths[i]);
            if (dist < best_dist)
            {
                best_dist = dist;
                mid_idx = i;
            }
        }
        out[steps].step_index = steps;
        out[steps].type = ROGUE_OBJ_CLEAR;
        out[steps].room_id = graph->rooms[mid_idx].id;
        out[steps].param0 = 0;
        steps++;
    }
    if (steps < max_steps)
    {
        out[steps].step_index = steps;
        out[steps].type = ROGUE_OBJ_BOSS;
        out[steps].room_id = graph->rooms[far_idx].id;
        out[steps].param0 = 0;
        steps++;
    }

    free(rdepths);
    return steps;
}
