#include "../../game/buffs.h"
#include "../../world/tilemap.h"
#include "../app/app_state.h"
#include "../equipment/equipment.h"
#include "../inventory/inventory_entries.h"
#include "../inventory/inventory_query.h"
#include "../inventory/inventory_tag_rules.h"
#include "../inventory/inventory_tags.h"
#include "../loot/loot_instances.h"
#include "../skills/skills.h"
#include "../vendor/vendor.h"
#include "save_intern.h"
#include "save_internal.h"
#include "save_replay.h"
#include "save_utils.h"
#include <string.h>

/* Forward-declared helpers from original implementation */
static int write_player_component(FILE* f);
static int read_player_component(FILE* f, size_t size);
static int write_inventory_component(FILE* f);
static int read_inventory_component(FILE* f, size_t size);
static int write_skills_component(FILE* f);
static int read_skills_component(FILE* f, size_t size);
static int write_buffs_component(FILE* f);
static int read_buffs_component(FILE* f, size_t size);
static int write_vendor_component(FILE* f);
static int read_vendor_component(FILE* f, size_t size);
static int write_strings_component(FILE* f);
static int read_strings_component(FILE* f, size_t size);
static int write_world_meta_component(FILE* f);
static int read_world_meta_component(FILE* f, size_t size);
static int write_replay_component(FILE* f);
static int read_replay_component(FILE* f, size_t size);
static int write_inv_entries_component(FILE* f);
static int read_inv_entries_component(FILE* f, size_t size);
static int write_inv_tags_component(FILE* f);
static int read_inv_tags_component(FILE* f, size_t size);
static int write_inv_tag_rules_component(FILE* f);
static int read_inv_tag_rules_component(FILE* f, size_t size);
static int write_inv_saved_searches_component(FILE* f);
static int read_inv_saved_searches_component(FILE* f, size_t size);

/* Inventory record diff metrics (exposed via header functions) */
void rogue_save_inventory_diff_metrics(unsigned* reused, unsigned* rewritten);

/* Implementations moved wholesale from original source (trimmed of unrelated static state) */

/* Player */
static int write_player_component(FILE* f)
{
    fwrite(&g_app.player.level, sizeof g_app.player.level, 1, f);
    fwrite(&g_app.player.xp, sizeof g_app.player.xp, 1, f);
    fwrite(&g_app.player.xp_to_next, sizeof g_app.player.xp_to_next, 1, f);
    fwrite(&g_app.player.xp_total_accum, sizeof g_app.player.xp_total_accum, 1, f);
    fwrite(&g_app.player.health, sizeof g_app.player.health, 1, f);
    fwrite(&g_app.player.mana, sizeof g_app.player.mana, 1, f);
    fwrite(&g_app.player.action_points, sizeof g_app.player.action_points, 1, f);
    fwrite(&g_app.player.strength, sizeof g_app.player.strength, 1, f);
    fwrite(&g_app.player.dexterity, sizeof g_app.player.dexterity, 1, f);
    fwrite(&g_app.player.vitality, sizeof g_app.player.vitality, 1, f);
    fwrite(&g_app.player.intelligence, sizeof g_app.player.intelligence, 1, f);
    fwrite(&g_app.talent_points, sizeof g_app.talent_points, 1, f);
    fwrite(&g_app.analytics_damage_dealt_total, sizeof g_app.analytics_damage_dealt_total, 1, f);
    fwrite(&g_app.analytics_gold_earned_total, sizeof g_app.analytics_gold_earned_total, 1, f);
    fwrite(&g_app.permadeath_mode, sizeof g_app.permadeath_mode, 1, f);
    fwrite(&g_app.player.equipped_weapon_id, sizeof g_app.player.equipped_weapon_id, 1, f);
    fwrite(&g_app.player.weapon_infusion, sizeof g_app.player.weapon_infusion, 1, f);
    fwrite(&g_app.session_start_seconds, sizeof g_app.session_start_seconds, 1, f);
    fwrite(&g_app.inventory_sort_mode, sizeof g_app.inventory_sort_mode, 1, f);
    int equip_count = ROGUE_EQUIP__COUNT;
    fwrite(&equip_count, sizeof equip_count, 1, f);
    for (int i = 0; i < equip_count; i++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) i);
        fwrite(&inst, sizeof inst, 1, f);
    }
    return 0;
}

