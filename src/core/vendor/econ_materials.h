#ifndef ROGUE_ECON_MATERIALS_H
#define ROGUE_ECON_MATERIALS_H

#include "core/loot/loot_item_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueMaterialEntry { int def_index; int base_value; } RogueMaterialEntry;

#ifndef ROGUE_ECON_MATERIAL_CAP
#define ROGUE_ECON_MATERIAL_CAP 128
#endif

int rogue_econ_material_catalog_build(void);
int rogue_econ_material_catalog_count(void);
const RogueMaterialEntry* rogue_econ_material_catalog_get(int idx);
int rogue_econ_material_base_value(int def_index);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ECON_MATERIALS_H */
