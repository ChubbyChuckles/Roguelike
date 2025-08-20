/* Crafting & Gathering Phase 2 (2.1-2.6)
 * Gathering Node System: deterministic spawn, depletion, respawn, tool gating, rare variant.
 * Node definition config format (gathering/nodes.cfg):
 * id,material_table, min_roll,max_roll,respawn_ms,tool_req_tier,biome_tags,spawn_chance_pct,rare_proc_chance_pct,rare_bonus_multiplier
 * material_table: material_id:weight;material_id:weight;...
 */
#ifndef ROGUE_GATHERING_H
#define ROGUE_GATHERING_H
#ifdef __cplusplus
extern "C" { 
#endif

#ifndef ROGUE_GATHER_NODE_CAP
#define ROGUE_GATHER_NODE_CAP 256
#endif

typedef struct RogueGatherNodeDef {
    char id[32];
    int material_defs[8];
    int material_weights[8];
    int material_count;
    int min_roll;
    int max_roll;
    float respawn_ms;
    int tool_req_tier;
    char biome_tags[32];
    int spawn_chance_pct; /* 0-100 */
    int rare_proc_chance_pct; /* 0-100 */
    float rare_bonus_multiplier; /* >1 increases yield */
} RogueGatherNodeDef;

typedef struct RogueGatherNodeInstance {
    int def_index; /* index into defs */
    int chunk_id;
    int state; /* 0=active 1=depleted */
    float respawn_timer_ms;
    int rare_last; /* flag: last harvest used rare variant */
} RogueGatherNodeInstance;

int rogue_gather_defs_load_default(void);
int rogue_gather_defs_load_path(const char* path);
void rogue_gather_defs_reset(void);
int rogue_gather_def_count(void);
const RogueGatherNodeDef* rogue_gather_def_at(int idx);

/* Spawns candidate nodes for chunk deterministically. Returns count added. */
int rogue_gather_spawn_chunk(unsigned int world_seed, int chunk_id);
int rogue_gather_node_count(void);
const RogueGatherNodeInstance* rogue_gather_node_at(int idx);

/* Harvest node: returns 0 success, <0 error (-1 invalid, -2 depleted, -3 tool tier insufficient). */
int rogue_gather_harvest(int node_index, unsigned int* rng_state, int* out_material_def, int* out_qty);

/* Advance simulation (respawn timers). */
void rogue_gather_update(float dt_ms);

/* Tool tier API for gating (simple global for single player). */
void rogue_gather_set_player_tool_tier(int tier);
int  rogue_gather_get_player_tool_tier(void);

/* Analytics */
unsigned long long rogue_gather_total_harvests(void);
unsigned long long rogue_gather_total_rare_procs(void);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_GATHERING_H */
