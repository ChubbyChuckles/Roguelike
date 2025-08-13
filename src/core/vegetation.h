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

/* Testing helpers */
int rogue_vegetation_count(void);
int rogue_vegetation_tree_count(void);
int rogue_vegetation_plant_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VEGETATION_H */
