#include "equipment_ui.h"
#include "../../entities/player.h"
#include "../../game/stat_cache.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "../loot/loot_tooltip.h" /* reuse simple builder for base lines */
#include "equipment_procs.h"
#include "equipment_stats.h" /* for runeword/set already aggregated; we only read cache */
#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_socket_sel_inst = -1, g_socket_sel_index = -1; /* ephemeral drag selection */
static int g_transmog_last[ROGUE_EQUIP__COUNT];
static int g_initialized = 0;

static void ensure_init(void)
{
    if (g_initialized)
        return;
    for (int i = 0; i < ROGUE_EQUIP__COUNT; i++)
        g_transmog_last[i] = -1;
    g_initialized = 1;
}

static unsigned int fnv1a(const char* s)
{
    unsigned int h = 2166136261u;
    while (s && *s)
    {
        h ^= (unsigned char) *s++;
        h *= 16777619u;
    }
    return h;
}

char* rogue_equipment_panel_build(char* buf, size_t cap)
{
    if (!buf || cap < 8)
        return NULL;
    ensure_init();
    size_t off = 0;
    int n = snprintf(buf + off, cap - off, "[Weapons]\n");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    int w = rogue_equip_get(ROGUE_EQUIP_WEAPON);
    int oh = rogue_equip_get(ROGUE_EQUIP_OFFHAND);
    const RogueItemInstance* wit = rogue_item_instance_at(w);
    const RogueItemDef* wd = wit ? rogue_item_def_at(wit->def_index) : NULL;
    const RogueItemInstance* ohit = rogue_item_instance_at(oh);
    const RogueItemDef* ohd = ohit ? rogue_item_def_at(ohit->def_index) : NULL;
    n = snprintf(buf + off, cap - off, "Weapon: %s\n", wd ? wd->name : "<empty>");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    n = snprintf(buf + off, cap - off, "Offhand: %s\n\n", ohd ? ohd->name : "<empty>");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    n = snprintf(buf + off, cap - off, "[Armor]\n");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    enum RogueEquipSlot armor_slots[] = {ROGUE_EQUIP_ARMOR_HEAD, ROGUE_EQUIP_ARMOR_CHEST,
                                         ROGUE_EQUIP_ARMOR_LEGS, ROGUE_EQUIP_ARMOR_HANDS,
                                         ROGUE_EQUIP_ARMOR_FEET, ROGUE_EQUIP_CLOAK};
    for (int i = 0; i < 6; i++)
    {
        enum RogueEquipSlot s = armor_slots[i];
        int inst = rogue_equip_get(s);
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        const RogueItemDef* d = it ? rogue_item_def_at(it->def_index) : NULL;
        const char* label = "";
        switch (s)
        {
        case ROGUE_EQUIP_ARMOR_HEAD:
            label = "Head";
            break;
        case ROGUE_EQUIP_ARMOR_CHEST:
            label = "Chest";
            break;
        case ROGUE_EQUIP_ARMOR_LEGS:
            label = "Legs";
            break;
        case ROGUE_EQUIP_ARMOR_HANDS:
            label = "Hands";
            break;
        case ROGUE_EQUIP_ARMOR_FEET:
            label = "Feet";
            break;
        case ROGUE_EQUIP_CLOAK:
            label = "Cloak";
            break;
        default:
            label = "Slot";
        }
        n = snprintf(buf + off, cap - off, "%s: %s\n", label, d ? d->name : "<empty>");
        if (n < 0 || n >= (int) (cap - off))
            return buf;
        off += n;
    }
    if (off + 2 < cap)
    {
        buf[off++] = '\n';
        buf[off] = '\0';
    }
    n = snprintf(buf + off, cap - off, "[Jewelry]\n");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    enum RogueEquipSlot jew[] = {ROGUE_EQUIP_RING1, ROGUE_EQUIP_RING2, ROGUE_EQUIP_AMULET,
                                 ROGUE_EQUIP_BELT};
    const char* jnames[] = {"Ring1", "Ring2", "Amulet", "Belt"};
    for (int i = 0; i < 4; i++)
    {
        int inst = rogue_equip_get(jew[i]);
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        const RogueItemDef* d = it ? rogue_item_def_at(it->def_index) : NULL;
        n = snprintf(buf + off, cap - off, "%s: %s\n", jnames[i], d ? d->name : "<empty>");
        if (n < 0 || n >= (int) (cap - off))
            return buf;
        off += n;
    }
    n = snprintf(buf + off, cap - off, "\n[Charms]\n");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    enum RogueEquipSlot charms[] = {ROGUE_EQUIP_CHARM1, ROGUE_EQUIP_CHARM2};
    for (int i = 0; i < 2; i++)
    {
        int inst = rogue_equip_get(charms[i]);
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        const RogueItemDef* d = it ? rogue_item_def_at(it->def_index) : NULL;
        n = snprintf(buf + off, cap - off, "Charm%d: %s\n", i + 1, d ? d->name : "<empty>");
        if (n < 0 || n >= (int) (cap - off))
            return buf;
        off += n;
    }
    /* Set progress (simple count by set_id) */
    int set_ids[16];
    int set_counts[16];
    int set_unique = 0;
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        if (inst < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d || d->set_id <= 0)
            continue;
        int found = -1;
        for (int k = 0; k < set_unique; k++)
            if (set_ids[k] == d->set_id)
            {
                found = k;
                break;
            }
        if (found < 0 && set_unique < 16)
        {
            set_ids[set_unique] = d->set_id;
            set_counts[set_unique] = 1;
            set_unique++;
        }
        else if (found >= 0)
            set_counts[found]++;
    }
    if (off + 16 < cap)
    {
        int n2 = snprintf(buf + off, cap - off, "\nSet Progress: ");
        if (n2 > 0 && n2 < (int) (cap - off))
            off += n2;
    }
    for (int i = 0; i < set_unique; i++)
    {
        int n2 = snprintf(buf + off, cap - off, "set_%d=%d ", set_ids[i], set_counts[i]);
        if (n2 < 0 || n2 >= (int) (cap - off))
            break;
        off += n2;
    }
    if (off < cap)
        buf[off] = '\0';
    return buf;
}

