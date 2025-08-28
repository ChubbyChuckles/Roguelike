/* skills_api_doc.c - Skillsystem Phase 10.4 (Auto-doc generator for skill sheets)
 * Emits a compact, curated description of skill sheet columns/fields and
 * coefficients JSON schema used by the loader. Intentional minimalism avoids
 * heavy reflection while providing an always up-to-date quick reference.
 */
#include "skills_api_doc.h"
#include <stdio.h>
#include <string.h>

typedef struct Entry
{
    const char* name;
    const char* desc;
} Entry;

/* Keep ordering stable. */
static const Entry k_sections[] = {
    {"SKILL_SHEET_COLUMNS",
     "CSV columns (order): name, icon, max_rank, base_cooldown_ms, cooldown_reduction_ms_per_rank, "
     "is_passive, tags(bitfield), synergy_id, synergy_value_per_rank, resource_cost_mana, "
     "action_point_cost, max_charges, charge_recharge_ms, cast_time_ms, input_buffer_ms, "
     "min_weave_ms, early_cancel_min_pct, cast_type(0 instant,1 cast,2 channel), "
     "combo_builder(0/1), combo_spender(0/1), effect_spec_id (>=0 = link to EffectSpec)."},
    {"SKILL_FLAGS_AND_TAGS",
     "Tags bitfield: FIRE(1<<0), FROST(1<<1), ARCANE(1<<2), MOVEMENT(1<<3), DEFENSE(1<<4), "
     "SUPPORT(1<<5), CONTROL(1<<6). Haste flags on def.haste_mode_flags: bit0=snapshot cast, "
     "bit1=snapshot channel."},
    {"COST_MAPPING_EXTENSIONS",
     "Optional fields (data-driven via JSON only in v10): ap_cost_pct_max(0..100), "
     "ap_cost_per_rank, "
     "ap_cost_surcharge_amount, ap_cost_surcharge_threshold, mana_cost_pct_max, "
     "mana_cost_per_rank, "
     "mana_cost_surcharge_amount, mana_cost_surcharge_threshold, refund_on_miss_pct, "
     "refund_on_resist_pct, refund_on_cancel_pct."},
    {"COEFFS_JSON_FIELDS",
     "Array of objects: {skill_id:int, base_scalar:float (default 1.0), per_rank_scalar:float, "
     "str_pct_per10:float, int_pct_per10:float, dex_pct_per10:float, stat_cap_pct:float, "
     "stat_softness:float}. Loaded via skills_coeffs_load."},
    {"EFFECTSPEC_JSON_REFERENCE",
     "EffectSpec JSON fields: kind, debuff, buff_type, magnitude, duration_ms, stack_rule, "
     "snapshot, "
     "scale_by_buff_type, scale_pct_per_point, snapshot_scale, require_buff_type, "
     "require_buff_min, "
     "pulse_period_ms, damage_type, crit_mode, crit_chance_pct, aura_radius, aura_group_mask. "
     "Link skills by setting RogueSkillDef.effect_spec_id to a registered EffectSpec id."},
    {"VALIDATION_TOOLING",
     "rogue_skills_validate_all(err,cap): checks invalid EffectSpec refs (skills & procs), "
     "duplicate (event_type,effect_spec_id) proc pairs, and missing coefficients for offensive "
     "skills."}};

int rogue_skills_generate_api_doc(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    if (cap < 128)
    {
        buf[0] = '\0';
        return -1;
    }
    int w = 0;
    w += snprintf(buf + w, cap - w, "SKILLS DOC (Phase 10.4)\n");
    for (size_t i = 0; i < sizeof(k_sections) / sizeof(k_sections[0]); ++i)
    {
        const Entry* e = &k_sections[i];
        int r = snprintf(buf + w, cap - w, "%s: %s\n", e->name, e->desc);
        if (r < 0)
            break;
        if (r >= cap - w)
        {
            w = cap - 1;
            break;
        }
        w += r;
    }
    if (w >= cap)
        w = cap - 1;
    buf[w] = '\0';
    return w;
}
