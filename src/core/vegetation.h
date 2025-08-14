#ifndef ROGUE_VEGETATION_H
#define ROGUE_VEGETATION_H

#include "app_state.h"

#ifdef __cplusplus
extern "C" { 
#endif

/* Lightweight runtime representation of a vegetation instance (tree or plant). */
typedef struct RogueVegetationInstance {
    float x, y;          /* tile coordinates (center) */
    unsigned short def_index; /* index into definition arrays (tree or plant) */
    unsigned char is_tree;     /* 1 tree, 0 plant */
    unsigned char variant;     /* for future animation/seasonal variants */
    unsigned char growth;      /* future growth stage */
} RogueVegetationInstance;

typedef struct RogueVegetationDef {
    char id[32];
    char image[128];
    unsigned short tile_x, tile_y;   /* top-left sprite (column, row) */
    unsigned short tile_x2, tile_y2; /* bottom-right sprite (inclusive) */
    unsigned short rarity;          /* relative weight */
    unsigned char canopy_radius;    /* tree footprint radius (tiles) */
    unsigned char is_tree;          /* 1 if tree, 0 plant */
} RogueVegetationDef;

/* Global vegetation registry + instances (owned by module) */
void rogue_vegetation_init(void);
void rogue_vegetation_clear_instances(void);
void rogue_vegetation_shutdown(void);

/* Load definitions from plants.cfg / trees.cfg
     Extended formats allow multi-sprite rectangles:
         PLANT,id,image,tx1,ty1,tx2,ty2,rarity
         TREE,id,image,tx1,ty1,tx2,ty2,rarity,canopy_radius
     Legacy single-sprite still supported:
         PLANT,id,image,tx,ty,rarity
         TREE,id,image,tx,ty,rarity,canopy_radius
*/
int rogue_vegetation_load_defs(const char* plants_cfg, const char* trees_cfg);

/* Generate static vegetation placement over existing world map grass tiles. */
void rogue_vegetation_generate(float tree_cover_target, unsigned int seed);

/* Adjust coverage at runtime (regenerates keeping seed stable if same). */
void rogue_vegetation_set_tree_cover(float cover_pct);
float rogue_vegetation_get_tree_cover(void);

/* Render (after world tiles, before entities). */
void rogue_vegetation_render(void);

/* Collision / movement cost queries */
int rogue_vegetation_tile_blocking(int tx,int ty); /* 1 if blocking (tree) */
float rogue_vegetation_tile_move_scale(int tx,int ty); /* multiplier (<1 slows) */
/* Fine-grained collision for entities using trunk-only logic with directional allowances.
    Returns 1 if movement ending at (nx,ny) from (ox,oy) should be blocked by any tree trunk. */
int rogue_vegetation_entity_blocking(float ox,float oy,float nx,float ny);
/* Enable/disable canopy tile blocking globally (affects tile-based collision). */
void rogue_vegetation_set_canopy_tile_blocking_enabled(int enabled);
int  rogue_vegetation_get_canopy_tile_blocking_enabled(void);

/* Debug/testing helpers: obtain first tree integer tile center and its canopy radius; returns 1 if found */
int rogue_vegetation_first_tree(int* out_tx,int* out_ty,int* out_radius);
/* Testing/introspection: get info about tree by index (0..tree_count-1). Returns 1 on success. */
int rogue_vegetation_tree_info(int index, float* out_x, float* out_y, int* out_tiles_w, int* out_tiles_h);

/* Enable/disable trunk entity collision globally (player + other entities). Enabled by default. */
void rogue_vegetation_set_trunk_collision_enabled(int enabled);
int  rogue_vegetation_get_trunk_collision_enabled(void);

/* Testing helpers */
int rogue_vegetation_count(void);
int rogue_vegetation_tree_count(void);
int rogue_vegetation_plant_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VEGETATION_H */
