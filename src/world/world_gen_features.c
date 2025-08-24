/* Feature passes: caves, rivers, erosion, smoothing, advanced post */
#include "world_gen_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void wg_generate_caves(RogueTileMap* map, const RogueWorldGenConfig* cfg)
{
    int w = map->width, h = map->height;
    unsigned char* cave = (unsigned char*) malloc((size_t) w * h);
    if (!cave)
        return;
    for (int i = 0; i < w * h; i++)
        cave[i] = (rng_norm() < cfg->cave_fill_chance) ? 1 : 0;
    int iters = cfg->cave_iterations;
    for (int it = 0; it < iters; ++it)
    {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                int count = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (ox == 0 && oy == 0)
                            continue;
                        int nx = x + ox, ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        {
                            count++;
                        }
                        else if (cave[ny * w + nx])
                            count++;
                    }
                int idx = y * w + x;
                unsigned char cur = cave[idx];
                cave[idx] = cur ? (count >= 4 ? 1 : 0) : (count >= 5 ? 1 : 0);
            }
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            if (cave[y * w + x])
                map->tiles[y * w + x] = (unsigned char) ROGUE_TILE_CAVE_WALL;
            else if (map->tiles[y * w + x] == ROGUE_TILE_MOUNTAIN)
                map->tiles[y * w + x] = (unsigned char) ROGUE_TILE_CAVE_FLOOR;
        }
    free(cave);
}

static void carve_river_single(RogueTileMap* map)
{
    int w = map->width, h = map->height;
    int x = rng_range(0, w - 1);
    int y = 0;
    for (int steps = 0; steps < h * 2 && y < h; ++steps)
    {
        map->tiles[y * w + x] = (unsigned char) ROGUE_TILE_RIVER;
        int dir = rng_range(0, 9);
        if (dir < 5)
            y++;
        else if (dir < 7)
            x++;
        else if (dir < 9)
            x--;
        else
            y++;
        if (x < 0)
            x = 0;
        if (x >= w)
            x = w - 1;
    }
}
void wg_carve_rivers(RogueTileMap* map, const RogueWorldGenConfig* cfg)
{
    int attempts = cfg->river_attempts;
    if (attempts < 0)
        attempts = 0;
    if (attempts > 16)
        attempts = 16;
    for (int i = 0; i < attempts; i++)
        carve_river_single(map);
}

void wg_apply_erosion(RogueTileMap* map)
{
    int w = map->width, h = map->height;
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            unsigned char* t = &map->tiles[y * w + x];
            if (*t == ROGUE_TILE_MOUNTAIN)
            {
                int water = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char n = map->tiles[(y + oy) * w + (x + ox)];
                        if (n == ROGUE_TILE_WATER || n == ROGUE_TILE_RIVER)
                            water++;
                    }
                if (water >= 3 && rng_norm() < 0.4)
                    *t = ROGUE_TILE_GRASS;
            }
        }
}

