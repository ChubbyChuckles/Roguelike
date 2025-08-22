#ifndef ROGUE_CORE_NAVIGATION_H
#define ROGUE_CORE_NAVIGATION_H
/* Lightweight navigation & cost helpers (cardinal only) */
int rogue_nav_is_blocked(int tx, int ty);  /* 1 if impassable (terrain or tree) */
float rogue_nav_tile_cost(int tx, int ty); /* >=1 cost (plants > 1) */
void rogue_nav_cardinal_step_towards(
    float sx, float sy, float tx, float ty, int* out_dx,
    int* out_dy); /* picks best axis step (-1,0,1), never diagonal */

/* A* pathfinding (cardinal moves) */
#define ROGUE_PATH_MAX_POINTS 512
typedef struct RoguePath
{
    int length;              /* number of points filled */
    unsigned char failed;    /* 1 if no path */
    unsigned char truncated; /* 1 if path longer than capacity */
    int xs[ROGUE_PATH_MAX_POINTS];
    int ys[ROGUE_PATH_MAX_POINTS];
} RoguePath;

/* Compute path from (sx,sy) to (tx,ty). Returns 1 on success (even if truncated), 0 on immediate
 * failure (blocked / no path). */
int rogue_nav_astar(int sx, int sy, int tx, int ty, RoguePath* out_path);

/* Path smoothing/simplification (cardinal):
 * Compresses consecutive collinear segments in a cardinal-only path, preserving start/end, and
 * keeping steps cardinal (no diagonals introduced). Returns number of points in out, or 0 if
 * input invalid. out_path may alias in_path. */
int rogue_nav_path_simplify(const RoguePath* in_path, RoguePath* out_path);

#endif
