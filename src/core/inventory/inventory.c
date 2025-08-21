#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_counts[ROGUE_ITEM_DEF_CAP];
static int g_distinct = 0;

void rogue_inventory_init(void)
{
    memset(g_counts, 0, sizeof g_counts);
    g_distinct = 0;
}
void rogue_inventory_reset(void) { rogue_inventory_init(); }

int rogue_inventory_add(int def_index, int quantity)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP || quantity <= 0)
        return 0;
    if (g_counts[def_index] == 0)
        g_distinct++;
    long long before = g_counts[def_index];
    long long after = before + quantity;
    if (after > 2147483647)
        after = 2147483647;
    g_counts[def_index] = (int) after;
    return (int) (after - before);
}

int rogue_inventory_get_count(int def_index)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP)
        return 0;
    return g_counts[def_index];
}
int rogue_inventory_total_distinct(void) { return g_distinct; }

int rogue_inventory_consume(int def_index, int quantity)
{
    if (def_index < 0 || def_index >= ROGUE_ITEM_DEF_CAP || quantity <= 0)
        return 0;
    int have = g_counts[def_index];
    if (have <= 0)
        return 0;
    int remove = quantity;
    if (remove > have)
        remove = have;
    g_counts[def_index] = have - remove;
    if (g_counts[def_index] == 0)
        g_distinct--;
    return remove;
}

void rogue_inventory_serialize(FILE* f)
{
    if (!f)
        return;
    for (int i = 0; i < ROGUE_ITEM_DEF_CAP; i++)
    {
        if (g_counts[i] > 0)
        {
            fprintf(f, "INV%d=%d\n", i, g_counts[i]);
        }
    }
}
int rogue_inventory_try_parse_kv(const char* key, const char* val)
{
    if (strncmp(key, "INV", 3) != 0)
        return 0;
    int idx = atoi(key + 3);
    if (idx < 0 || idx >= ROGUE_ITEM_DEF_CAP)
        return 0;
    int q = atoi(val);
    if (q < 0)
        q = 0;
    if (g_counts[idx] == 0 && q > 0)
        g_distinct++;
    g_counts[idx] = q;
    return 1;
}
