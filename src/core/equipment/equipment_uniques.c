/* Equipment Phase 4.2: Unique item registry implementation */
#include "equipment_uniques.h"
#include "../loot/loot_item_defs.h"
#include <string.h>

static RogueUniqueDef g_uniques[ROGUE_UNIQUE_CAP];
static int g_unique_count = 0;

int rogue_unique_register(const RogueUniqueDef* def)
{
    if (!def || !def->id[0] || !def->base_item_id[0])
        return -1;
    if (g_unique_count >= ROGUE_UNIQUE_CAP)
        return -2; /* ensure base item exists */
    if (rogue_item_def_index(def->base_item_id) < 0)
        return -3; /* prevent duplicate id */
    for (int i = 0; i < g_unique_count; i++)
    {
        if (strcmp(g_uniques[i].id, def->id) == 0)
            return -4;
    }
    g_uniques[g_unique_count] = *def;
    return g_unique_count++;
}
int rogue_unique_count(void) { return g_unique_count; }
const RogueUniqueDef* rogue_unique_at(int index)
{
    if (index < 0 || index >= g_unique_count)
        return NULL;
    return &g_uniques[index];
}
int rogue_unique_find_by_base_def(int def_index)
{
    const RogueItemDef* d = rogue_item_def_at(def_index);
    if (!d)
        return -1;
    for (int i = 0; i < g_unique_count; i++)
    {
        if (strcmp(g_uniques[i].base_item_id, d->id) == 0)
            return i;
    }
    return -1;
}
