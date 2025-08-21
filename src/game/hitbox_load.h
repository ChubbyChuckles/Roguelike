#ifndef ROGUE_GAME_HITBOX_LOAD_H
#define ROGUE_GAME_HITBOX_LOAD_H
#include "game/hitbox.h"

/* Phase 5.2: Hitbox authoring (lightweight JSON subset parser)
   Supports an array of objects each specifying one hitbox primitive. Grammar (subset):
   [ { "type":"capsule", "ax":0, "ay":0, "bx":1, "by":0, "r":0.5 }, ... ]
   Types & fields:
     capsule: ax,ay,bx,by,r
     arc: ox,oy,radius,a0,a1 (optional inner_radius)
     chain: radius, points:[ [x,y], [x,y], ... ] (>=1, <=8)
     projectile_spawn: count, ox,oy,speed,spread,center
   Whitespace ignored. Numbers use strtod. Unknown keys ignored for forward compatibility.
*/

int rogue_hitbox_load_sequence_from_memory(const char* json, RogueHitbox* out, int max_out,
                                           int* out_count);
int rogue_hitbox_load_sequence_file(const char* path, RogueHitbox* out, int max_out,
                                    int* out_count);
/* Phase M3.6: Load and concatenate all hitbox sequence files from a directory (non-recursive).
   Accepts files ending in .hitbox or .json. Returns 1 on success, 0 on failure. Stops early if
   capacity reached. */
int rogue_hitbox_load_directory(const char* dir, RogueHitbox* out, int max_out, int* out_count);

/* Phase 5.3: Broadphase + narrow-phase collection helper.
   Given hitbox h and arrays of point positions (x[i],y[i]) with alive flag (non-zero), collect
   indices whose point lies inside h. Returns count (capped by max_out). Performs fast coarse AABB
   prune before precise test.
*/
int rogue_hitbox_collect_point_overlaps(const RogueHitbox* h, const float* xs, const float* ys,
                                        const int* alive, int count, int* out_indices, int max_out);

#endif