static int read_player_component(FILE* f, size_t size)
{
    if (size < sizeof(int) * 4)
        return -1;
    long start = ftell(f);
    int remain = (int) size;
    fread(&g_app.player.level, sizeof g_app.player.level, 1, f);
    fread(&g_app.player.xp, sizeof g_app.player.xp, 1, f);
    remain -= sizeof(int) * 2;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.xp_to_next, sizeof g_app.player.xp_to_next, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.xp_to_next = 0;
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.player.xp_total_accum, sizeof g_app.player.xp_total_accum, 1, f);
        remain -= (int) sizeof(unsigned long long);
    }
    else
        g_app.player.xp_total_accum = 0ULL;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.health, sizeof g_app.player.health, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.health = 0;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.mana, sizeof g_app.player.mana, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.mana = 0;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.action_points, sizeof g_app.player.action_points, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.action_points = 0;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.strength, sizeof g_app.player.strength, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.strength = 5;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.dexterity, sizeof g_app.player.dexterity, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.dexterity = 5;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.vitality, sizeof g_app.player.vitality, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.vitality = 15;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.intelligence, sizeof g_app.player.intelligence, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.intelligence = 5;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.talent_points, sizeof g_app.talent_points, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.talent_points = 0;
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.analytics_damage_dealt_total, sizeof g_app.analytics_damage_dealt_total, 1, f);
        remain -= (int) sizeof(unsigned long long);
    }
    else
        g_app.analytics_damage_dealt_total = 0ULL;
    if (remain >= (int) sizeof(unsigned long long))
    {
        fread(&g_app.analytics_gold_earned_total, sizeof g_app.analytics_gold_earned_total, 1, f);
        remain -= (int) sizeof(unsigned long long);
    }
    else
        g_app.analytics_gold_earned_total = 0ULL;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.permadeath_mode, sizeof g_app.permadeath_mode, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.permadeath_mode = 0;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.equipped_weapon_id, sizeof g_app.player.equipped_weapon_id, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.equipped_weapon_id = -1;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.player.weapon_infusion, sizeof g_app.player.weapon_infusion, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.player.weapon_infusion = 0;
    if (remain >= (int) sizeof(double))
    {
        fread(&g_app.session_start_seconds, sizeof g_app.session_start_seconds, 1, f);
        remain -= sizeof(double);
    }
    else
        g_app.session_start_seconds = 0.0;
    if (remain >= (int) sizeof(int))
    {
        fread(&g_app.inventory_sort_mode, sizeof g_app.inventory_sort_mode, 1, f);
        remain -= sizeof(int);
    }
    else
        g_app.inventory_sort_mode = 0;
    if (remain >= (int) sizeof(int))
    {
        int equip_count = 0;
        fread(&equip_count, sizeof equip_count, 1, f);
        remain -= sizeof(int);
        if (equip_count > 0 && equip_count <= ROGUE_EQUIP__COUNT)
        {
            for (int i = 0; i < equip_count && remain >= (int) sizeof(int); i++)
            {
                int inst = -1;
                fread(&inst, sizeof inst, 1, f);
                remain -= sizeof(int);
                if (inst >= 0)
                    rogue_equip_try((enum RogueEquipSlot) i, inst);
            }
        }
    }
    (void) start;
    return 0;
}

/* ---- Inventory (with diff metrics support) ---- */
typedef struct InvRecordSnapshot
{
    int def_index, quantity, rarity, prefix_index, prefix_value, suffix_index, suffix_value,
        durability_cur, durability_max, enchant_level;
} InvRecordSnapshot;
static InvRecordSnapshot* g_inv_prev_records = NULL;
static unsigned g_inv_prev_count = 0;
static unsigned g_inv_diff_reused_last = 0;
static unsigned g_inv_diff_rewritten_last = 0;

void rogue_save_inventory_diff_metrics(unsigned* reused, unsigned* rewritten)
{
    if (reused)
        *reused = g_inv_diff_reused_last;
    if (rewritten)
        *rewritten = g_inv_diff_rewritten_last;
}

static void inv_record_snapshot_update(const InvRecordSnapshot* cur, unsigned count)
{
    InvRecordSnapshot* nr = (InvRecordSnapshot*) realloc(
        g_inv_prev_records, sizeof(InvRecordSnapshot) * (size_t) count);
    if (!nr && count > 0)
        return;
    if (nr)
    {
        g_inv_prev_records = nr;
        memcpy(g_inv_prev_records, cur, sizeof(InvRecordSnapshot) * (size_t) count);
        g_inv_prev_count = count;
    }
}

/* Probe helper used by save loop before deciding section reuse */
int inventory_component_probe_and_prepare_reuse(void)
{
    int count = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (it)
            count++;
    }
    if (count == 0)
    {
        int changed = (g_inv_prev_count != 0);
        if (!changed)
        {
            g_inv_diff_reused_last = 0;
            g_inv_diff_rewritten_last = 0;
        }
        return changed;
    }
    InvRecordSnapshot* cur =
        (InvRecordSnapshot*) malloc(sizeof(InvRecordSnapshot) * (size_t) count);
    if (!cur)
        return 1;
    int out = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        cur[out++] = (InvRecordSnapshot){it->def_index,    it->quantity,       it->rarity,
                                         it->prefix_index, it->prefix_value,   it->suffix_index,
                                         it->suffix_value, it->durability_cur, it->durability_max,
                                         it->enchant_level};
    }
    int changed = 0;
    if (g_inv_prev_count != (unsigned) count)
    {
        changed = 1;
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            if (memcmp(&g_inv_prev_records[i], &cur[i], sizeof(InvRecordSnapshot)) != 0)
            {
                changed = 1;
                break;
            }
        }
    }
    if (!changed)
    {
        g_inv_diff_reused_last = (unsigned) count;
        g_inv_diff_rewritten_last = 0;
    }
    free(cur);
    return changed;
}

