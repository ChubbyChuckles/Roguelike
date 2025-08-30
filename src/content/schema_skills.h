#ifndef ROGUE_CONTENT_SCHEMA_SKILLS_H
#define ROGUE_CONTENT_SCHEMA_SKILLS_H

#include "../core/integration/json_schema.h"
#include "../core/skills/skills.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Build the schema that describes a single skill definition object. */
    bool rogue_skills_build_schema(RogueSchema* out_schema);

    /* Validate a single JSON object against the skills schema. */
    bool rogue_skills_validate_skill_json(const RogueJsonValue* json,
                                          RogueSchemaValidationResult* result);

    /* Convenience: validate an array of RogueSkillDef entries by converting them to JSON
       objects and validating each against the schema. Returns true only if all pass. */
    bool rogue_skills_validate_defs(const struct RogueSkillDef* defs, int count,
                                    RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_SKILLS_H */
