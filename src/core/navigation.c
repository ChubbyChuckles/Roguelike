#include "core/navigation.h"
#include "core/app_state.h"
#include "core/vegetation/vegetation.h"
#include <math.h>
#include <stdlib.h>

static int tile_block(unsigned char t)
{
    switch (t)
    {
    case ROGUE_TILE_WATER:
    case ROGUE_TILE_RIVER:
    case ROGUE_TILE_RIVER_WIDE:
    case ROGUE_TILE_RIVER_DELTA:
    case ROGUE_TILE_MOUNTAIN:
    case ROGUE_TILE_CAVE_WALL:
        return 1;
    default:
        return 0;
    }
}

int rogue_nav_is_blocked(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_app.world_map.width || ty >= g_app.world_map.height)
        return 1;
    unsigned char t = g_app.world_map.tiles[ty * g_app.world_map.width + tx];
    if (tile_block(t))
        return 1;
    if (rogue_vegetation_tile_blocking(tx, ty))
        return 1;
    return 0;
}

float rogue_nav_tile_cost(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g_app.world_map.width || ty >= g_app.world_map.height)
        return 9999.0f;
    float base = 1.0f;
    float scale = rogue_vegetation_tile_move_scale(tx, ty); /* 1 or <1 */
    if (scale < 0.999f)
    {
        base *= (1.0f / scale);
    } /* slower move => higher cost */
    return base;
}

void rogue_nav_cardinal_step_towards(float sx, float sy, float tx, float ty, int* out_dx,
                                     int* out_dy)
{
    int best_dx = 0, best_dy = 0;
    float best_score = 1e9f;
    int dir_candidates[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; i++)
    {
        int dx = dir_candidates[i][0];
        int dy = dir_candidates[i][1];
        int nx = (int) floorf(sx + dx + 0.5f);
        int ny = (int) floorf(sy + dy + 0.5f);
        if (rogue_nav_is_blocked(nx, ny))
            continue;
        float manhattan = fabsf((tx - nx)) + fabsf((ty - ny));
        float cost = rogue_nav_tile_cost(nx, ny);
        float score = manhattan + cost * 0.15f; /* weight cost lightly */
        if (score < best_score)
        {
            best_score = score;
            best_dx = dx;
            best_dy = dy;
        }
    }
    *out_dx = best_dx;
    *out_dy = best_dy; /* (0,0) if no move */
}

/* -------- A* Implementation (cardinal, cost from vegetation) -------- */
typedef struct NodeRec
{
    unsigned short x, y;
    float g, f;
    unsigned short parent_index;
    unsigned char open;
    unsigned char closed;
} NodeRec;
/* Simple fixed-size grid-limited open set using linear scan for min f (sufficient for moderate map
 * sizes) */