static int write_inventory_component(FILE* f)
{
    int count = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (it)
            count++;
    }
    if (g_active_write_version >= 4)
    {
        if (rogue_write_varuint(f, (uint32_t) count) != 0)
            return -1;
    }
    else
    {
        if (fwrite(&count, sizeof count, 1, f) != 1)
            return -1;
    }
    if (count == 0)
    {
        g_inv_prev_count = 0;
        g_inv_diff_reused_last = 0;
        g_inv_diff_rewritten_last = 0;
        return 0;
    }
    InvRecordSnapshot* cur =
        (InvRecordSnapshot*) malloc(sizeof(InvRecordSnapshot) * (size_t) count);
    if (!cur)
        return -1;
    int out = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        cur[out++] = (InvRecordSnapshot){it->def_index,    it->quantity,       it->rarity,
                                         it->prefix_index, it->prefix_value,   it->suffix_index,
                                         it->suffix_value, it->durability_cur, it->durability_max,
                                         it->enchant_level};
    }
    if (out != count)
    {
        free(cur);
        return -1;
    }
    g_inv_diff_reused_last = 0;
    g_inv_diff_rewritten_last = 0;
    if (g_save_incremental_enabled && g_inv_prev_count == (unsigned) count)
    {
        for (int i = 0; i < count; i++)
        {
            if (memcmp(&g_inv_prev_records[i], &cur[i], sizeof(InvRecordSnapshot)) == 0)
                g_inv_diff_reused_last++;
            else
                g_inv_diff_rewritten_last++;
        }
    }
    else
    {
        g_inv_diff_rewritten_last = (unsigned) count;
    }
    for (int i = 0; i < count; i++)
    {
        InvRecordSnapshot* r = &cur[i];
        fwrite(&r->def_index, sizeof(r->def_index), 1, f);
        fwrite(&r->quantity, sizeof(r->quantity), 1, f);
        fwrite(&r->rarity, sizeof(r->rarity), 1, f);
        fwrite(&r->prefix_index, sizeof(r->prefix_index), 1, f);
        fwrite(&r->prefix_value, sizeof(r->prefix_value), 1, f);
        fwrite(&r->suffix_index, sizeof(r->suffix_index), 1, f);
        fwrite(&r->suffix_value, sizeof(r->suffix_value), 1, f);
        fwrite(&r->durability_cur, sizeof(r->durability_cur), 1, f);
        fwrite(&r->durability_max, sizeof(r->durability_max), 1, f);
        fwrite(&r->enchant_level, sizeof(r->enchant_level), 1, f);
    }
    inv_record_snapshot_update(cur, (unsigned) count);
    free(cur);
    return 0;
}

static int read_inventory_component(FILE* f, size_t size)
{
    if (!g_app.item_instances || g_app.item_instance_cap <= 0)
    {
        rogue_items_init_runtime();
    }
    int count = 0;
    long section_start = ftell(f);
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (rogue_read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count < 0)
        return -1;
    long after_count = ftell(f);
    size_t count_bytes = (size_t) (after_count - section_start);
    if (count == 0)
        return 0;
    size_t remaining = (size_t) ((size > count_bytes) ? (size - count_bytes) : 0);
    size_t rec_ints = 0;
    if (remaining >= (size_t) count * (sizeof(int) * 10))
        rec_ints = 10;
    else if (remaining >= (size_t) count * (sizeof(int) * 9))
        rec_ints = 9;
    else if (remaining >= (size_t) count * (sizeof(int) * 7))
        rec_ints = 7;
    else
        return -1;
    for (int i = 0; i < count; i++)
    {
        int def_index = 0, quantity = 0, rarity = 0, pidx = 0, pval = 0, sidx = 0, sval = 0;
        if (fread(&def_index, sizeof def_index, 1, f) != 1)
            return -1;
        if (fread(&quantity, sizeof quantity, 1, f) != 1)
            return -1;
        if (fread(&rarity, sizeof rarity, 1, f) != 1)
            return -1;
        if (fread(&pidx, sizeof pidx, 1, f) != 1)
            return -1;
        if (fread(&pval, sizeof pval, 1, f) != 1)
            return -1;
        if (fread(&sidx, sizeof sidx, 1, f) != 1)
            return -1;
        if (fread(&sval, sizeof sval, 1, f) != 1)
            return -1;
        int durability_cur = 0, durability_max = 0, enchant_level = 0;
        if (rec_ints >= 9)
        {
            if (fread(&durability_cur, sizeof durability_cur, 1, f) != 1)
                return -1;
            if (fread(&durability_max, sizeof durability_max, 1, f) != 1)
                return -1;
        }
        if (rec_ints >= 10)
        {
            if (fread(&enchant_level, sizeof enchant_level, 1, f) != 1)
                return -1;
        }
        int inst = rogue_items_spawn(def_index, quantity, 0.0f, 0.0f);
        if (inst >= 0)
        {
            rogue_item_instance_apply_affixes(inst, rarity, pidx, pval, sidx, sval);
            if (durability_max > 0)
            {
                RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
                if (it)
                {
                    it->durability_max = durability_max;
                    it->durability_cur = durability_cur;
                }
            }
            if (enchant_level > 0)
            {
                RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
                if (it)
                {
                    it->enchant_level = enchant_level;
                }
            }
        }
    }
    rogue_items_sync_app_view();
    return 0;
}

