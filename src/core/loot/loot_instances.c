#include "loot_instances.h"
#include "../app/app_state.h"
#include "loot_affixes.h"
#include "loot_logging.h"
#include "loot_rarity_adv.h"
#include "loot_vfx.h"
#include <string.h>
/* Forward declaration (12.4) */
int rogue_minimap_ping_loot(float x, float y, int rarity);

static RogueItemInstance g_instances[ROGUE_ITEM_INSTANCE_CAP];
/* Runtime flag (Phase 18.6): allow tests to suppress INFO spam from loot spawns without
    requiring compile-time macro injection into core sources. */
int g_rogue_loot_suppress_spawn_log = 0;

void rogue_items_init_runtime(void)
{
    memset(g_instances, 0, sizeof g_instances);
    g_app.item_instances = g_instances;
    g_app.item_instance_cap = ROGUE_ITEM_INSTANCE_CAP;
    g_app.item_instance_count = 0;
}
void rogue_items_shutdown_runtime(void)
{
    g_app.item_instances = NULL;
    g_app.item_instance_cap = 0;
    g_app.item_instance_count = 0;
}

void rogue_items_sync_app_view(void)
{
    /* Wire global app view to the internal static pool without mutating item state. */
    g_app.item_instances = g_instances;
    g_app.item_instance_cap = ROGUE_ITEM_INSTANCE_CAP;
    int c = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        if (g_instances[i].active)
            c++;
    }
    g_app.item_instance_count = c;
    fprintf(stderr, "DBG: rogue_items_sync_app_view ptr=%p cap=%d active=%d\n",
            (void*) g_app.item_instances, g_app.item_instance_cap, g_app.item_instance_count);
}

int rogue_items_spawn(int def_index, int quantity, float x, float y)
{
    if (def_index < 0 || quantity <= 0)
    {
        ROGUE_LOOT_LOG_DEBUG("loot_spawn: rejected def=%d qty=%d", def_index, quantity);
        return -1;
    }
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
        if (!g_instances[i].active)
        {
            const RogueItemDef* idef = rogue_item_def_at(def_index);
            int rarity = (idef ? idef->rarity : 0);
            g_instances[i].def_index = def_index;
            g_instances[i].quantity = quantity;
            g_instances[i].x = x;
            g_instances[i].y = y;
            g_instances[i].life_ms = 0;
            g_instances[i].active = 1;
            g_instances[i].rarity = rarity;
            g_instances[i].item_level = 1; /* baseline */
            g_instances[i].prefix_index = -1;
            g_instances[i].suffix_index = -1;
            g_instances[i].prefix_value = 0;
            g_instances[i].suffix_value = 0;
            g_instances[i].hidden_filter = 0;
            g_instances[i].fractured = 0;
            g_instances[i].quality = 0;
            g_instances[i].stored_affix_index = -1;
            g_instances[i].stored_affix_value = 0;
            g_instances[i].stored_affix_used = 0;
            g_instances[i].guid = ((unsigned long long) def_index << 32) ^
                                  (unsigned long long) ((i + 1) * 0x9E3779B185EBCA87ULL) ^
                                  (unsigned long long) (quantity * 0xC2B2AE3D27D4EB4FULL);
            g_instances[i].equip_hash_chain = 0ULL;
            /* Initialize sockets (Phase 5.1). Random count inside min..max if range >0 using local
             * deterministic LCG seeded from position & def_index. */
            g_instances[i].socket_count = 0;
            for (int s = 0; s < 6; s++)
                g_instances[i].sockets[s] = -1;
            if (idef)
            {
                int min = idef->socket_min, max = idef->socket_max;
                if (max > 6)
                    max = 6;
                if (min < 0)
                    min = 0;
                if (max >= min && max > 0)
                {
                    unsigned int seed =
                        (unsigned int) (i * 2654435761u) ^ (unsigned int) def_index ^
                        (unsigned int) ((int) x * 73856093) ^ (unsigned int) ((int) y * 19349663);
                    seed = seed * 1664525u + 1013904223u;
                    int span = (max - min) + 1;
                    int roll = (span > 0) ? (int) (seed % (unsigned int) span) : 0;
                    g_instances[i].socket_count = min + roll;
                    if (g_instances[i].socket_count > 6)
                        g_instances[i].socket_count = 6;
                }
            }
            /* Set durability baseline: weapons & armor categories get base derived from level &
             * rarity. */
            if (idef && (idef->category == ROGUE_ITEM_WEAPON || idef->category == ROGUE_ITEM_ARMOR))
            {
                int base_dur = 50 + rarity * 25;
                g_instances[i].durability_max = base_dur;
                g_instances[i].durability_cur = base_dur;
            }
            else
            {
                g_instances[i].durability_max = 0;
                g_instances[i].durability_cur = 0;
            }
            if (i >= g_app.item_instance_count)
                g_app.item_instance_count = i + 1;
            /* 12.4 spawn minimap loot ping */
            rogue_minimap_ping_loot(x, y, rarity);
            rogue_loot_vfx_on_spawn(i, rarity);
            if (!g_rogue_loot_suppress_spawn_log)
            {
                ROGUE_LOOT_LOG_INFO(
                    "loot_spawn: def=%d qty=%d at(%.2f,%.2f) slot=%d active_total=%d", def_index,
                    quantity, x, y, i, rogue_items_active_count() + 1);
            }
            return i;
        }
    ROGUE_LOG_WARN("loot_spawn: pool full (cap=%d) def=%d qty=%d", ROGUE_ITEM_INSTANCE_CAP,
                   def_index, quantity);
    return -1;
}

