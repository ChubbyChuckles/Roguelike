#include "core/equipment/equipment_enhance.h"
#include "core/loot/loot_affixes.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/stat_cache.h"
#include "core/vendor/economy.h"
#include "core\inventory\inventory.h"
#include <stdlib.h>

/* Catalyst material names (resolve lazily). These are placeholders tying into existing item defs if
 * present. */
static int g_catalyst_imbue_prefix = -1; /* imbue_prefix_stone */
static int g_catalyst_imbue_suffix = -1; /* imbue_suffix_stone */
static int g_catalyst_temper = -1;       /* temper_core */
static int g_catalyst_socket = -1;       /* socket_chisel */

static void resolve_catalysts(void)
{
    if (g_catalyst_imbue_prefix < 0)
        g_catalyst_imbue_prefix = rogue_item_def_index("imbue_prefix_stone");
    if (g_catalyst_imbue_suffix < 0)
        g_catalyst_imbue_suffix = rogue_item_def_index("imbue_suffix_stone");
    if (g_catalyst_temper < 0)
        g_catalyst_temper = rogue_item_def_index("temper_core");
    if (g_catalyst_socket < 0)
        g_catalyst_socket = rogue_item_def_index("socket_chisel");
}

/* Lightweight deterministic PRNG (LCG). Seed derived from item guid and operation discriminator. */
static unsigned int op_seed(unsigned long long guid, unsigned int salt)
{
    unsigned int s = (unsigned int) (guid ^ (guid >> 32) ^ salt ^ 0xA5C3F1E5u);
    if (s == 0)
        s = 0x1234567u;
    return s;
}
static unsigned int lcg_next(unsigned int* s)
{
    *s = (*s * 1664525u) + 1013904223u;
    return *s;
}

int rogue_item_instance_imbue(int inst_index, int is_prefix, int* out_affix_index,
                              int* out_affix_value)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    int* slot_index = is_prefix ? &it->prefix_index : &it->suffix_index;
    int* slot_value = is_prefix ? &it->prefix_value : &it->suffix_value;
    if (*slot_index >= 0)
        return -2; /* occupied */
    int cap = rogue_budget_max(it->item_level, it->rarity);
    int cur = rogue_item_instance_total_affix_weight(inst_index);
    if (cur < 0)
        return -3;
    int remaining = cap - cur;
    if (remaining <= 0)
        return -3;
    resolve_catalysts();
    int catalyst = is_prefix ? g_catalyst_imbue_prefix : g_catalyst_imbue_suffix;
    if (catalyst >= 0 && rogue_inventory_get_count(catalyst) <= 0)
        return -5; /* require catalyst if defined */
    unsigned int seed = op_seed(it->guid, is_prefix ? 0x1111u : 0x2222u);
    /* Roll affix */
    int affix_index =
        rogue_affix_roll(is_prefix ? ROGUE_AFFIX_PREFIX : ROGUE_AFFIX_SUFFIX, it->rarity, &seed);
    if (affix_index < 0)
        return -4;
    int value = rogue_affix_roll_value(affix_index, &seed);
    if (value <= 0)
        value = 1;
    if (value > remaining)
    {
        value = remaining;
        if (value <= 0)
            return -6;
    }
    *slot_index = affix_index;
    *slot_value = value;
    if (catalyst >= 0)
        rogue_inventory_consume(catalyst, 1);
    if (out_affix_index)
        *out_affix_index = affix_index;
    if (out_affix_value)
        *out_affix_value = value;
    if (rogue_item_instance_validate_budget(inst_index) != 0)
    { /* extremely unlikely due to clamp */
        *slot_index = -1;
        *slot_value = 0;
        return -3;
    }
    rogue_stat_cache_mark_dirty();
    return 0;
}

int rogue_item_instance_temper(int inst_index, int is_prefix, int intensity, int* out_new_value,
                               int* out_failed)
{
    if (out_failed)
        *out_failed = 0;
    if (intensity <= 0)
        return -3;
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    int* slot_index = is_prefix ? &it->prefix_index : &it->suffix_index;
    int* slot_value = is_prefix ? &it->prefix_value : &it->suffix_value;
    if (*slot_index < 0)
        return -2; /* no affix */
    resolve_catalysts();
    if (g_catalyst_temper >= 0 && rogue_inventory_get_count(g_catalyst_temper) <= 0)
        return -5;
    int cap = rogue_budget_max(it->item_level, it->rarity);
    int cur_total = rogue_item_instance_total_affix_weight(inst_index);
    if (cur_total < 0)
        return -1;
    int remaining = cap - cur_total;
    if (remaining <= 0)
        return 1; /* no-op already at cap */
    unsigned int seed = op_seed(it->guid, is_prefix ? 0x3333u : 0x4444u);
    /* success probability fixed 80% - can evolve later with skill/perks */
    unsigned int roll = lcg_next(&seed) % 100u;
    int success = roll < 80u;
    if (!success)
    {
        if (g_catalyst_temper >= 0)
            rogue_inventory_consume(g_catalyst_temper, 1);
        /* fracture risk via durability damage */
        int dmg = 5 + intensity;
        rogue_item_instance_damage_durability(inst_index, dmg);
        if (out_failed)
            *out_failed = 1;
        if (out_new_value)
            *out_new_value = *slot_value;
        return 2; /* failure */
    }
    int delta = intensity;
    if (delta > remaining)
        delta = remaining;
    if (delta <= 0)
        return 1;
    *slot_value += delta;
    if (g_catalyst_temper >= 0)
        rogue_inventory_consume(g_catalyst_temper, 1);
    if (rogue_item_instance_validate_budget(inst_index) != 0)
    {
        *slot_value -= delta;
        return -3;
    }
    rogue_stat_cache_mark_dirty();
    if (out_new_value)
        *out_new_value = *slot_value;
    return 0;
}

int rogue_item_instance_add_socket(int inst_index, int* out_new_count)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    if (!d)
        return -1;
    int max = d->socket_max;
    int min = d->socket_min;
    (void) min;
    if (max <= 0)
        return -2;
    if (it->socket_count >= max)
        return 1; /* already max */
    resolve_catalysts();
    if (g_catalyst_socket >= 0 && rogue_inventory_get_count(g_catalyst_socket) <= 0)
        return -3;
    it->socket_count += 1;
    if (it->socket_count > max)
        it->socket_count = max; /* cap */
    if (g_catalyst_socket >= 0)
        rogue_inventory_consume(g_catalyst_socket, 1);
    if (out_new_count)
        *out_new_count = it->socket_count;
    return 0;
}

int rogue_item_instance_reroll_sockets(int inst_index, int* out_new_count)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    if (!d)
        return -1;
    int min = d->socket_min, max = d->socket_max;
    if (max <= 0 || max < min)
        return -2;
    if (max > 6)
        max = 6;
    if (min < 0)
        min = 0;
    resolve_catalysts();
    if (g_catalyst_socket >= 0 && rogue_inventory_get_count(g_catalyst_socket) <= 0)
        return -3;
    unsigned int seed = op_seed(it->guid, 0x5555u);
    int span = (max - min) + 1;
    int roll = span > 0 ? (int) (lcg_next(&seed) % (unsigned int) span) : 0;
    int new_count = min + roll;
    if (new_count > 6)
        new_count = 6;
    it->socket_count = new_count;
    for (int s = 0; s < new_count && s < 6; ++s)
        it->sockets[s] = -1;
    if (g_catalyst_socket >= 0)
        rogue_inventory_consume(g_catalyst_socket, 1);
    if (out_new_count)
        *out_new_count = new_count;
    return 0;
}