/* Skills */
static int write_skills_component(FILE* f)
{
    if (g_active_write_version >= 4)
    {
        if (rogue_write_varuint(f, (uint32_t) g_app.skill_count) != 0)
            return -1;
    }
    else
        fwrite(&g_app.skill_count, sizeof g_app.skill_count, 1, f);
    for (int i = 0; i < g_app.skill_count; i++)
    {
        const RogueSkillState* st = rogue_skill_get_state(i);
        int rank = st ? st->rank : 0;
        double cd = st ? st->cooldown_end_ms : 0.0;
        fwrite(&rank, sizeof rank, 1, f);
        fwrite(&cd, sizeof cd, 1, f);
        double cast_progress = st ? st->cast_progress_ms : 0.0;
        double channel_end = st ? st->channel_end_ms : 0.0;
        double next_charge_ready = st ? st->next_charge_ready_ms : 0.0;
        int charges_cur = st ? st->charges_cur : 0;
        unsigned char casting_active = st ? st->casting_active : 0;
        unsigned char channel_active = st ? st->channel_active : 0;
        fwrite(&cast_progress, sizeof cast_progress, 1, f);
        fwrite(&channel_end, sizeof channel_end, 1, f);
        fwrite(&next_charge_ready, sizeof next_charge_ready, 1, f);
        fwrite(&charges_cur, sizeof charges_cur, 1, f);
        fwrite(&casting_active, sizeof casting_active, 1, f);
        fwrite(&channel_active, sizeof channel_active, 1, f);
    }
    return 0;
}