const RogueItemInstance* rogue_item_instance_at(int index)
{
    if (index < 0 || index >= ROGUE_ITEM_INSTANCE_CAP)
        return NULL;
    if (!g_instances[index].active)
        return NULL;
    return &g_instances[index];
}

unsigned long long rogue_item_instance_guid(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    return it ? it->guid : 0ULL;
}
unsigned long long rogue_item_instance_equip_chain(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    return it ? it->equip_hash_chain : 0ULL;
}

int rogue_item_instance_generate_affixes(int inst_index, unsigned int* rng_state, int rarity)
{
    if (inst_index < 0 || inst_index >= ROGUE_ITEM_INSTANCE_CAP)
        return -1;
    if (!rng_state)
        return -1;
    RogueItemInstance* it = &g_instances[inst_index];
    if (!it->active)
        return -1;
    /* Simple rule: rarity >=2 gives 1 prefix OR suffix (50/50), rarity >=3 gives both if possible
     */
    int want_prefix = 0, want_suffix = 0;
    if (rarity >= 2)
    {
        if (rarity >= 3)
        {
            want_prefix = 1;
            want_suffix = 1;
        }
        else
        {
            want_prefix = ((*rng_state) & 1) == 0;
            want_suffix = !want_prefix;
        }
    }
    if (want_prefix)
    {
        int pi = rogue_affix_roll(ROGUE_AFFIX_PREFIX, rarity, rng_state);
        if (pi >= 0)
        {
            it->prefix_index = pi;
            it->prefix_value = rogue_affix_roll_value(pi, rng_state);
        }
    }
    if (want_suffix)
    {
        int si = rogue_affix_roll(ROGUE_AFFIX_SUFFIX, rarity, rng_state);
        if (si >= 0)
        {
            it->suffix_index = si;
            it->suffix_value = rogue_affix_roll_value(si, rng_state);
        }
    }
    /* Phase 3.3: clamp any over-budget rolls immediately */
    int cap = rogue_budget_max(it->item_level, it->rarity);
    int total = 0;
    if (it->prefix_index >= 0)
    {
        total += it->prefix_value;
    }
    if (it->suffix_index >= 0)
    {
        total += it->suffix_value;
    }
    if (total > cap)
    {
        /* Reduce larger affix first until within cap */
        while (total > cap)
        {
            int reduce_prefix = 0;
            if (it->prefix_index >= 0 && it->suffix_index >= 0)
            {
                reduce_prefix = (it->prefix_value >= it->suffix_value);
            }
            else if (it->prefix_index >= 0)
            {
                reduce_prefix = 1;
            }
            else
                reduce_prefix = 0;
            if (reduce_prefix && it->prefix_index >= 0 && it->prefix_value > 0)
            {
                it->prefix_value--;
                total--;
            }
            else if (it->suffix_index >= 0 && it->suffix_value > 0)
            {
                it->suffix_value--;
                total--;
            }
            else
                break; /* cannot reduce further */
        }
    }
    return 0;
}

