#include "skill_debug.h"
#include "../app/app_state.h"
#include "skills.h"
#include "skills_coeffs.h"
#include "skills_internal.h"
#include <string.h>

int rogue_skill_debug_count(void) { return g_app.skill_count; }

const char* rogue_skill_debug_name(int id)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    return d && d->name ? d->name : "<noname>";
}

int rogue_skill_debug_get_coeff(int id, RogueSkillCoeffParams* out)
{
    if (!out)
        return -1;
    return rogue_skill_coeff_get_params(id, out);
}

int rogue_skill_debug_set_coeff(int id, const RogueSkillCoeffParams* in)
{
    if (!in)
        return -1;
    return rogue_skill_coeff_register(id, in);
}

int rogue_skill_debug_get_timing(int id, float* base_cooldown_ms, float* cd_red_ms_per_rank,
                                 float* cast_time_ms)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    if (base_cooldown_ms)
        *base_cooldown_ms = d->base_cooldown_ms;
    if (cd_red_ms_per_rank)
        *cd_red_ms_per_rank = d->cooldown_reduction_ms_per_rank;
    if (cast_time_ms)
        *cast_time_ms = d->cast_time_ms;
    return 0;
}

int rogue_skill_debug_set_timing(int id, float base_cooldown_ms, float cd_red_ms_per_rank,
                                 float cast_time_ms)
{
    RogueSkillDef* d = NULL;
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    d = &g_app.skill_defs[id];
    d->base_cooldown_ms = base_cooldown_ms;
    d->cooldown_reduction_ms_per_rank = cd_red_ms_per_rank;
    d->cast_time_ms = cast_time_ms;
    return 0;
}

int rogue_skill_debug_simulate(const char* profile_json, char* out_buf, int out_cap)
{
    return skill_simulate_rotation(profile_json, out_buf, out_cap);
}