void wg_smooth_small_islands(RogueTileMap* map, int max_island_size, int target_tile,
                             int replacement_bias)
{
    (void) replacement_bias;
    if (max_island_size <= 0)
        return;
    int w = map->width, h = map->height;
    size_t total = (size_t) w * (size_t) h;
    unsigned char* visited = (unsigned char*) malloc(total);
    if (!visited)
        return;
    memset(visited, 0, total);
    int stack_cap = max_island_size * 8;
    int* sx = (int*) malloc(sizeof(int) * (size_t) stack_cap);
    int* sy = (int*) malloc(sizeof(int) * (size_t) stack_cap);
    if (!sx || !sy)
    {
        free(visited);
        if (sx)
            free(sx);
        if (sy)
            free(sy);
        return;
    }
    int* compx = (int*) malloc(sizeof(int) * (size_t) max_island_size * 4);
    int* compy = (int*) malloc(sizeof(int) * (size_t) max_island_size * 4);
    if (!compx || !compy)
    {
        free(visited);
        free(sx);
        free(sy);
        if (compx)
            free(compx);
        if (compy)
            free(compy);
        return;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            size_t idx = (size_t) y * (size_t) w + (size_t) x;
            if (visited[idx])
                continue;
            unsigned char tile = map->tiles[idx];
            if (target_tile >= 0 && tile != (unsigned char) target_tile)
            {
                visited[idx] = 1;
                continue;
            }
            if (tile == ROGUE_TILE_RIVER)
            {
                visited[idx] = 1;
                continue;
            }
            int comp_count = 0;
            int sp = 0;
            sx[sp] = x;
            sy[sp] = y;
            sp++;
            visited[idx] = 1;
            int aborted = 0;
            while (sp > 0)
            {
                int cx = sx[--sp];
                int cy = sy[sp];
                if (comp_count < max_island_size * 4)
                {
                    compx[comp_count] = cx;
                    compy[comp_count] = cy;
                }
                comp_count++;
                if (comp_count > max_island_size)
                {
                    aborted = 1;
                    break;
                }
                static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int di = 0; di < 4; ++di)
                {
                    int nx = cx + dirs[di][0];
                    int ny = cy + dirs[di][1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    size_t nidx = (size_t) ny * (size_t) w + (size_t) nx;
                    if (visited[nidx])
                        continue;
                    if (map->tiles[nidx] == tile)
                    {
                        if (sp < stack_cap)
                        {
                            sx[sp] = nx;
                            sy[sp] = ny;
                            sp++;
                        }
                        visited[nidx] = 1;
                    }
                }
            }
            if (aborted)
                continue;
            if (comp_count <= max_island_size)
            {
                int counts[ROGUE_TILE_MAX];
                memset(counts, 0, sizeof counts);
                for (int i = 0; i < comp_count; i++)
                {
                    int cx = compx[i];
                    int cy = compy[i];
                    static const int ndirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (int di = 0; di < 4; ++di)
                    {
                        int nx = cx + ndirs[di][0];
                        int ny = cy + ndirs[di][1];
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                            continue;
                        unsigned char nt = map->tiles[(size_t) ny * (size_t) w + (size_t) nx];
                        if (nt != tile && nt < ROGUE_TILE_MAX)
                            counts[nt]++;
                    }
                }
                int best_type = -1;
                int best_count = 0;
                for (int t = 0; t < ROGUE_TILE_MAX; t++)
                    if (t != tile && counts[t] > best_count)
                    {
                        best_count = counts[t];
                        best_type = t;
                    }
                if (best_type >= 0 && best_count > 0)
                {
                    for (int i = 0; i < comp_count; i++)
                    {
                        int cx = compx[i];
                        int cy = compy[i];
                        map->tiles[(size_t) cy * (size_t) w + (size_t) cx] =
                            (unsigned char) best_type;
                    }
                }
            }
        }
    free(visited);
    free(sx);
    free(sy);
    free(compx);
    free(compy);
}

void wg_thicken_shores(RogueTileMap* map)
{
    int w = map->width, h = map->height;
    unsigned char* copy = (unsigned char*) malloc((size_t) w * h);
    if (!copy)
        return;
    memcpy(copy, map->tiles, (size_t) w * h);
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            size_t idx = (size_t) y * (size_t) w + (size_t) x;
            unsigned char t = copy[idx];
            if (t == ROGUE_TILE_GRASS || t == ROGUE_TILE_FOREST)
            {
                int water_adj = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char nt = copy[(size_t) (y + oy) * w + (size_t) (x + ox)];
                        if (nt == ROGUE_TILE_WATER || nt == ROGUE_TILE_RIVER)
                            water_adj++;
                    }
                if (water_adj >= 5)
                {
                    map->tiles[idx] = ROGUE_TILE_WATER;
                }
            }
        }
    free(copy);
}