static int affix_damage_bonus(const RogueItemInstance* it)
{
    int bonus = 0;
    if (!it)
        return 0;
    if (it->prefix_index >= 0)
    {
        const RogueAffixDef* a = rogue_affix_at(it->prefix_index);
        if (a && a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
            bonus += it->prefix_value;
    }
    if (it->suffix_index >= 0)
    {
        const RogueAffixDef* a = rogue_affix_at(it->suffix_index);
        if (a && a->stat == ROGUE_AFFIX_STAT_DAMAGE_FLAT)
            bonus += it->suffix_value;
    }
    return bonus;
}

static int apply_quality_scale(int base, int quality)
{
    if (quality <= 0)
        return base;
    if (quality > 20)
        quality = 20; /* linear 0..20 -> up to +10% */
    /* Compute additive delta with ceil and tiny epsilon to avoid equality at exact boundaries */
    float delta_f = (float) base * (quality * 0.006f) + 1e-6f;
    int delta = (int) ceilf(delta_f);
    if (delta < 0)
        delta = 0;
    return base + delta;
}
int rogue_item_instance_damage_min(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0;
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    int base = d ? d->base_damage_min : 0;
    base = apply_quality_scale(base, it->quality);
    int val = base + affix_damage_bonus(it);
    if (it->fractured)
        val = (int) (val * 0.6f);
    return val;
}
int rogue_item_instance_damage_max(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0;
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    int base = d ? d->base_damage_max : 0;
    base = apply_quality_scale(base, it->quality);
    int val = base + affix_damage_bonus(it);
    if (it->fractured)
        val = (int) (val * 0.6f);
    return val;
}

int rogue_item_instance_apply_affixes(int inst_index, int rarity, int prefix_index,
                                      int prefix_value, int suffix_index, int suffix_value)
{
    if (inst_index < 0 || inst_index >= ROGUE_ITEM_INSTANCE_CAP)
        return -1;
    RogueItemInstance* it = &g_instances[inst_index];
    if (!it->active)
        return -1;
    it->rarity = (rarity >= 0 && rarity <= 4) ? rarity : it->rarity;
    it->prefix_index = prefix_index;
    it->prefix_value = prefix_value;
    it->suffix_index = suffix_index;
    it->suffix_value = suffix_value;
    return 0;
}

/* -------- Phase 3 Budget Governance Implementation (minimal) -------- */
int rogue_budget_max(int item_level, int rarity)
{
    if (item_level < 1)
        item_level = 1;
    if (rarity < 0)
        rarity = 0;
    if (rarity > 4)
        rarity = 4; /* simple scaling: base 20 + item_level*5 + rarity^2 * 10 */
    return 20 + item_level * 5 + (rarity * rarity) * 10;
}

int rogue_item_instance_total_affix_weight(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int total = 0;
    if (it->prefix_index >= 0)
    {
        total += it->prefix_value;
    }
    if (it->suffix_index >= 0)
    {
        total += it->suffix_value;
    }
    return total;
}

int rogue_item_instance_validate_budget(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int cap = rogue_budget_max(it->item_level, it->rarity);
    int total = rogue_item_instance_total_affix_weight(inst_index);
    if (total < 0)
        return -2;
    return (total <= cap) ? 0 : -3;
}

int rogue_item_instance_upgrade_level(int inst_index, int levels, unsigned int* rng_state)
{
    if (levels <= 0)
        return 0;
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    it->item_level += levels;
    if (it->item_level > 999)
        it->item_level =
            999; /* attempt gentle elevation: +1 to each affix value if under new cap */
    int cap = rogue_budget_max(it->item_level, it->rarity);
    int total = rogue_item_instance_total_affix_weight(inst_index);
    if (total < 0)
        return -2;
    while (total < cap && (it->prefix_index >= 0 || it->suffix_index >= 0))
    {
        if (rng_state)
        {
            *rng_state = (*rng_state * 1664525u) + 1013904223u;
        }
        int choose_prefix = (it->prefix_index >= 0 && it->suffix_index >= 0)
                                ? ((*rng_state) & 1)
                                : (it->suffix_index < 0);
        if (choose_prefix && it->prefix_index >= 0 && it->prefix_value < cap)
        {
            it->prefix_value++;
            total++;
        }
        else if (it->suffix_index >= 0 && it->suffix_value < cap)
        {
            it->suffix_value++;
            total++;
        }
        else
            break;
    }
    return 0;
}

int rogue_item_instance_get_durability(int inst_index, int* cur, int* max)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (cur)
        *cur = it->durability_cur;
    if (max)
        *max = it->durability_max;
    return 0;
}
int rogue_item_instance_damage_durability(int inst_index, int amount)
{
    if (amount <= 0)
        return 0;
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    if (it->durability_max <= 0)
        return it->durability_cur;
    it->durability_cur -= amount;
    if (it->durability_cur < 0)
        it->durability_cur = 0;
    if (it->durability_cur == 0)
        it->fractured = 1;
    return it->durability_cur;
}
int rogue_item_instance_repair_full(int inst_index)
{
    const RogueItemInstance* itc = rogue_item_instance_at(inst_index);
    if (!itc)
        return -1;
    RogueItemInstance* it = (RogueItemInstance*) itc;
    if (it->durability_max <= 0)
        return 0;
    it->durability_cur = it->durability_max;
    it->fractured = 0;
    return it->durability_cur;
}

