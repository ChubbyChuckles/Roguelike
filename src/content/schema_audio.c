/* schema_audio.c - Phase 2: Audio clip metadata schema */
#include "schema_audio.h"
#include "../util/json_parser.h"
#include <stdio.h>
#include <string.h>

/* Required: name (1-63), file (1-255)
   Optional: format (enum-ish string), volume (0..1 as number), loop (bool), loop_start_ms>=0,
   loop_end_ms>=0, tags[] string */
bool rogue_audio_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "audio", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for audio clip metadata",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true;

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 63);
    /* Optional schema_version for future migrations (1..1024) */
    RogueSchemaField* f_schema_version =
        rogue_schema_add_field(out_schema, "schema_version", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_schema_version)
        rogue_schema_field_set_range(f_schema_version, 1, 1024);
    RogueSchemaField* f_file = rogue_schema_add_field(out_schema, "file", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_file, true);
    rogue_schema_field_set_string_length(f_file, 1, 255);
    rogue_schema_add_field(out_schema, "format", ROGUE_SCHEMA_TYPE_STRING); /* advisory only */
    RogueSchemaField* f_volume =
        rogue_schema_add_field(out_schema, "volume", ROGUE_SCHEMA_TYPE_NUMBER);
    /* approximate 0..1 range using integer/number constraints not yet specialized; leave as
     * advisory */
    RogueSchemaField* f_loop =
        rogue_schema_add_field(out_schema, "loop", ROGUE_SCHEMA_TYPE_BOOLEAN);
    (void) f_loop; /* silence unused if build toggles */
    RogueSchemaField* f_loop_start =
        rogue_schema_add_field(out_schema, "loop_start_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    (void) f_loop_start;
    RogueSchemaField* f_loop_end =
        rogue_schema_add_field(out_schema, "loop_end_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    (void) f_loop_end;
    RogueSchemaField* f_tags = rogue_schema_add_field(out_schema, "tags", ROGUE_SCHEMA_TYPE_ARRAY);
    /* array of strings (optional, no min) */
    static RogueSchema tag_item_schema;
    memset(&tag_item_schema, 0, sizeof(tag_item_schema));
    strncpy(tag_item_schema.name, "tag", sizeof(tag_item_schema.name) - 1);
    tag_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* tag_val =
        rogue_schema_add_field(&tag_item_schema, "value", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(tag_val, 1, 63);
    f_tags->array_item_schema = (RogueSchemaField*) &tag_item_schema.fields[0];
    f_tags->nested_schema = &tag_item_schema;
    return true;
}

bool rogue_audio_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_audio_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool schema_exec = rogue_schema_validate_json(&schema, json, result);
    if (schema_exec && result->error_count == 0)
    {
        result->is_valid = true;
    }
    return schema_exec;
}
