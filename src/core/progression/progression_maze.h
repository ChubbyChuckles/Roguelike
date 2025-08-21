/* Progression Maze Skill Graph Framework (Phase 4)
 * Wraps the lower-level skill_maze generator with progression metadata:
 * - Node gating predicates (level & attribute thresholds derived from ring)
 * - Traversal costs (allocation point cost per node)
 * - Procedural optional branch augmentation & difficulty tagging
 * - Adjacency lists & shortest path utilities (Dijkstra over small graph)
 */
#ifndef ROGUE_PROGRESSION_MAZE_H
#define ROGUE_PROGRESSION_MAZE_H
#include "core/skill_maze.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueProgressionMazeNodeMeta {
    int node_id;           /* 0..node_count-1 */
    int ring;              /* copy of generation ring */
    int level_req;         /* derived: ring*5 baseline */
    int str_req, dex_req, int_req, vit_req; /* derived attribute thresholds */
    int cost_points;       /* point cost to unlock (sublinear ramp by ring) */
    unsigned int tags;     /* future classification (e.g., offensive/defensive/utility) */
    unsigned int flags;    /* bit0: optional_branch, bit1: difficulty_tag_high, bit2: keystone */
    int adj_start;         /* offset into adjacency index array */
    int adj_count;         /* number of neighbors */
} RogueProgressionMazeNodeMeta;

typedef struct RogueProgressionMaze {
    RogueSkillMaze base;              /* underlying geometric graph */
    RogueProgressionMazeNodeMeta* meta; /* meta per node */
    int* adjacency;                   /* flattened adjacency indices */
    int total_adjacency;              /* length of adjacency array */
    int optional_nodes;               /* count of nodes flagged optional */
} RogueProgressionMaze;

/* Build progression maze from config JSON; performs procedural augmentation (optional branches). Returns 1 on success. */
int rogue_progression_maze_build(const char* config_path, RogueProgressionMaze* out_maze);
void rogue_progression_maze_free(RogueProgressionMaze* m);

/* Query gating: returns 1 if unlockable given player level & attributes, else 0. */
int rogue_progression_maze_node_unlockable(const RogueProgressionMaze* m, int node_id, int level,int str,int dex,int intel,int vit);

/* Compute shortest point cost between two nodes (cost_points as edge weight of destination node). Returns -1 if unreachable. */
int rogue_progression_maze_shortest_cost(const RogueProgressionMaze* m, int from_node, int to_node);

/* Simple orphan audit: returns number of non-root nodes (node_id>0) with adj_count==0 and not optional. */
int rogue_progression_maze_orphan_count(const RogueProgressionMaze* m);

/* Phase 7 additions */
/* Flag helpers */
#define ROGUE_MAZE_FLAG_OPTIONAL   0x1u
#define ROGUE_MAZE_FLAG_DIFFICULTY 0x2u
#define ROGUE_MAZE_FLAG_KEYSTONE   0x4u

/* Returns 1 if node flagged a keystone else 0. */
int rogue_progression_maze_is_keystone(const RogueProgressionMaze* m, int node_id);
/* Count keystone nodes in the maze. */
int rogue_progression_maze_keystone_total(const RogueProgressionMaze* m);

/* Ring expansion milestones (conceptual extra outer rings unlocked at milestone levels). Returns additional ring layers available beyond the base maze->base.rings. */
int rogue_progression_ring_expansions_unlocked(int player_level);

/* Dynamically expand the maze by appending `extra_rings` outer ring layers (Phase 7.1 full implementation).
 * Returns number of new rings actually added (0 if none). New nodes are procedurally placed in a circular band.
 */
int rogue_progression_maze_expand(RogueProgressionMaze* m, int extra_rings, unsigned int seed);

/* Return total rings including expansions (wrapper for m->base.rings). */
int rogue_progression_maze_total_rings(const RogueProgressionMaze* m);

/* Phase 7.4 Visualization Layering */
/* Populate out_layers array (capacity >= m->base.rings) with ring radii (average distance from origin) and returns ring count; returns 0 on failure. */
int rogue_progression_maze_layers(const RogueProgressionMaze* m, float* out_layers, int max_layers);
/* Project node index to polar coordinates (radius,theta) relative to center (computed centroid). Returns 1 on success. */
int rogue_progression_maze_project(const RogueProgressionMaze* m, int node_id, float* out_radius, float* out_theta);
/* Generate a coarse ASCII overview (concentric approximation). Writes up to `buf_size-1` chars incl NUL. Returns chars written (excluding NUL) or -1. */
int rogue_progression_maze_ascii_overview(const RogueProgressionMaze* m, char* buffer, int buf_size, int cols, int rows);

#ifdef __cplusplus
}
#endif
#endif
