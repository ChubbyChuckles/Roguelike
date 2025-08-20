/* Crafting & Gathering Phase 0–1 Material Registry
 * Provides unified material taxonomy independent of generic item defs.
 * Data File Format (materials/materials.cfg):
 *   id,item_def_id,tier,category,base_value
 *   # category one of: ore,plant,essence,component,currency
 *
 * Responsibilities:
 *  - Enumerate & classify materials (Phase 0.1 / 0.3)
 *  - Map salvage & crafting currency items to registry (0.2/0.4)
 *  - Expose lookups by id, item def, prefix (1.3)
 *  - Deterministic ordering based on file order (1.5)
 */
#ifndef ROGUE_MATERIAL_REGISTRY_H
#define ROGUE_MATERIAL_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ROGUE_MATERIAL_REGISTRY_CAP
#define ROGUE_MATERIAL_REGISTRY_CAP 128
#endif

enum RogueMaterialCategory { ROGUE_MAT_ORE=0, ROGUE_MAT_PLANT, ROGUE_MAT_ESSENCE, ROGUE_MAT_COMPONENT, ROGUE_MAT_CURRENCY, ROGUE_MAT__COUNT };

typedef struct RogueMaterialDef {
    char id[32];
    int item_def_index; /* link to item defs */
    int tier; /* arbitrary 0+ small int */
    int category; /* RogueMaterialCategory */
    int base_value; /* override or fallback to item base value if <=0 */
} RogueMaterialDef;

/* Load default registry (assets/materials/materials.cfg). Returns number loaded or <0 on error. */
int rogue_material_registry_load_default(void);
/* Load from explicit path. */
int rogue_material_registry_load_path(const char* path);
/* Reset registry (clears entries). */
void rogue_material_registry_reset(void);

int rogue_material_count(void);
const RogueMaterialDef* rogue_material_get(int idx);
const RogueMaterialDef* rogue_material_find(const char* id);
const RogueMaterialDef* rogue_material_find_by_item(int item_def_index);
/* Prefix search: returns count of matches written to out_indices (up to max). */
int rogue_material_prefix_search(const char* prefix, int* out_indices, int max);

/* Deterministic seed mixing for material node generation (Phase 1.4 baseline). */
unsigned int rogue_material_seed_mix(unsigned int world_seed, int material_index);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_MATERIAL_REGISTRY_H */