int rogue_item_instance_get_quality(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    return it->quality;
}
int rogue_item_instance_set_quality(int inst_index, int quality)
{
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (quality < 0)
        quality = 0;
    if (quality > 20)
        quality = 20;
    it->quality = quality;
    return it->quality;
}
int rogue_item_instance_improve_quality(int inst_index, int delta)
{
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int q = it->quality + delta;
    if (q < 0)
        q = 0;
    if (q > 20)
        q = 20;
    it->quality = q;
    return it->quality;
}

/* ---- Phase 10.1 Upgrade Stone Implementation ---- */
int rogue_item_instance_apply_upgrade_stone(int inst_index, int tiers, unsigned int* rng_state)
{
    if (tiers <= 0)
        return 0;
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    int before_level = it->item_level;
    int rc = rogue_item_instance_upgrade_level(inst_index, tiers, rng_state);
    if (rc < 0)
        return rc; /* reuse existing upgrade logic */
    (void) before_level;
    return 0;
}

/* ---- Phase 10.2 Affix Transfer (Extraction to Orb) ---- */
static RogueItemInstance* item_mut(int idx)
{
    return (RogueItemInstance*) rogue_item_instance_at(idx);
}
int rogue_item_instance_affix_extract(int inst_index, int is_prefix, int orb_inst_index)
{
    RogueItemInstance* src = item_mut(inst_index);
    RogueItemInstance* orb = item_mut(orb_inst_index);
    if (!src || !orb)
        return -1;
    if (orb->stored_affix_index >= 0)
        return -2;
    if (orb->active == 0)
        return -3;
    if (orb_inst_index == inst_index)
        return -4;
    int* a_index = is_prefix ? &src->prefix_index : &src->suffix_index;
    int* a_value = is_prefix ? &src->prefix_value : &src->suffix_value;
    if (*a_index < 0)
        return -5; /* nothing to extract */
    orb->stored_affix_index = *a_index;
    orb->stored_affix_value = *a_value;
    orb->stored_affix_used = 0;
    *a_index = -1;
    *a_value = 0; /* clear from source */
    return 0;
}

/* Apply orb to target: picks prefix if affix is prefix, else suffix based on affix definition
 * category. */