static int read_skills_component(FILE* f, size_t size)
{
    long section_start = ftell(f);
    int count = 0;
    size_t count_bytes = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (rogue_read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else
    {
        if (fread(&count, sizeof count, 1, f) != 1)
            return -1;
    }
    count_bytes = (size_t) (ftell(f) - section_start);
    if (count < 0 || count > 4096)
        return -1;
    size_t remaining = (size_t) size - count_bytes;
    size_t minimal_rec = sizeof(int) + sizeof(double);
    size_t extended_extra = sizeof(double) * 3 + sizeof(int) + 2 * sizeof(unsigned char);
    int has_extended = 0;
    if (count > 0)
    {
        if (remaining >= (size_t) count * (minimal_rec + extended_extra))
            has_extended = 1;
    }
    int limit = (count < g_app.skill_count) ? count : g_app.skill_count;
    for (int i = 0; i < count; i++)
    {
        int rank = 0;
        double cd = 0.0;
        if (fread(&rank, sizeof rank, 1, f) != 1)
            return -1;
        if (fread(&cd, sizeof cd, 1, f) != 1)
            return -1;
        double cast_progress = 0.0, channel_end = 0.0, next_charge_ready = 0.0;
        int charges_cur = 0;
        unsigned char casting_active = 0, channel_active = 0;
        if (has_extended)
        {
            if (fread(&cast_progress, sizeof cast_progress, 1, f) != 1)
                return -1;
            if (fread(&channel_end, sizeof channel_end, 1, f) != 1)
                return -1;
            if (fread(&next_charge_ready, sizeof next_charge_ready, 1, f) != 1)
                return -1;
            if (fread(&charges_cur, sizeof charges_cur, 1, f) != 1)
                return -1;
            if (fread(&casting_active, sizeof casting_active, 1, f) != 1)
                return -1;
            if (fread(&channel_active, sizeof channel_active, 1, f) != 1)
                return -1;
        }
        if (i < limit)
        {
            const RogueSkillDef* d = rogue_skill_get_def(i);
            struct RogueSkillState* st = (struct RogueSkillState*) rogue_skill_get_state(i);
            if (d && st)
            {
                if (rank > d->max_rank)
                    rank = d->max_rank;
                st->rank = rank;
                st->cooldown_end_ms = cd;
                if (has_extended)
                {
                    st->cast_progress_ms = cast_progress;
                    st->channel_end_ms = channel_end;
                    st->next_charge_ready_ms = next_charge_ready;
                    st->charges_cur = charges_cur;
                    st->casting_active = casting_active;
                    st->channel_active = channel_active;
                }
            }
        }
    }
    return 0;
}

/* Buffs */
static int write_buffs_component(FILE* f)
{
    int active_count = rogue_buffs_active_count();
    if (g_active_write_version >= 4)
    {
        if (rogue_write_varuint(f, (uint32_t) active_count) != 0)
            return -1;
    }
    else
        fwrite(&active_count, sizeof active_count, 1, f);
    for (int i = 0; i < active_count; i++)
    {
        RogueBuff tmp;
        if (!rogue_buffs_get_active(i, &tmp))
            break;
        double now = g_app.game_time_ms;
        double remaining_ms = (tmp.end_ms > now) ? (tmp.end_ms - now) : 0.0;
        int type = tmp.type;
        int magnitude = tmp.magnitude;
        fwrite(&type, sizeof type, 1, f);
        fwrite(&magnitude, sizeof magnitude, 1, f);
        fwrite(&remaining_ms, sizeof remaining_ms, 1, f);
    }
    return 0;
}

static int read_buffs_component(FILE* f, size_t size)
{
    long start = ftell(f);
    int count = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (rogue_read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count < 0 || count > 512)
        return -1;
    size_t count_bytes = (size_t) (ftell(f) - start);
    size_t remaining = size - count_bytes;
    if (count == 0)
        return 0;
    size_t rec_size = remaining / (size_t) count;
    for (int i = 0; i < count; i++)
    {
        if (rec_size >= sizeof(int) * 3 + sizeof(double))
        {
            struct LegacyBuff
            {
                int active;
                int type;
                double end_ms;
                int magnitude;
            } lb;
            if (fread(&lb, sizeof lb, 1, f) != 1)
                return -1;
            double now = g_app.game_time_ms;
            double remaining_ms = (lb.end_ms > now) ? (lb.end_ms - now) : 0.0;
            rogue_buffs_apply((RogueBuffType) lb.type, lb.magnitude, remaining_ms, now,
                              ROGUE_BUFF_STACK_ADD, 1);
        }
        else
        {
            int type = 0;
            int magnitude = 0;
            double remaining_ms = 0.0;
            if (fread(&type, sizeof type, 1, f) != 1)
                return -1;
            if (fread(&magnitude, sizeof magnitude, 1, f) != 1)
                return -1;
            if (fread(&remaining_ms, sizeof remaining_ms, 1, f) != 1)
                return -1;
            double now = g_app.game_time_ms;
            rogue_buffs_apply((RogueBuffType) type, magnitude, remaining_ms, now,
                              ROGUE_BUFF_STACK_ADD, 1);
        }
    }
    return 0;
}

/* Vendor */
static int write_vendor_component(FILE* f)
{
    fwrite(&g_app.vendor_seed, sizeof g_app.vendor_seed, 1, f);
    fwrite(&g_app.vendor_time_accum_ms, sizeof g_app.vendor_time_accum_ms, 1, f);
    fwrite(&g_app.vendor_restock_interval_ms, sizeof g_app.vendor_restock_interval_ms, 1, f);
    int count = rogue_vendor_item_count();
    if (count < 0)
        count = 0;
    if (count > ROGUE_VENDOR_SLOT_CAP)
        count = ROGUE_VENDOR_SLOT_CAP;
    fwrite(&count, sizeof count, 1, f);
    for (int i = 0; i < count; i++)
    {
        const RogueVendorItem* it = rogue_vendor_get(i);
        if (!it)
        {
            int zero = 0;
            fwrite(&zero, sizeof zero, 1, f);
            fwrite(&zero, sizeof zero, 1, f);
            fwrite(&zero, sizeof zero, 1, f);
        }
        else
        {
            fwrite(&it->def_index, sizeof it->def_index, 1, f);
            fwrite(&it->rarity, sizeof it->rarity, 1, f);
            fwrite(&it->price, sizeof it->price, 1, f);
        }
    }
    return 0;
}

static int read_vendor_component(FILE* f, size_t size)
{
    (void) size;
    fread(&g_app.vendor_seed, sizeof g_app.vendor_seed, 1, f);
    fread(&g_app.vendor_time_accum_ms, sizeof g_app.vendor_time_accum_ms, 1, f);
    fread(&g_app.vendor_restock_interval_ms, sizeof g_app.vendor_restock_interval_ms, 1, f);
    int count = 0;
    if (fread(&count, sizeof count, 1, f) != 1)
        return 0;
    if (count < 0 || count > ROGUE_VENDOR_SLOT_CAP)
        count = 0;
    rogue_vendor_reset();
    for (int i = 0; i < count; i++)
    {
        int def = 0, rar = 0, price = 0;
        if (fread(&def, sizeof def, 1, f) != 1)
            return -1;
        if (fread(&rar, sizeof rar, 1, f) != 1)
            return -1;
        if (fread(&price, sizeof price, 1, f) != 1)
            return -1;
        if (def >= 0)
        {
            int recomputed = rogue_vendor_price_formula(def, rar);
            rogue_vendor_append(def, rar, recomputed);
        }
    }
    return 0;
}

/* Strings intern table */
static int write_strings_component(FILE* f)
{
    int count = rogue_save_intern_count();
    if (g_active_write_version >= 4)
    {
        if (rogue_write_varuint(f, (uint32_t) count) != 0)
            return -1;
    }
    else
        fwrite(&count, sizeof count, 1, f);
    for (int i = 0; i < count; i++)
    {
        const char* s = rogue_save_intern_get(i);
        uint32_t len = (uint32_t) strlen(s);
        if (g_active_write_version >= 4)
        {
            if (rogue_write_varuint(f, len) != 0)
                return -1;
        }
        else
            fwrite(&len, sizeof len, 1, f);
        if (fwrite(s, 1, len, f) != len)
            return -1;
    }
    return 0;
}

static int read_strings_component(FILE* f, size_t size)
{
    (void) size;
    int count = 0;
    if (g_active_read_version >= 4)
    {
        uint32_t c = 0;
        if (rogue_read_varuint(f, &c) != 0)
            return -1;
        count = (int) c;
    }
    else if (fread(&count, sizeof count, 1, f) != 1)
        return -1;
    if (count < 0)
        return -1;
    rogue_save_intern_reset_and_reserve(count);
    for (int i = 0; i < count; i++)
    {
        uint32_t len = 0;
        if (g_active_read_version >= 4)
        {
            if (rogue_read_varuint(f, &len) != 0)
                return -1;
        }
        else if (fread(&len, sizeof len, 1, f) != 1)
            return -1;
        if (len > 4096)
            return -1;
        char* buf = (char*) malloc(len + 1);
        if (!buf)
            return -1;
        if (fread(buf, 1, len, f) != len)
        {
            free(buf);
            return -1;
        }
        buf[len] = '\0';
        rogue_save_intern_set_loaded(i, buf);
    }
    return 0;
}

/* World meta */
static int write_world_meta_component(FILE* f)
{
    fwrite(&g_app.pending_seed, sizeof g_app.pending_seed, 1, f);
    fwrite(&g_app.gen_water_level, sizeof g_app.gen_water_level, 1, f);
    fwrite(&g_app.gen_cave_thresh, sizeof g_app.gen_cave_thresh, 1, f);
    fwrite(&g_app.gen_noise_octaves, sizeof g_app.gen_noise_octaves, 1, f);
    fwrite(&g_app.gen_noise_gain, sizeof g_app.gen_noise_gain, 1, f);
    fwrite(&g_app.gen_noise_lacunarity, sizeof g_app.gen_noise_lacunarity, 1, f);
    fwrite(&g_app.gen_river_sources, sizeof g_app.gen_river_sources, 1, f);
    fwrite(&g_app.gen_river_max_length, sizeof g_app.gen_river_max_length, 1, f);
    return 0;
}

static int read_world_meta_component(FILE* f, size_t size)
{
    size_t remain = size;
    if (remain < sizeof(unsigned int) + sizeof(double) * 2)
        return -1;
    fread(&g_app.pending_seed, sizeof g_app.pending_seed, 1, f);
    remain -= sizeof g_app.pending_seed;
    fread(&g_app.gen_water_level, sizeof g_app.gen_water_level, 1, f);
    remain -= sizeof g_app.gen_water_level;
    fread(&g_app.gen_cave_thresh, sizeof g_app.gen_cave_thresh, 1, f);
    remain -= sizeof g_app.gen_cave_thresh;
    if (remain >= sizeof g_app.gen_noise_octaves)
    {
        fread(&g_app.gen_noise_octaves, sizeof g_app.gen_noise_octaves, 1, f);
        remain -= sizeof g_app.gen_noise_octaves;
    }
    if (remain >= sizeof g_app.gen_noise_gain)
    {
        fread(&g_app.gen_noise_gain, sizeof g_app.gen_noise_gain, 1, f);
        remain -= sizeof g_app.gen_noise_gain;
    }
    if (remain >= sizeof g_app.gen_noise_lacunarity)
    {
        fread(&g_app.gen_noise_lacunarity, sizeof g_app.gen_noise_lacunarity, 1, f);
        remain -= sizeof g_app.gen_noise_lacunarity;
    }
    if (remain >= sizeof g_app.gen_river_sources)
    {
        fread(&g_app.gen_river_sources, sizeof g_app.gen_river_sources, 1, f);
        remain -= sizeof g_app.gen_river_sources;
    }
    if (remain >= sizeof g_app.gen_river_max_length)
    {
        fread(&g_app.gen_river_max_length, sizeof g_app.gen_river_max_length, 1, f);
        remain -= sizeof g_app.gen_river_max_length;
    }
    return 0;
}

/* Replay (v8) */
static int write_replay_component(FILE* f)
{
    rogue_replay_compute_hash();
    uint32_t count = g_replay_event_count;
    fwrite(&count, sizeof count, 1, f);
    if (count)
    {
        fwrite(g_replay_events, sizeof(RogueReplayEvent), count, f);
    }
    fwrite(g_last_replay_hash, 1, 32, f);
    return 0;
}

static int read_replay_component(FILE* f, size_t size)
{
    if (size < sizeof(uint32_t) + 32)
        return -1;
    uint32_t count = 0;
    fread(&count, sizeof count, 1, f);
    size_t need = (size_t) count * sizeof(RogueReplayEvent) + 32;
    if (size < sizeof(uint32_t) + need)
        return -1;
    if (count > ROGUE_REPLAY_MAX_EVENTS)
        return -1;
    if (count)
    {
        fread(g_replay_events, sizeof(RogueReplayEvent), count, f);
    }
    fread(g_last_replay_hash, 1, 32, f);
    g_replay_event_count = count;
    RogueSHA256Ctx sha;
    unsigned char chk[32];
    rogue_sha256_init(&sha);
    rogue_sha256_update(&sha, g_replay_events, count * sizeof(RogueReplayEvent));
    rogue_sha256_final(&sha, chk);
    if (memcmp(chk, g_last_replay_hash, 32) != 0)
        return -1;
    return 0;
}

/* Inventory entries */
static int write_inv_entries_component(FILE* f)
{
    uint32_t count = 0;
    for (int i = 0; i < 4096; i++)
    {
        if (rogue_inventory_quantity(i) > 0)
            count++;
    }
    if (rogue_write_varuint(f, count) != 0)
        return -1;
    for (int i = 0; i < 4096; i++)
    {
        uint64_t q = rogue_inventory_quantity(i);
        if (q > 0)
        {
            unsigned lbl = rogue_inventory_entry_labels(i);
            int def = i;
            fwrite(&def, sizeof(def), 1, f);
            fwrite(&q, sizeof(q), 1, f);
            fwrite(&lbl, sizeof(lbl), 1, f);
        }
    }
    rogue_inventory_entries_dirty_pairs(NULL, NULL, 0);
    return 0;
}

static int read_inv_entries_component(FILE* f, size_t size)
{
    uint32_t count = 0;
    if (rogue_read_varuint(f, &count) != 0)
        return -1;
    size_t need = (size_t) count * (sizeof(int) + sizeof(uint64_t) + sizeof(unsigned));
    if (size < need)
        return -1;
    rogue_inventory_entries_init();
    for (uint32_t k = 0; k < count; k++)
    {
        int def = 0;
        uint64_t qty = 0;
        unsigned lbl = 0;
        if (fread(&def, sizeof(def), 1, f) != 1)
            return -1;
        if (fread(&qty, sizeof(qty), 1, f) != 1)
            return -1;
        if (fread(&lbl, sizeof(lbl), 1, f) != 1)
            return -1;
        if (def >= 0)
        {
            rogue_inventory_register_pickup(def, qty);
            if (lbl)
                rogue_inventory_entry_set_labels(def, lbl);
        }
    }
    rogue_inventory_entries_dirty_pairs(NULL, NULL, 0);
    return 0;
}

/* Inventory tags */
static int write_inv_tags_component(FILE* f)
{
    uint32_t count = 0;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        if (rogue_inv_tags_get_flags(i) || rogue_inv_tags_list(i, NULL, 0) > 0)
            count++;
    }
    if (rogue_write_varuint(f, count) != 0)
        return -1;
    if (count == 0)
        return 0;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        unsigned fl = rogue_inv_tags_get_flags(i);
        int tc = rogue_inv_tags_list(i, NULL, 0);
        if (fl == 0 && tc <= 0)
            continue;
        if (tc < 0)
            tc = 0;
        if (tc > ROGUE_INV_TAG_MAX_TAGS_PER_DEF)
            tc = ROGUE_INV_TAG_MAX_TAGS_PER_DEF;
        fwrite(&i, sizeof(int), 1, f);
        fwrite(&fl, sizeof(fl), 1, f);
        unsigned char tcc = (unsigned char) tc;
        fwrite(&tcc, 1, 1, f);
        if (tc > 0)
        {
            const char* tmp[ROGUE_INV_TAG_MAX_TAGS_PER_DEF];
            rogue_inv_tags_list(i, tmp, ROGUE_INV_TAG_MAX_TAGS_PER_DEF);
            for (int k = 0; k < tc; k++)
            {
                size_t len = strlen(tmp[k]);
                if (len > 255)
                    len = 255;
                unsigned char l = (unsigned char) len;
                fwrite(&l, 1, 1, f);
                fwrite(tmp[k], 1, len, f);
            }
        }
    }
    return 0;
}

