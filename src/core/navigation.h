#ifndef ROGUE_CORE_NAVIGATION_H
#define ROGUE_CORE_NAVIGATION_H
/* Lightweight navigation & cost helpers (cardinal only) */
int rogue_nav_is_blocked(int tx,int ty); /* 1 if impassable (terrain or tree) */
float rogue_nav_tile_cost(int tx,int ty); /* >=1 cost (plants > 1) */
void rogue_nav_cardinal_step_towards(float sx,float sy,float tx,float ty,int* out_dx,int* out_dy); /* picks best axis step (-1,0,1), never diagonal */
#endif
