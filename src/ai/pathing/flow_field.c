#include "flow_field.h"
#include "../../core/app/app_state.h"
#include <math.h>
#include <stdlib.h>

typedef struct FFNode
{
    int x, y;
    float dist;
} FFNode;

static inline int idx(int x, int y, int w) { return y * w + x; }

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
