#ifndef ROGUE_WORLD_GEN_DUNGEON_KERNEL_H
#define ROGUE_WORLD_GEN_DUNGEON_KERNEL_H

#include "world_gen.h"

/* Utility helpers for the dungeon layout kernel and tests.
 * These do not change generation behavior; they analyze graphs produced by
 * rogue_dungeon_generate_graph for invariants and planning (Phase 1 helpers).
 */

/* Longest shortest-path length across all rooms (graph diameter in edges).
 * Returns -1 if g is NULL or has no rooms.
 */
int rogue_dungeon_graph_critical_path_length(const RogueDungeonGraph* g);

/* Degree statistics over the undirected graph edges.
 * out_max_degree: maximum node degree observed (0 if no edges)
 * out_avg_degree: average degree (2*E / N) reported as double. If N==0, 0.0.
 */
void rogue_dungeon_graph_degree_stats(const RogueDungeonGraph* g, int* out_max_degree,
                                      double* out_avg_degree);

/* Deterministic hash of a dungeon graph based on normalized sorted edge list and room ids. */
unsigned long long rogue_dungeon_graph_hash(const RogueDungeonGraph* g);

#endif /* ROGUE_WORLD_GEN_DUNGEON_KERNEL_H */
