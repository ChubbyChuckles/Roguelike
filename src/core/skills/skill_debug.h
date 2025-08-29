#ifndef ROGUE_CORE_SKILL_DEBUG_H
#define ROGUE_CORE_SKILL_DEBUG_H

#include "skills_coeffs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Lightweight debug/inspection APIs for skills; safe in headless unit tests. */

    /* Return number of registered skills. */
    int rogue_skill_debug_count(void);

    /* Get display name by id; returns non-NULL string or "<noname>". */
    const char* rogue_skill_debug_name(int id);

    /* Get read-write coeff params for a skill; returns 0 on success. */
    int rogue_skill_debug_get_coeff(int id, RogueSkillCoeffParams* out);

    /* Overwrite coeff params for a skill (live update). Returns 0 on success. */
    int rogue_skill_debug_set_coeff(int id, const RogueSkillCoeffParams* in);

    /* Get/edit core timing properties on the definition; returns 0 on success. */
    int rogue_skill_debug_get_timing(int id, float* base_cooldown_ms, float* cd_red_ms_per_rank,
                                     float* cast_time_ms);
    int rogue_skill_debug_set_timing(int id, float base_cooldown_ms, float cd_red_ms_per_rank,
                                     float cast_time_ms);

    /* Simple wrapper to run rotation simulation and write result JSON. */
    int rogue_skill_debug_simulate(const char* profile_json, char* out_buf, int out_cap);

    /* Export all skills' timing + coeff overrides to a compact JSON array into out_buf.
        Returns number of bytes written (>=0) or <0 on overflow/error. */
    int rogue_skill_debug_export_overrides_json(char* out_buf, int out_cap);

    /* Parse overrides JSON text (same schema as export) and apply live to registry.
        Returns number of entries applied or <0 on parse error. */
    int rogue_skill_debug_load_overrides_text(const char* json_text);

    /* Save overrides to file atomically using content/json_io. Returns 0 on success. */
    int rogue_skill_debug_save_overrides(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILL_DEBUG_H */