static int read_inv_tags_component(FILE* f, size_t size)
{
    uint32_t count = 0;
    if (rogue_read_varuint(f, &count) != 0)
        return -1;
    size_t consumed = 0;
    rogue_inv_tags_init();
    for (uint32_t r = 0; r < count; r++)
    {
        int def = 0;
        unsigned flags = 0;
        unsigned char tcc = 0;
        if (fread(&def, sizeof(def), 1, f) != 1)
            return -1;
        if (fread(&flags, sizeof(flags), 1, f) != 1)
            return -1;
        if (fread(&tcc, 1, 1, f) != 1)
            return -1;
        consumed += sizeof(def) + sizeof(flags) + 1;
        rogue_inv_tags_set_flags(def, flags);
        for (unsigned char k = 0; k < tcc; k++)
        {
            unsigned char l = 0;
            if (fread(&l, 1, 1, f) != 1)
                return -1;
            consumed += 1;
            if (l > 0)
            {
                char buf[256];
                if (l >= sizeof(buf))
                    l = (unsigned char) (sizeof(buf) - 1);
                if (fread(buf, 1, l, f) != l)
                    return -1;
                buf[l] = '\0';
                rogue_inv_tags_add_tag(def, buf);
                consumed += l;
            }
            if (consumed > size)
                return -1;
        }
    }
    return 0;
}