int rogue_item_instance_affix_orb_apply(int orb_inst_index, int target_inst_index)
{
    RogueItemInstance* orb = item_mut(orb_inst_index);
    RogueItemInstance* tgt = item_mut(target_inst_index);
    if (!orb || !tgt)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: invalid instances orb=%d tgt=%d", orb_inst_index,
                             target_inst_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-1 (invalid instances) orb=%d tgt=%d\n",
                orb_inst_index, target_inst_index);
        return -1;
    }
    if (orb->stored_affix_index < 0)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: no stored affix on orb=%d", orb_inst_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-2 (no stored affix) orb=%d\n", orb_inst_index);
        return -2;
    }
    if (orb->stored_affix_used)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: orb already used orb=%d", orb_inst_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-3 (orb used) orb=%d\n", orb_inst_index);
        return -3;
    }
    if (orb_inst_index == target_inst_index)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: same orb and target index=%d", target_inst_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-4 (same index) idx=%d\n", target_inst_index);
        return -4;
    }
    const RogueAffixDef* a = rogue_affix_at(orb->stored_affix_index);
    if (!a)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: affix def not found idx=%d",
                             orb->stored_affix_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-5 (affix not found) idx=%d\n",
                orb->stored_affix_index);
        return -5;
    }
    int is_prefix = (a->type == ROGUE_AFFIX_PREFIX);
    int* slot_index = is_prefix ? &tgt->prefix_index : &tgt->suffix_index;
    int* slot_value = is_prefix ? &tgt->prefix_value : &tgt->suffix_value;
    int* alt_index = is_prefix ? &tgt->suffix_index : &tgt->prefix_index;
    int* alt_value = is_prefix ? &tgt->suffix_value : &tgt->prefix_value;
    /* Validate budget after applying */
    int cap = rogue_budget_max(tgt->item_level, tgt->rarity);
    int current = rogue_item_instance_total_affix_weight(target_inst_index);
    if (current < 0)
    {
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: failed to get current weight for tgt=%d",
                             target_inst_index);
        fprintf(stderr, "DBG affix_orb_apply rc=-7 (weight err) tgt=%d\n", target_inst_index);
        return -7;
    }
    if (*slot_index >= 0)
    {
        /* Try alternate empty slot if primary is occupied. Type mismatch is tolerated for this
           phase's orb behavior focused on value transfer. */
        if (*alt_index >= 0)
        {
            ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: both slots occupied on tgt=%d (pref=%d,suf=%d)",
                                 target_inst_index, tgt->prefix_index, tgt->suffix_index);
            fprintf(stderr, "DBG affix_orb_apply rc=-6 (both occupied) tgt=%d pref=%d suf=%d\n",
                    target_inst_index, tgt->prefix_index, tgt->suffix_index);
            return -6; /* both occupied */
        }
        int allowed = cap - current;
        if (allowed <= 0)
        {
            ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: no budget headroom on tgt=%d (cur=%d cap=%d)",
                                 target_inst_index, current, cap);
            fprintf(stderr, "DBG affix_orb_apply rc=-8 (no headroom ALT) tgt=%d cur=%d cap=%d\n",
                    target_inst_index, current, cap);
            return -8; /* no room at all */
        }
        int applied_val = orb->stored_affix_value > allowed ? allowed : orb->stored_affix_value;
        *alt_index = orb->stored_affix_index;
        *alt_value = applied_val;
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: applied to ALT slot on tgt=%d idx=%d val=%d "
                             "(clamped_from=%d) cur=%d cap=%d",
                             target_inst_index, orb->stored_affix_index, applied_val,
                             orb->stored_affix_value, current, cap);
    }
    else
    {
        int allowed = cap - current;
        if (allowed <= 0)
        {
            ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: no budget headroom on tgt=%d (cur=%d cap=%d)",
                                 target_inst_index, current, cap);
            fprintf(stderr,
                    "DBG affix_orb_apply rc=-8 (no headroom PRIMARY) tgt=%d cur=%d cap=%d\n",
                    target_inst_index, current, cap);
            return -8; /* no room at all */
        }
        int applied_val = orb->stored_affix_value > allowed ? allowed : orb->stored_affix_value;
        *slot_index = orb->stored_affix_index;
        *slot_value = applied_val;
        ROGUE_LOOT_LOG_DEBUG("affix_orb_apply: applied to PRIMARY slot on tgt=%d idx=%d val=%d "
                             "(clamped_from=%d) cur=%d cap=%d",
                             target_inst_index, orb->stored_affix_index, applied_val,
                             orb->stored_affix_value, current, cap);
    }
    orb->stored_affix_used = 1; /* one-time use */
    return 0;
}

