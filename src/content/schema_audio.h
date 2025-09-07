/* schema_audio.h - Phase 2: Audio clip metadata schema */
#ifndef ROGUE_CONTENT_SCHEMA_AUDIO_H
#define ROGUE_CONTENT_SCHEMA_AUDIO_H

#include "../core/integration/json_schema.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool rogue_audio_build_schema(RogueSchema* out_schema);
    bool rogue_audio_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_SCHEMA_AUDIO_H */
