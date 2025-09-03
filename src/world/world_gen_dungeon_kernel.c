#include "world_gen_dungeon_kernel.h"
#include <stdlib.h>
#include <string.h>

static unsigned long long fnv1a64(const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    unsigned long long h = 1469598103934665603ULL; /* FNV offset */
    for (size_t i = 0; i < len; ++i)
    {
        h ^= (unsigned long long) p[i];
        h *= 1099511628211ULL; /* FNV prime */
    }
    return h;
}

/* Simple queue for BFS (fixed-capacity using room_count) */
static int bfs_farthest_node(const RogueDungeonGraph* g, int start, int* out_maxdist)
{
    if (!g || g->room_count <= 0)
        return -1;
    int n = g->room_count;
    int* dist = (int*) malloc((size_t) n * sizeof(int));
    if (!dist)
        return -1;
    for (int i = 0; i < n; ++i)
        dist[i] = -1;
    int* q = (int*) malloc((size_t) n * sizeof(int));
    if (!q)
    {
        free(dist);
        return -1;
    }
    int qs = 0, qe = 0;
    dist[start] = 0;
    q[qe++] = start;
    int far = start;
    while (qs < qe)
    {
        int u = q[qs++];
        if (dist[u] > dist[far])
            far = u;
        for (int e = 0; e < g->edge_count; ++e)
        {
            int a = g->edges[e].a;
            int b = g->edges[e].b;
            int v = -1;
            if (a == u)
                v = b;
            else if (b == u)
                v = a;
            if (v >= 0 && v < n && dist[v] == -1)
            {
                dist[v] = dist[u] + 1;
                q[qe++] = v;
            }
        }
    }
    int maxdist = dist[far];
    free(q);
    free(dist);
    if (out_maxdist)
        *out_maxdist = maxdist;
    return far;
}

int rogue_dungeon_graph_critical_path_length(const RogueDungeonGraph* g)
{
    if (!g || g->room_count <= 0)
        return -1;
    /* Pick arbitrary node 0, BFS to farthest node, BFS again from that node and return max distance
     */
    int tmpDist = 0;
    int u = bfs_farthest_node(g, 0, &tmpDist);
    if (u < 0)
        return -1;
    int diam = 0;
    (void) bfs_farthest_node(g, u, &diam);
    return diam;
}

void rogue_dungeon_graph_degree_stats(const RogueDungeonGraph* g, int* out_max_degree,
                                      double* out_avg_degree)
{
    int max_d = 0;
    double avg = 0.0;
    if (g && g->room_count > 0)
    {
        int n = g->room_count;
        int* deg = (int*) calloc((size_t) n, sizeof(int));
        if (deg)
        {
            for (int e = 0; e < g->edge_count; ++e)
            {
                int a = g->edges[e].a;
                int b = g->edges[e].b;
                if (a >= 0 && a < n)
                    deg[a]++;
                if (b >= 0 && b < n)
                    deg[b]++;
            }
            int sum = 0;
            for (int i = 0; i < n; ++i)
            {
                if (deg[i] > max_d)
                    max_d = deg[i];
                sum += deg[i];
            }
            avg = n ? ((double) sum / (double) n) : 0.0;
            free(deg);
        }
    }
    if (out_max_degree)
        *out_max_degree = max_d;
    if (out_avg_degree)
        *out_avg_degree = avg;
}

unsigned long long rogue_dungeon_graph_hash(const RogueDungeonGraph* g)
{
    if (!g)
        return 0ULL;
    /* Normalize edges as (min,max,loop) and sort; also incorporate room ids count */
    typedef struct
    {
        int a, b, loop;
    } EdgeN;
    int ec = g->edge_count;
    EdgeN* arr = NULL;
    if (ec > 0)
    {
        arr = (EdgeN*) malloc((size_t) ec * sizeof(EdgeN));
        if (!arr)
            return 0ULL;
        for (int i = 0; i < ec; ++i)
        {
            int a = g->edges[i].a, b = g->edges[i].b;
            arr[i].a = (a < b) ? a : b;
            arr[i].b = (a < b) ? b : a;
            arr[i].loop = g->edges[i].loop ? 1 : 0;
        }
        /* simple insertion sort (small edge counts typical) */
        for (int i = 1; i < ec; ++i)
        {
            EdgeN key = arr[i];
            int j = i - 1;
            while (j >= 0 &&
                   (arr[j].a > key.a ||
                    (arr[j].a == key.a &&
                     (arr[j].b > key.b || (arr[j].b == key.b && arr[j].loop > key.loop)))))
            {
                arr[j + 1] = arr[j];
                --j;
            }
            arr[j + 1] = key;
        }
    }
    unsigned long long h = 0xcbf29ce484222325ULL; /* start from FNV offset too */
    h ^= fnv1a64(&g->room_count, sizeof(g->room_count));
    h ^= fnv1a64(&g->edge_count, sizeof(g->edge_count));
    if (arr)
    {
        h ^= fnv1a64(arr, (size_t) ec * sizeof(EdgeN));
        free(arr);
    }
    return h;
}