/* ---- Phase 10.3 Fusion Implementation ---- */
static int highest_affix(const RogueItemInstance* it, int* is_prefix_out, int* index_out,
                         int* value_out)
{
    if (!it)
        return -1;
    int have = -1;
    int idx = -1;
    int val = 0;
    int pref = 0;
    if (it->prefix_index >= 0)
    {
        have = 1;
        idx = it->prefix_index;
        val = it->prefix_value;
        pref = 1;
    }
    if (it->suffix_index >= 0)
    {
        if (have < 0 || it->suffix_value > val)
        {
            have = 1;
            idx = it->suffix_index;
            val = it->suffix_value;
            pref = 0;
        }
    }
    if (have < 0)
        return -1;
    if (is_prefix_out)
        *is_prefix_out = pref;
    if (index_out)
        *index_out = idx;
    if (value_out)
        *value_out = val;
    return 0;
}

int rogue_item_instance_fusion(int target_inst_index, int sacrifice_inst_index)
{
    if (target_inst_index == sacrifice_inst_index)
        return -10;
    RogueItemInstance* tgt = item_mut(target_inst_index);
    RogueItemInstance* sac = item_mut(sacrifice_inst_index);
    if (!tgt || !sac)
        return -1;
    if (!sac->active)
        return -2;
    /* Build candidate set from donor's prefix/suffix if present */
    int cand_idx[2] = {-1, -1};
    int cand_val[2] = {0, 0};
    int cand_is_pref[2] = {0, 0};
    int cand_count = 0;
    if (sac->prefix_index >= 0)
    {
        cand_idx[cand_count] = sac->prefix_index;
        cand_val[cand_count] = sac->prefix_value;
        cand_is_pref[cand_count] = 1;
        cand_count++;
    }
    if (sac->suffix_index >= 0)
    {
        cand_idx[cand_count] = sac->suffix_index;
        cand_val[cand_count] = sac->suffix_value;
        cand_is_pref[cand_count] = 0;
        cand_count++;
    }
    if (cand_count == 0)
        return -3; /* nothing to transfer */

    int cap = rogue_budget_max(tgt->item_level, tgt->rarity);
    int cur = rogue_item_instance_total_affix_weight(target_inst_index);
    if (cur < 0)
        return -4;
    int allowed = cap - cur;
    if (allowed <= 0)
        return -5; /* no headroom */

    /* Evaluate which candidates can be placed (slot vacant) and pick highest value */
    int best = -1;
    for (int i = 0; i < cand_count; i++)
    {
        int* slot_index = cand_is_pref[i] ? &tgt->prefix_index : &tgt->suffix_index;
        if (*slot_index >= 0)
            continue; /* occupied */
        if (best < 0 || cand_val[i] > cand_val[best])
            best = i;
    }
    if (best < 0)
        return -6; /* both occupied */

    int applied_val = cand_val[best] > allowed ? allowed : cand_val[best];
    if (applied_val <= 0)
        return -5; /* effectively no room */

    int* dst_index = cand_is_pref[best] ? &tgt->prefix_index : &tgt->suffix_index;
    int* dst_value = cand_is_pref[best] ? &tgt->prefix_value : &tgt->suffix_value;
    *dst_index = cand_idx[best];
    *dst_value = applied_val;
    sac->active = 0; /* sacrifice consumed */
    ROGUE_LOOT_LOG_DEBUG(
        "fusion: applied %s idx=%d val=%d (clamped_from=%d) to tgt=%d cur=%d cap=%d",
        cand_is_pref[best] ? "PREFIX" : "SUFFIX", cand_idx[best], applied_val, cand_val[best],
        target_inst_index, cur, cap);
    return 0;
}

