#ifndef ROGUE_CORE_SKILLS_VALIDATE_H
#define ROGUE_CORE_SKILLS_VALIDATE_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Validate skills/procs/effects cross-references.
       - Invalid references: skill.effect_spec_id must exist when >=0; proc.effect_spec_id must
       exist.
       - Cyclic procs: detect simple cycles proc(event)->effect->event that feeds same proc set.
       - Missing coefficients: warn/error if a skill has rankable or offensive metadata but no
       coeffs. Returns 0 on success, -1 on validation failure. Populates err when provided. */
    int rogue_skills_validate_all(char* err, int err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILLS_VALIDATE_H */
