#include "core/loot/loot_tooltip.h"
#include "core/equipment/equipment.h"
#include "core/loot/loot_affixes.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void append(char** w, char* end, const char* fmt, ...)
{
    if (!w || !*w)
        return;
    va_list ap;
    va_start(ap, fmt);
    int rem = (int) (end - *w);
    if (rem <= 0)
    {
        va_end(ap);
        return;
    }
    int n = vsnprintf(*w, rem, fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        if (n >= rem)
        {
            *w = end;
            return;
        }
        *w += n;
    }
}

char* rogue_item_tooltip_build(int inst_index, char* buf, size_t buf_sz)
{
    if (!buf || buf_sz < 4)
        return NULL;
    buf[0] = '\0';
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
    {
        snprintf(buf, buf_sz, "<invalid>");
        return buf;
    }
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    if (!d)
    {
        snprintf(buf, buf_sz, "<missing def>");
        return buf;
    }
    char* w = buf;
    char* end = buf + buf_sz;
    append(&w, end, "%s (x%d)\n", d->name, it->quantity);
    if (d->base_damage_max > 0)
        append(&w, end, "Damage: %d-%d\n", rogue_item_instance_damage_min(inst_index),
               rogue_item_instance_damage_max(inst_index));
    if (d->base_armor > 0)
        append(&w, end, "Armor: %d\n", d->base_armor);
    if (it->prefix_index >= 0)
    {
        const RogueAffixDef* a = rogue_affix_at(it->prefix_index);
        if (a)
        {
            append(&w, end, "%s +%d\n", a->id, it->prefix_value);
        }
    }
    if (it->suffix_index >= 0)
    {
        const RogueAffixDef* a = rogue_affix_at(it->suffix_index);
        if (a)
        {
            append(&w, end, "%s +%d\n", a->id, it->suffix_value);
        }
    }
    int cur, max;
    if (rogue_item_instance_get_durability(inst_index, &cur, &max) == 0 && max > 0)
    {
        append(&w, end, "Durability: %d/%d\n", cur, max);
    }
    return buf;
}

char* rogue_item_tooltip_build_compare(int inst_index, int compare_slot, char* buf, size_t buf_sz)
{
    if (!buf)
        return NULL;
    char base[512];
    rogue_item_tooltip_build(inst_index, base, sizeof base);
    snprintf(buf, buf_sz, "%s", base);
    if (compare_slot < 0)
        return buf;
    int equipped = rogue_equip_get((enum RogueEquipSlot) compare_slot);
    if (equipped < 0)
        return buf;
    const RogueItemInstance* cand = rogue_item_instance_at(inst_index);
    const RogueItemInstance* cur = rogue_item_instance_at(equipped);
    if (!cand || !cur)
        return buf;
    int cand_min = rogue_item_instance_damage_min(inst_index);
    int cand_max = rogue_item_instance_damage_max(inst_index);
    int cur_min = rogue_item_instance_damage_min(equipped);
    int cur_max = rogue_item_instance_damage_max(equipped);
    int dmin = cand_min - cur_min;
    int dmax = cand_max - cur_max;
    size_t len = strlen(buf);
    if (len + 64 < buf_sz)
    {
        snprintf(buf + len, buf_sz - len, "Compared to equipped: %+d-%+d dmg\n", dmin, dmax);
    }
    return buf;
}
