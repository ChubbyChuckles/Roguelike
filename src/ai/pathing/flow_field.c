/**
 * @file flow_field.c
 * @brief Flow field pathfinding implementation using Dijkstra's algorithm.
 *
 * This module provides flow field pathfinding capabilities for AI agents. Flow fields
 * precompute optimal movement directions from any point on the map toward a target
 * location, enabling efficient crowd movement and coordinated AI behavior.
 *
 * The implementation uses Dijkstra's algorithm to compute shortest paths from the
 * target to all reachable cells, storing both distance information and directional
 * vectors for each cell. This allows agents to make optimal movement decisions
 * without needing to recalculate paths individually.
 *
 * Key features:
 * - Dijkstra-based pathfinding with tile movement costs
 * - Precomputed directional vectors for efficient agent movement
 * - Memory-efficient storage using distance arrays and direction vectors
 * - Integration with navigation system for blocking and cost calculations
 */

#include "flow_field.h"
#include "../../core/app/app_state.h"
#include <math.h>
#include <stdlib.h>

/**
 * @brief Internal node structure for Dijkstra's algorithm priority queue.
 *
 * Used in the Dijkstra implementation to store nodes in the priority queue,
 * containing position coordinates and distance from the target.
 */
typedef struct FFNode
{
    int x, y;     /**< World coordinates of this node */
    float dist;   /**< Distance from target (used for priority queue ordering) */
} FFNode;

/**
 * @brief Converts 2D coordinates to 1D array index.
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width of the 2D array
 * @return Linear array index
 */
static inline int idx(int x, int y, int w) { return y * w + x; }

/**
 * @brief Builds a flow field pointing toward a target location using Dijkstra's algorithm.
 *
 * This function computes a complete flow field from all reachable cells on the map
 * toward the specified target coordinates. The algorithm:
 * 1. Initializes distance arrays and direction vectors
 * 2. Uses Dijkstra's algorithm with a priority queue to compute shortest paths
 * 3. Stores directional vectors pointing toward the target for each reachable cell
 * 4. Handles tile movement costs and blocking terrain
 *
 * The resulting flow field can be queried to get optimal movement directions
 * from any point on the map toward the target.
 *
 * @param tx Target X coordinate
 * @param ty Target Y coordinate
 * @param out_ff Pointer to flow field structure to populate
 * @return 1 on success, 0 on failure (invalid parameters, out of bounds, or memory allocation failure)
 */
int rogue_flow_field_build(int tx, int ty, RogueFlowField* out_ff)
{
    if (!out_ff)
        return 0;
    int w = g_app.world_map.width, h = g_app.world_map.height;
    if (w <= 0 || h <= 0)
        return 0;
    if (tx < 0 || ty < 0 || tx >= w || ty >= h)
        return 0;

    size_t n = (size_t) w * (size_t) h;
    float* dist = (float*) malloc(n * sizeof(float));
    signed char* dirx = (signed char*) malloc(n);
    signed char* diry = (signed char*) malloc(n);
    if (!dist || !dirx || !diry)
    {
        free(dist);
        free(dirx);
        free(diry);
        return 0;
    }
    for (size_t i = 0; i < n; ++i)
    {
        dist[i] = INFINITY;
        dirx[i] = 0;
        diry[i] = 0;
    }

    /* Dijkstra from target to all cells (reverse) */
    const int cap = w * h;
    FFNode* heap = (FFNode*) malloc((size_t) cap * sizeof(FFNode));
    int heap_size = 0;
    if (!heap)
    {
        free(dist);
        free(dirx);
        free(diry);
        return 0;
    }

    if (!rogue_nav_is_blocked(tx, ty))
    {
        dist[idx(tx, ty, w)] = 0.0f;
        heap[heap_size++] = (FFNode){tx, ty, 0.0f};
    }
    /* Simple linear pop-min loop (cap moderate); could be optimized later */
    while (heap_size > 0)
    {
        int min_i = 0;
        for (int i = 1; i < heap_size; ++i)
            if (heap[i].dist < heap[min_i].dist)
                min_i = i;
        FFNode cur = heap[min_i];
        heap[min_i] = heap[--heap_size];
        int cx = cur.x, cy = cur.y;
        float base = dist[idx(cx, cy, w)];
        /* Visit 4 neighbors and relax with forward costs */
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; ++d)
        {
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;
            if (rogue_nav_is_blocked(nx, ny))
                continue;
            float step = rogue_nav_tile_cost(cx, cy); /* cost to leave neighbor toward cur */
            float nd = base + step;
            int nidx = idx(nx, ny, w);
            if (nd < dist[nidx])
            {
                dist[nidx] = nd;
                /* Direction at neighbor should point toward current cell (to target) */
                dirx[nidx] = (signed char) (-dirs[d][0]);
                diry[nidx] = (signed char) (-dirs[d][1]);
                heap[heap_size++] = (FFNode){nx, ny, nd};
            }
        }
    }

    out_ff->width = w;
    out_ff->height = h;
    out_ff->dist = dist;
    out_ff->dir_x = dirx;
    out_ff->dir_y = diry;
    out_ff->target_x = tx;
    out_ff->target_y = ty;
    free(heap);
    return 1;
}

/**
 * @brief Frees all heap memory associated with a flow field.
 *
 * Deallocates the distance and direction arrays owned by the flow field
 * and resets all fields to safe default values. After calling this function,
 * the flow field structure should not be used until rebuilt.
 *
 * @param ff Pointer to the flow field to free
 */
void rogue_flow_field_free(RogueFlowField* ff)
{
    if (!ff)
        return;
    free(ff->dist);
    ff->dist = NULL;
    free(ff->dir_x);
    ff->dir_x = NULL;
    free(ff->dir_y);
    ff->dir_y = NULL;
    ff->width = ff->height = 0;
    ff->target_x = ff->target_y = 0;
}

/**
 * @brief Gets the recommended movement direction from a position toward the target.
 *
 * Queries the flow field to determine the optimal cardinal movement direction
 * from the specified position toward the target. The direction is computed
 * during flow field construction and represents the first step along the
 * shortest path to the target.
 *
 * @param ff Pointer to the flow field to query
 * @param x Current X position
 * @param y Current Y position
 * @param out_dx Pointer to store the X movement delta (-1, 0, or 1)
 * @param out_dy Pointer to store the Y movement delta (-1, 0, or 1)
 * @return 0 on success, -1 on invalid parameters, -2 on out-of-bounds position, 1 if unreachable (stays put)
 */
int rogue_flow_field_step(const RogueFlowField* ff, int x, int y, int* out_dx, int* out_dy)
{
    if (!ff || !out_dx || !out_dy)
        return -1;
    if (x < 0 || y < 0 || x >= ff->width || y >= ff->height)
        return -2;
    int i = idx(x, y, ff->width);
    if (!isfinite(ff->dist[i]))
    {
        *out_dx = 0;
        *out_dy = 0;
        return 1;
    }
    *out_dx = ff->dir_x[i];
    *out_dy = ff->dir_y[i];
    return 0;
}