/* ---- Phase 5.1 Socket API implementation ---- */
int rogue_item_instance_socket_count(int inst_index)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    return it->socket_count;
}
int rogue_item_instance_get_socket(int inst_index, int slot)
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (slot < 0 || slot >= it->socket_count || slot >= 6)
        return -2;
    return it->sockets[slot];
}
int rogue_item_instance_socket_insert(int inst_index, int slot, int gem_def_index)
{
    if (gem_def_index < 0)
        return -5;
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (slot < 0 || slot >= it->socket_count || slot >= 6)
        return -2;
    if (it->sockets[slot] >= 0)
        return -3;
    it->sockets[slot] = gem_def_index;
    return 0;
}
int rogue_item_instance_socket_remove(int inst_index, int slot)
{
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst_index);
    if (!it)
        return -1;
    if (slot < 0 || slot >= it->socket_count || slot >= 6)
        return -2;
    if (it->sockets[slot] < 0)
        return -3;
    it->sockets[slot] = -1;
    return 0;
}

int rogue_items_active_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        if (g_instances[i].active)
            c++;
    }
    return c;
}
int rogue_items_visible_count(void)
{
    int c = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        if (g_instances[i].active && !g_instances[i].hidden_filter)
            c++;
    }
    return c;
}
/* Forward declare to avoid heavy include; loot_filter.c provides implementation */
int rogue_loot_filter_match(const RogueItemDef* def);
void rogue_items_reapply_filter(void)
{
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        if (!g_instances[i].active)
            continue;
        const RogueItemDef* d = rogue_item_def_at(g_instances[i].def_index);
        g_instances[i].hidden_filter = (rogue_loot_filter_match(d) == 0) ? 1 : 0;
    }
}
void rogue_items_update(float dt_ms)
{
    /* Advance lifetime & mark for despawn */
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
        if (g_instances[i].active)
        {
            g_instances[i].life_ms += dt_ms;
            int override_ms =
                rogue_rarity_get_despawn_ms(g_instances[i].rarity); /* advanced rarity override */
            int limit = override_ms > 0 ? override_ms : ROGUE_ITEM_DESPAWN_MS;
            if (g_instances[i].life_ms >= (float) limit)
            {
                g_instances[i].active = 0;
                rogue_loot_vfx_on_despawn(i);
                continue;
            }
        }
    /* Stack merge pass (single sweep O(n^2) small cap) */
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
        if (g_instances[i].active)
        {
            for (int j = i + 1; j < ROGUE_ITEM_INSTANCE_CAP; j++)
                if (g_instances[j].active)
                {
                    if (g_instances[i].def_index == g_instances[j].def_index &&
                        g_instances[i].rarity == g_instances[j].rarity)
                    {
                        float dx = g_instances[i].x - g_instances[j].x;
                        float dy = g_instances[i].y - g_instances[j].y;
                        if (dx * dx + dy * dy <=
                            ROGUE_ITEM_STACK_MERGE_RADIUS * ROGUE_ITEM_STACK_MERGE_RADIUS)
                        {
                            /* Merge j into i respecting stack_max */
                            const RogueItemDef* d = rogue_item_def_at(g_instances[i].def_index);
                            int stack_max = d ? d->stack_max : 999999;
                            int space = stack_max - g_instances[i].quantity;
                            if (space > 0)
                            {
                                int move = g_instances[j].quantity < space ? g_instances[j].quantity
                                                                           : space;
                                g_instances[i].quantity += move;
                                g_instances[j].quantity -= move;
                                if (g_instances[j].quantity <= 0)
                                {
                                    g_instances[j].active = 0;
                                }
                            }
                        }
                    }
                }
        }
    /* Update VFX after lifetime & potential merges */
    rogue_loot_vfx_update(dt_ms);
}