static void compute_primary_deltas(int inst_index, int compare_slot, int* d_str, int* d_dex,
                                   int* d_vit, int* d_int, int* d_armor, int* d_res_phys)
{
    if (d_str)
        *d_str = 0;
    if (d_dex)
        *d_dex = 0;
    if (d_vit)
        *d_vit = 0;
    if (d_int)
        *d_int = 0;
    if (d_armor)
        *d_armor = 0;
    if (d_res_phys)
        *d_res_phys = 0;
    if (compare_slot < 0)
        return;
    int equipped = rogue_equip_get((enum RogueEquipSlot) compare_slot);
    if (equipped < 0)
        return;
    /* We simulate equipping candidate by temporarily swapping and recomputing stat cache. For
     * determinism we snapshot+restore. */
    extern RoguePlayer g_exposed_player_for_stats; /* used by optimizer & tests */
    int original_inst = equipped;
    int cur_str = g_player_stat_cache.total_strength, cur_dex = g_player_stat_cache.total_dexterity,
        cur_vit = g_player_stat_cache.total_vitality,
        cur_int = g_player_stat_cache.total_intelligence;
    int cur_phys = g_player_stat_cache.resist_physical;
    int cur_armor = g_player_stat_cache.affix_armor_flat; /* armor flat aggregated */
    /* Equip candidate */
    rogue_equip_try((enum RogueEquipSlot) compare_slot, inst_index);
    rogue_stat_cache_mark_dirty();
    rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats);
    rogue_stat_cache_force_update(&g_exposed_player_for_stats);
    int cand_str = g_player_stat_cache.total_strength,
        cand_dex = g_player_stat_cache.total_dexterity,
        cand_vit = g_player_stat_cache.total_vitality,
        cand_int = g_player_stat_cache.total_intelligence;
    int cand_phys = g_player_stat_cache.resist_physical;
    int cand_armor = g_player_stat_cache.affix_armor_flat;
    /* Revert */
    rogue_equip_try((enum RogueEquipSlot) compare_slot, original_inst);
    rogue_stat_cache_mark_dirty();
    rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats);
    rogue_stat_cache_force_update(&g_exposed_player_for_stats);
    if (d_str)
        *d_str = cand_str - cur_str;
    if (d_dex)
        *d_dex = cand_dex - cur_dex;
    if (d_vit)
        *d_vit = cand_vit - cur_vit;
    if (d_int)
        *d_int = cand_int - cur_int;
    if (d_res_phys)
        *d_res_phys = cand_phys - cur_phys;
    if (d_armor)
        *d_armor = cand_armor - cur_armor;
}