static int write_inv_tag_rules_component(FILE* f) { return rogue_inv_tag_rules_write(f); }
static int read_inv_tag_rules_component(FILE* f, size_t size)
{
    return rogue_inv_tag_rules_read(f, size);
}

static int write_inv_saved_searches_component(FILE* f)
{
    return rogue_inventory_saved_searches_write(f);
}
static int read_inv_saved_searches_component(FILE* f, size_t size)
{
    return rogue_inventory_saved_searches_read(f, size);
}

/* Component descriptors and registry */
static RogueSaveComponent PLAYER_COMP = {ROGUE_SAVE_COMP_PLAYER, write_player_component,
                                         read_player_component, "player"};
static RogueSaveComponent INVENTORY_COMP = {ROGUE_SAVE_COMP_INVENTORY, write_inventory_component,
                                            read_inventory_component, "inventory"};
static RogueSaveComponent INV_ENTRIES_COMP = {ROGUE_SAVE_COMP_INV_ENTRIES,
                                              write_inv_entries_component,
                                              read_inv_entries_component, "inv_entries"};
static RogueSaveComponent INV_TAGS_COMP = {ROGUE_SAVE_COMP_INV_TAGS, write_inv_tags_component,
                                           read_inv_tags_component, "inv_tags"};
static RogueSaveComponent INV_TAG_RULES_COMP = {ROGUE_SAVE_COMP_INV_TAG_RULES,
                                                write_inv_tag_rules_component,
                                                read_inv_tag_rules_component, "inv_tag_rules"};