int rogue_nav_astar(int sx, int sy, int tx, int ty, RoguePath* out_path)
{
    if (!out_path)
        return 0;
    out_path->length = 0;
    out_path->failed = 0;
    out_path->truncated = 0;
    if (sx == tx && sy == ty)
    {
        out_path->xs[0] = sx;
        out_path->ys[0] = sy;
        out_path->length = 1;
        return 1;
    }
    if (rogue_nav_is_blocked(sx, sy) || rogue_nav_is_blocked(tx, ty))
    {
        out_path->failed = 1;
        return 0;
    }
    int w = g_app.world_map.width, h = g_app.world_map.height;
    if (w <= 0 || h <= 0)
    {
        out_path->failed = 1;
        return 0;
    }
    int max_nodes = w * h;
    if (max_nodes > 32768)
        max_nodes = 32768; /* clamp */
    NodeRec* nodes = (NodeRec*) malloc((size_t) max_nodes * sizeof(NodeRec));
    if (!nodes)
    {
        out_path->failed = 1;
        return 0;
    }
    for (int i = 0; i < max_nodes; i++)
    {
        nodes[i].open = nodes[i].closed = 0;
        nodes[i].g = 1e30f;
        nodes[i].f = 1e30f;
        nodes[i].parent_index = 0xffff;
    }
#define NODE_INDEX(X, Y) ((Y) * w + (X))
    int start_i = NODE_INDEX(sx, sy);
    int goal_i = NODE_INDEX(tx, ty);
    nodes[start_i].g = 0.0f;
    float h0 = fabsf((float) tx - sx) + fabsf((float) ty - sy);
    nodes[start_i].f = h0;
    nodes[start_i].open = 1;
    int open_count = 1;
    int found = 0;
    int iter = 0, iter_limit = 40000; /* safety */
    while (open_count > 0 && iter < iter_limit)
    {
        iter++;
        /* find best f */
        int best = -1;
        float best_f = 1e30f;
        for (int i = 0; i < max_nodes; i++)
            if (nodes[i].open && !nodes[i].closed)
            {
                if (nodes[i].f < best_f)
                {
                    best_f = nodes[i].f;
                    best = i;
                }
            }
        if (best < 0)
            break;
        NodeRec* cur = &nodes[best];
        cur->closed = 1;
        cur->open = 0;
        open_count--;
        if (best == goal_i)
        {
            found = 1;
            break;
        }
        int cx = best % w;
        int cy = best / w;
        static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; d++)
        {
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;
            if (rogue_nav_is_blocked(nx, ny))
                continue;
            int ni = NODE_INDEX(nx, ny);
            NodeRec* nn = &nodes[ni];
            if (nn->closed)
                continue;
            float step_cost = rogue_nav_tile_cost(nx, ny);
            float tentative_g = cur->g + step_cost;
            if (tentative_g < nn->g)
            {
                nn->g = tentative_g;
                float hcost = fabsf((float) tx - nx) + fabsf((float) ty - ny);
                nn->f = tentative_g + hcost;
                nn->parent_index = (unsigned short) best;
                if (!nn->open)
                {
                    nn->open = 1;
                    open_count++;
                }
            }
        }
    }
    if (!found)
    {
        free(nodes);
        out_path->failed = 1;
        return 0;
    }
    /* Reconstruct */
    int chain[ROGUE_PATH_MAX_POINTS];
    int clen = 0;
    int cur_i = goal_i;
    while (cur_i != start_i && clen < ROGUE_PATH_MAX_POINTS)
    {
        chain[clen++] = cur_i;
        cur_i = nodes[cur_i].parent_index;
        if (cur_i < 0 || cur_i >= max_nodes)
        {
            break;
        }
    }
    if (cur_i == start_i)
    {
        chain[clen++] = start_i;
    }
    else
    {
        out_path->failed = 1;
        free(nodes);
        return 0;
    }
    /* Reverse into out_path (start -> goal) */
    for (int i = 0; i < clen; i++)
    {
        int idx = chain[clen - 1 - i];
        int px = idx % w;
        int py = idx / w;
        if (i < ROGUE_PATH_MAX_POINTS)
        {
            out_path->xs[i] = px;
            out_path->ys[i] = py;
        }
        else
        {
            out_path->truncated = 1;
            break;
        }
    }
    out_path->length = (clen > ROGUE_PATH_MAX_POINTS) ? ROGUE_PATH_MAX_POINTS : clen;
    free(nodes);
    return 1;
}

int rogue_nav_path_simplify(const RoguePath* in_path, RoguePath* out_path)
{
    if (!in_path || !out_path || in_path->failed || in_path->length <= 0)
        return 0;
    out_path->failed = in_path->failed;
    out_path->truncated = 0;
    if (in_path->length == 1)
    {
        out_path->xs[0] = in_path->xs[0];
        out_path->ys[0] = in_path->ys[0];
        out_path->length = 1;
        return 1;
    }
    int out_len = 0;
    int last_dx = 0, last_dy = 0;
    /* Always include first point */
    out_path->xs[out_len] = in_path->xs[0];
    out_path->ys[out_len] = in_path->ys[0];
    out_len++;
    for (int i = 1; i < in_path->length; ++i)
    {
        int px = in_path->xs[i - 1];
        int py = in_path->ys[i - 1];
        int cx = in_path->xs[i];
        int cy = in_path->ys[i];
        int dx = cx - px;
        int dy = cy - py;
        /* Sanity: ensure cardinal step in input; if not, treat as break */
        int man = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (man != 1)
        {
            /* Force a breakpoint to preserve connectivity */
            last_dx = 0;
            last_dy = 0;
        }
        if (i == 1)
        {
            last_dx = dx;
            last_dy = dy;
        }
        /* If direction changes, emit previous point as a corner */
        if (dx != last_dx || dy != last_dy)
        {
            if (out_len < ROGUE_PATH_MAX_POINTS)
            {
                out_path->xs[out_len] = px;
                out_path->ys[out_len] = py;
                out_len++;
            }
            else
            {
                out_path->truncated = 1;
            }
            last_dx = dx;
            last_dy = dy;
        }
    }
    /* Always include final point */
    int end_x = in_path->xs[in_path->length - 1];
    int end_y = in_path->ys[in_path->length - 1];
    if (out_len < ROGUE_PATH_MAX_POINTS)
    {
        out_path->xs[out_len] = end_x;
        out_path->ys[out_len] = end_y;
        out_len++;
    }
    else
    {
        out_path->truncated = 1;
    }
    out_path->length = out_len;
    return out_len;
}
