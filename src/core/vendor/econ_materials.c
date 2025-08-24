#include "econ_materials.h"
#include <string.h>

static RogueMaterialEntry g_catalog[ROGUE_ECON_MATERIAL_CAP];
static int g_catalog_count = 0;

static int is_currency_like(const RogueItemDef* d)
{
    if (!d)
        return 0;
    if (d->category == ROGUE_ITEM_MATERIAL)
        return 1;
    if (strstr(d->id, "stone") || strstr(d->id, "essence"))
        return 1;
    return 0;
}

int rogue_econ_material_catalog_build(void)
{
    g_catalog_count = 0;
    int total = rogue_item_defs_count();
    for (int i = 0; i < total && g_catalog_count < ROGUE_ECON_MATERIAL_CAP; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (!is_currency_like(d))
            continue;
        g_catalog[g_catalog_count].def_index = i;
        g_catalog[g_catalog_count].base_value = d->base_value > 0 ? d->base_value : 1;
        g_catalog_count++;
    }
    return g_catalog_count;
}
int rogue_econ_material_catalog_count(void) { return g_catalog_count; }
const RogueMaterialEntry* rogue_econ_material_catalog_get(int idx)
{
    if (idx < 0 || idx >= g_catalog_count)
        return NULL;
    return &g_catalog[idx];
}
int rogue_econ_material_base_value(int def_index)
{
    for (int i = 0; i < g_catalog_count; i++)
    {
        if (g_catalog[i].def_index == def_index)
            return g_catalog[i].base_value;
    }
    return -1;
}