static RogueSaveComponent INV_SAVED_SEARCHES_COMP = {
    ROGUE_SAVE_COMP_INV_SAVED_SEARCHES, write_inv_saved_searches_component,
    read_inv_saved_searches_component, "inv_saved_searches"};
static RogueSaveComponent SKILLS_COMP = {ROGUE_SAVE_COMP_SKILLS, write_skills_component,
                                         read_skills_component, "skills"};
static RogueSaveComponent BUFFS_COMP = {ROGUE_SAVE_COMP_BUFFS, write_buffs_component,
                                        read_buffs_component, "buffs"};
static RogueSaveComponent VENDOR_COMP = {ROGUE_SAVE_COMP_VENDOR, write_vendor_component,
                                         read_vendor_component, "vendor"};
static RogueSaveComponent STRINGS_COMP = {ROGUE_SAVE_COMP_STRINGS, write_strings_component,
                                          read_strings_component, "strings"};
static RogueSaveComponent WORLD_META_COMP = {ROGUE_SAVE_COMP_WORLD_META, write_world_meta_component,
                                             read_world_meta_component, "world_meta"};
static RogueSaveComponent REPLAY_COMP = {ROGUE_SAVE_COMP_REPLAY, write_replay_component,
                                         read_replay_component, "replay"};

void rogue_register_all_components_internal(void)
{
    rogue_save_manager_register(&WORLD_META_COMP);
    rogue_save_manager_register(&INVENTORY_COMP);
    rogue_save_manager_register(&INV_ENTRIES_COMP);
    rogue_save_manager_register(&INV_TAGS_COMP);
    rogue_save_manager_register(&INV_TAG_RULES_COMP);
    rogue_save_manager_register(&INV_SAVED_SEARCHES_COMP);
    rogue_save_manager_register(&PLAYER_COMP);
    rogue_save_manager_register(&SKILLS_COMP);
    rogue_save_manager_register(&BUFFS_COMP);
    rogue_save_manager_register(&VENDOR_COMP);
    rogue_save_manager_register(&STRINGS_COMP);
#if ROGUE_SAVE_FORMAT_VERSION >= 8
    rogue_save_manager_register(&REPLAY_COMP);
#endif
}
