#ifndef ROGUE_AI_SCHEDULER_H
#define ROGUE_AI_SCHEDULER_H
/* Phase 9.2/9.3: Incremental evaluation + LOD behaviour */
#include <stddef.h>
struct RogueEnemy;

/* Configure number of buckets to spread heavy AI ticks across frames (>=1). */
void rogue_ai_scheduler_set_buckets(int buckets);
int  rogue_ai_scheduler_get_buckets(void);

/* Configure Level of Detail (LOD) radius (tiles). Enemies farther than this distance from player
   run only a cheap maintenance tick (no BT execution) every frame. */
void rogue_ai_lod_set_radius(float radius);
float rogue_ai_lod_get_radius(void);

/* Advance scheduler one frame: distributes behavior tree ticks across buckets and applies LOD gating.
   dt_seconds passed to BT ticks for active agents. */
void rogue_ai_scheduler_tick(struct RogueEnemy* enemies, int count, float dt_seconds);

/* Frame counter accessor (monotonic). */
unsigned int rogue_ai_scheduler_frame(void);

/* Testing helper: reset internal state. */
void rogue_ai_scheduler_reset_for_tests(void);

#endif