char* rogue_equipment_compare_deltas(int inst_index, int compare_slot, char* buf, size_t cap)
{
    if (!buf || cap < 4)
        return NULL;
    buf[0] = '\0';
    if (compare_slot < 0)
        return buf;
    int equipped = rogue_equip_get((enum RogueEquipSlot) compare_slot);
    if (equipped < 0)
        return buf;
    int cand_min = rogue_item_instance_damage_min(inst_index);
    int cand_max = rogue_item_instance_damage_max(inst_index);
    int cur_min = rogue_item_instance_damage_min(equipped);
    int cur_max = rogue_item_instance_damage_max(equipped);
    int dmin = cand_min - cur_min;
    int dmax = cand_max - cur_max;
    int d_str, d_dex, d_vit, d_int, d_arm, d_rphys;
    compute_primary_deltas(inst_index, compare_slot, &d_str, &d_dex, &d_vit, &d_int, &d_arm,
                           &d_rphys);
    snprintf(buf, cap,
             "Delta Damage: %+d-%+d\nStr:%+d Dex:%+d Vit:%+d Int:%+d Armor:%+d PhysRes:%+d\n", dmin,
             dmax, d_str, d_dex, d_vit, d_int, d_arm, d_rphys);
    return buf;
}

char* rogue_item_tooltip_build_layered(int inst_index, int compare_slot, char* buf, size_t cap)
{
    if (!buf || cap < 8)
        return NULL;
    char base[512];
    rogue_item_tooltip_build(inst_index, base, sizeof base); /* base builder */
    size_t off = 0;
    int n = snprintf(buf + off, cap - off, "%s", base);
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    const RogueItemDef* d = it ? rogue_item_def_at(it->def_index) : NULL;
    if (d && d->base_armor > 0)
    {
        n = snprintf(buf + off, cap - off, "Implicit: +%d Armor\n", d->base_armor);
        if (n > 0 && n < (int) (cap - off))
            off += n;
    }
    if (it)
    {
        for (int s = 0; s < it->socket_count && s < 6; s++)
        {
            int gem = rogue_item_instance_get_socket(inst_index, s);
            if (gem >= 0)
            {
                n = snprintf(buf + off, cap - off, "Gem%d: id=%d\n", s, gem);
                if (n < 0 || n >= (int) (cap - off))
                    break;
                off += n;
            }
        }
    }
    if (d && d->set_id > 0)
    {
        n = snprintf(buf + off, cap - off, "Set: %d\n", d->set_id);
        if (n > 0 && n < (int) (cap - off))
            off += n;
    }
    /* Runeword indicator: we approximate by hashing def id into a faux pattern label if runeword
     * stats present (cache layer). */
    if (d)
    { /* If runeword layer contributed any primary stat for this item, show generic runeword line.
       */
        if (g_player_stat_cache.runeword_strength || g_player_stat_cache.runeword_dexterity ||
            g_player_stat_cache.runeword_vitality || g_player_stat_cache.runeword_intelligence)
        {
            n = snprintf(buf + off, cap - off, "Runeword: active\n");
            if (n > 0 && n < (int) (cap - off))
                off += n;
        }
    }
    if (compare_slot >= 0)
    {
        char delta[256];
        rogue_equipment_compare_deltas(inst_index, compare_slot, delta, sizeof delta);
        n = snprintf(buf + off, cap - off, "%s", delta);
        if (n > 0 && n < (int) (cap - off))
            off += n;
    }
    if (off < cap)
        buf[off] = '\0';
    return buf;
}