void wg_advanced_post(RogueTileMap* out_map, const RogueWorldGenConfig* cfg)
{
    int w = out_map->width, h = out_map->height;
    double water_level = (cfg->water_level > 0.0 ? cfg->water_level : 0.32);
    int oct = cfg->noise_octaves > 0 ? cfg->noise_octaves : 5;
    double lac = cfg->noise_lacunarity > 0.0 ? cfg->noise_lacunarity : 2.0;
    double gain = cfg->noise_gain > 0.0 ? cfg->noise_gain : 0.5;
    double* elev = (double*) malloc((size_t) w * h * sizeof(double));
    if (!elev)
        return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            double nx = (double) x / (double) w - 0.5;
            double ny = (double) y / (double) h - 0.5;
            double dist = sqrt(nx * nx + ny * ny);
            double e = fbm((nx + 5.0) * 2.0, (ny + 7.0) * 2.0, oct, lac, gain);
            e -= dist * 0.35;
            elev[y * w + x] = e;
        }
    int sources = cfg->river_sources > 0 ? cfg->river_sources : 8;
    int max_len = cfg->river_max_length > 0 ? cfg->river_max_length : (h * 2);
    for (int s = 0; s < sources; s++)
    {
        int sx = rng_range(0, w - 1), sy = rng_range(0, h - 1);
        int attempts = 0;
        while (attempts < 200)
        {
            double e = elev[sy * w + sx];
            if (e > water_level + 0.25)
                break;
            sx = rng_range(0, w - 1);
            sy = rng_range(0, h - 1);
            attempts++;
        }
        int x = sx, y = sy;
        double prev_e = elev[y * w + x];
        for (int step = 0; step < max_len; ++step)
        {
            size_t idx = (size_t) y * w + x;
            out_map->tiles[idx] = (unsigned char) ROGUE_TILE_RIVER;
            if (step % 25 == 0)
            {
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        int nx = x + ox, ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                            continue;
                        if (rng_norm() < 0.4)
                            out_map->tiles[(size_t) ny * w + nx] =
                                (unsigned char) ROGUE_TILE_RIVER_WIDE;
                    }
            }
            int bestx = x, besty = y;
            double beste = prev_e;
            for (int oy = -1; oy <= 1; oy++)
                for (int ox = -1; ox <= 1; ox++)
                {
                    if (!ox && !oy)
                        continue;
                    int nx = x + ox, ny = y + oy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    double ne = elev[ny * w + nx];
                    if (ne < beste)
                    {
                        beste = ne;
                        bestx = nx;
                        besty = ny;
                    }
                }
            if (bestx == x && besty == y)
                break;
            x = bestx;
            y = besty;
            prev_e = beste;
            if (prev_e < water_level)
                break;
        }
    }
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            size_t idx = (size_t) y * w + x;
            unsigned char t = out_map->tiles[idx];
            if (t == ROGUE_TILE_RIVER || t == ROGUE_TILE_RIVER_WIDE)
            {
                int water_neighbors = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char nt =
                            out_map->tiles[(size_t) (y + oy) * w + (size_t) (x + ox)];
                        if (nt == ROGUE_TILE_WATER)
                            water_neighbors++;
                    }
                if (water_neighbors >= 4)
                {
                    out_map->tiles[idx] = (unsigned char) ROGUE_TILE_RIVER_DELTA;
                }
            }
        }
    double thresh = (cfg->cave_mountain_elev_thresh > 0.0 ? cfg->cave_mountain_elev_thresh
                                                          : (water_level + 0.28));
    unsigned char* cave = (unsigned char*) malloc((size_t) w * h);
    if (cave)
    {
        for (int i = 0; i < w * h; i++)
            cave[i] = 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                double e = elev[y * w + x];
                if (e > thresh)
                {
                    cave[y * w + x] = (rng_norm() < cfg->cave_fill_chance) ? 1 : 0;
                }
            }
        int iters = cfg->cave_iterations;
        for (int it = 0; it < iters; ++it)
        {
            for (int y = 1; y < h - 1; y++)
                for (int x = 1; x < w - 1; x++)
                {
                    if (!cave[y * w + x])
                        continue;
                    int count = 0;
                    for (int oy = -1; oy <= 1; oy++)
                        for (int ox = -1; ox <= 1; ox++)
                        {
                            if (!ox && !oy)
                                continue;
                            if (cave[(y + oy) * w + (x + ox)])
                                count++;
                        }
                    if (count < 4)
                        cave[y * w + x] = 0;
                }
        }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (cave[y * w + x])
                {
                    size_t idx = (size_t) y * w + x;
                    if (out_map->tiles[idx] == ROGUE_TILE_MOUNTAIN)
                        out_map->tiles[idx] =
                            (rng_norm() < 0.2) ? ROGUE_TILE_CAVE_FLOOR : ROGUE_TILE_CAVE_WALL;
                }
        free(cave);
    }
    free(elev);
}
