/* schema_levels.c - Phase 2: Level / Map JSON schema implementation
   Shape (single level definition object):
     required: id (1-63), w (1..16384), h (1..16384), tiles (1..1,000,000 chars RLE string)
     optional: tile_size (1..4096), spawns[] (>=0) of { type (1-63), x>=0, y>=0, faction (1-31 opt)
   } environment_tags[] strings (1-63) Notes:
     - We allow additional fields for forward compatibility (e.g., lighting, regions).
     - Cross-field validation (e.g., spawn x< w) is deferred to runtime validators; schema enforces
   basic scalar ranges only.
*/

#include "schema_levels.h"
#include "../util/json_parser.h"
#include <string.h>

bool rogue_levels_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "levels", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description,
            "Schema for level/map definitions (dimensions, tiles RLE, spawns)",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* forward compat */

    /* id */
    RogueSchemaField* f_id = rogue_schema_add_field(out_schema, "id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_id, true);
    rogue_schema_field_set_string_length(f_id, 1, 63);

    /* width / height */
    RogueSchemaField* f_w = rogue_schema_add_field(out_schema, "w", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_w, true);
    rogue_schema_field_set_range(f_w, 1, 16384);
    RogueSchemaField* f_h = rogue_schema_add_field(out_schema, "h", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_h, true);
    rogue_schema_field_set_range(f_h, 1, 16384);

    /* optional tile_size */
    RogueSchemaField* f_ts =
        rogue_schema_add_field(out_schema, "tile_size", ROGUE_SCHEMA_TYPE_INTEGER);
    (void) f_ts; /* not required; range advisory only */
    rogue_schema_field_set_range(f_ts, 1, 4096);

    /* tiles RLE string */
    RogueSchemaField* f_tiles =
        rogue_schema_add_field(out_schema, "tiles", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_tiles, true);
    rogue_schema_field_set_string_length(f_tiles, 1, 1000000);

    /* spawns array (optional) */
    RogueSchemaField* f_spawns =
        rogue_schema_add_field(out_schema, "spawns", ROGUE_SCHEMA_TYPE_ARRAY);
    /* describe spawn item schema */
    static RogueSchema spawn_item_schema; /* static lifetime */
    memset(&spawn_item_schema, 0, sizeof(spawn_item_schema));
    strncpy(spawn_item_schema.name, "spawn", sizeof(spawn_item_schema.name) - 1);
    spawn_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* sp_type =
        rogue_schema_add_field(&spawn_item_schema, "type", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(sp_type, true);
    rogue_schema_field_set_string_length(sp_type, 1, 63);
    RogueSchemaField* sp_x =
        rogue_schema_add_field(&spawn_item_schema, "x", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_x, 0, 1000000);
    RogueSchemaField* sp_y =
        rogue_schema_add_field(&spawn_item_schema, "y", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_y, 0, 1000000);
    RogueSchemaField* sp_faction =
        rogue_schema_add_field(&spawn_item_schema, "faction", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(sp_faction, 1, 31);
    f_spawns->array_item_schema = (RogueSchemaField*) &spawn_item_schema.fields[0];
    f_spawns->nested_schema = &spawn_item_schema;

    /* environment_tags array (optional) */
    RogueSchemaField* f_env =
        rogue_schema_add_field(out_schema, "environment_tags", ROGUE_SCHEMA_TYPE_ARRAY);
    static RogueSchema tag_item_schema;
    memset(&tag_item_schema, 0, sizeof(tag_item_schema));
    strncpy(tag_item_schema.name, "env_tag", sizeof(tag_item_schema.name) - 1);
    tag_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* tag_val =
        rogue_schema_add_field(&tag_item_schema, "value", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(tag_val, 1, 63);
    f_env->array_item_schema = (RogueSchemaField*) &tag_item_schema.fields[0];
    f_env->nested_schema = &tag_item_schema;

    return true;
}

bool rogue_levels_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_levels_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = true;
    return ok;
}