float rogue_equipment_proc_preview_dps(void)
{
    float sum = 0.f;
    for (int i = 0; i < 64; i++)
    {
        float t = rogue_proc_triggers_per_min(i);
        if (t <= 0)
            continue;
        sum += t / 60.f;
    }
    return sum;
}

int rogue_equipment_socket_select(int inst_index, int socket_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (socket_index < 0 || socket_index >= it->socket_count)
        return -2;
    g_socket_sel_inst = inst_index;
    g_socket_sel_index = socket_index;
    return 0;
}
int rogue_equipment_socket_place_gem(int gem_item_def_index)
{
    if (g_socket_sel_inst < 0 || g_socket_sel_index < 0)
        return -1;
    const RogueItemInstance* it = rogue_item_instance_at(g_socket_sel_inst);
    if (!it)
        return -2;
    if (gem_item_def_index < 0)
        return -3;
    if (rogue_item_instance_get_socket(g_socket_sel_inst, g_socket_sel_index) >= 0)
        return -4;
    int r = rogue_item_instance_socket_insert(g_socket_sel_inst, g_socket_sel_index,
                                              gem_item_def_index);
    g_socket_sel_inst = -1;
    g_socket_sel_index = -1;
    return r;
}
void rogue_equipment_socket_clear_selection(void)
{
    g_socket_sel_inst = -1;
    g_socket_sel_index = -1;
}

int rogue_equipment_transmog_select(enum RogueEquipSlot slot, int def_index)
{
    ensure_init();
    int r = rogue_equip_set_transmog(slot, def_index);
    if (r == 0)
        g_transmog_last[slot] = def_index;
    return r;
}
int rogue_equipment_transmog_last_selected(enum RogueEquipSlot slot)
{
    ensure_init();
    if (slot < 0 || slot >= ROGUE_EQUIP__COUNT)
        return -1;
    return g_transmog_last[slot];
}

unsigned int rogue_item_tooltip_hash(int inst_index, int compare_slot)
{
    char tmp[768];
    rogue_item_tooltip_build_layered(inst_index, compare_slot, tmp, sizeof tmp);
    return fnv1a(tmp);
}

char* rogue_equipment_gem_inventory_panel(char* buf, size_t cap)
{
    if (!buf || cap < 8)
        return NULL;
    size_t off = 0;
    int n = snprintf(buf + off, cap - off, "[Gem Inventory]\n");
    if (n < 0 || n >= (int) (cap - off))
        return buf;
    off += n;
    if (g_socket_sel_inst >= 0)
    {
        n = snprintf(buf + off, cap - off, "Selected: inst=%d socket=%d\n", g_socket_sel_inst,
                     g_socket_sel_index);
        if (n > 0 && n < (int) (cap - off))
            off += n;
    }
    /* Stub: list first 5 gem item defs encountered among equipped sockets. */
    int listed = 0;
    for (int slot = 0; slot < ROGUE_EQUIP__COUNT && listed < 5; ++slot)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) slot);
        if (inst < 0)
            continue;
        const RogueItemInstance* it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        for (int s = 0; s < it->socket_count && listed < 5; ++s)
        {
            int gem = rogue_item_instance_get_socket(inst, s);
            if (gem >= 0)
            {
                n = snprintf(buf + off, cap - off, "GemDef:%d (slot %d)\n", gem, s);
                if (n < 0 || n >= (int) (cap - off))
                    break;
                off += n;
                listed++;
            }
        }
    }
    if (off < cap)
        buf[off] = '\0';
    return buf;
}

float rogue_equipment_softcap_saturation(float value, float cap, float softness)
{
    if (cap <= 0.f || softness <= 0.f)
        return 0.f;
    float adjusted = rogue_soft_cap_apply(value, cap, softness);
    return fminf(1.f, adjusted / cap);
}
