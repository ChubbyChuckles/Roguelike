#include "schema_entities.h"
#include "../util/json_parser.h"
#include "../util/log.h"
#include "json_envelope.h"
#include "json_io.h"
#include <string.h>

/* Helper to create a JSON object representing one RogueEnemyTypeDef for schema validation */
static RogueJsonValue* enemy_typedef_to_json(const RogueEnemyTypeDef* t)
{
    RogueJsonValue* obj = json_create_object();
    if (!obj)
        return NULL;

    json_object_set(obj, "id", json_create_string(t->id));
    json_object_set(obj, "name", json_create_string(t->name));
    json_object_set(obj, "group_min", json_create_integer(t->group_min));
    json_object_set(obj, "group_max", json_create_integer(t->group_max));
    json_object_set(obj, "patrol_radius", json_create_integer(t->patrol_radius));
    json_object_set(obj, "aggro_radius", json_create_integer(t->aggro_radius));
    json_object_set(obj, "speed", json_create_number((double) t->speed));
    json_object_set(obj, "pop_target", json_create_integer(t->pop_target));
    json_object_set(obj, "xp_reward", json_create_integer(t->xp_reward));
    json_object_set(obj, "loot_chance", json_create_number((double) t->loot_chance));
    json_object_set(obj, "base_level_offset", json_create_integer(t->base_level_offset));
    json_object_set(obj, "tier_id", json_create_integer(t->tier_id));
    json_object_set(obj, "archetype_id", json_create_integer(t->archetype_id));
    /* Texture paths are optional in headless tests, represented as strings if present */
    /* We don't include sprite arrays in schema; paths suffice. */
    return obj;
}

bool rogue_entities_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "entities", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for enemy/entity type definitions",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* allow extra authoring fields for now */

    RogueSchemaField* f_id = rogue_schema_add_field(out_schema, "id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_id, true);
    rogue_schema_field_set_string_length(f_id, 1, 31);

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 31);

    RogueSchemaField* f_group_min =
        rogue_schema_add_field(out_schema, "group_min", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_group_min, 1, 100);

    RogueSchemaField* f_group_max =
        rogue_schema_add_field(out_schema, "group_max", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_group_max, 1, 100);

    RogueSchemaField* f_patrol =
        rogue_schema_add_field(out_schema, "patrol_radius", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_patrol, 0, 1000);

    RogueSchemaField* f_aggro =
        rogue_schema_add_field(out_schema, "aggro_radius", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_aggro, 0, 1000);

    RogueSchemaField* f_speed =
        rogue_schema_add_field(out_schema, "speed", ROGUE_SCHEMA_TYPE_NUMBER);
    /* number range helper not implemented; reuse integer range semantics by description */
    /* Enforce via integer-style flags with casting would be confusing; skip for now. */

    RogueSchemaField* f_pop =
        rogue_schema_add_field(out_schema, "pop_target", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_pop, 0, 100000);

    RogueSchemaField* f_xp =
        rogue_schema_add_field(out_schema, "xp_reward", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_xp, 0, 100000);

    RogueSchemaField* f_loot =
        rogue_schema_add_field(out_schema, "loot_chance", ROGUE_SCHEMA_TYPE_NUMBER);

    (void) f_speed;
    (void) f_loot;

    rogue_schema_add_field(out_schema, "base_level_offset", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "tier_id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "archetype_id", ROGUE_SCHEMA_TYPE_INTEGER);

    /* Optional texture paths as strings if present */
    rogue_schema_add_field(out_schema, "idle_sheet", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "run_sheet", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "death_sheet", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "loot_table", ROGUE_SCHEMA_TYPE_STRING);

    return true;
}

bool rogue_entities_validate_types(const RogueEnemyTypeDef* types, int count,
                                   RogueSchemaValidationResult* result)
{
    if (!types || count <= 0 || !result)
        return false;

    RogueSchema schema;
    if (!rogue_entities_build_schema(&schema))
        return false;

    /* Validate each type; accumulate errors (stop on first failure for simplicity) */
    for (int i = 0; i < count; ++i)
    {
        /* Snapshot key fields to detect unexpected mutations during validation */
        char snap_id[32];
        char snap_name[32];
        int snap_gmin = types[i].group_min;
        int snap_gmax = types[i].group_max;
#if defined(_MSC_VER)
        strncpy_s(snap_id, sizeof snap_id, types[i].id, _TRUNCATE);
        strncpy_s(snap_name, sizeof snap_name, types[i].name, _TRUNCATE);
#else
        strncpy(snap_id, types[i].id, sizeof snap_id - 1);
        snap_id[sizeof snap_id - 1] = '\0';
        strncpy(snap_name, types[i].name, sizeof snap_name - 1);
        snap_name[sizeof snap_name - 1] = '\0';
#endif
        RogueJsonValue* obj = enemy_typedef_to_json(&types[i]);
        if (!obj)
            return false;
        RogueSchemaValidationResult local = {0};
        bool ok = rogue_schema_validate_json(&schema, obj, &local);
        if (!ok)
        {
            /* Debug: dump key fields from the JSON object to diagnose mismatch */
            const RogueJsonValue* jid = json_object_get(obj, "id");
            const RogueJsonValue* jname = json_object_get(obj, "name");
            const RogueJsonValue* jgmin = json_object_get(obj, "group_min");
            const RogueJsonValue* jgmax = json_object_get(obj, "group_max");
            ROGUE_LOG_WARN(
                "entity[%d] debug: id='%s' name='%s' gmin=%lld gmax=%lld (from JSON)", i,
                (jid && jid->type == JSON_STRING && jid->data.string_value) ? jid->data.string_value
                                                                            : "<null>",
                (jname && jname->type == JSON_STRING && jname->data.string_value)
                    ? jname->data.string_value
                    : "<null>",
                (long long) ((jgmin && jgmin->type == JSON_INTEGER) ? jgmin->data.integer_value
                                                                    : -9999),
                (long long) ((jgmax && jgmax->type == JSON_INTEGER) ? jgmax->data.integer_value
                                                                    : -9999));
            ROGUE_LOG_WARN("entity[%d] snapshot before validate: id='%s' name='%s' gmin=%d gmax=%d "
                           "(from types)",
                           i, snap_id, snap_name, snap_gmin, snap_gmax);
            ROGUE_LOG_WARN(
                "entity[%d] current types: id='%s' name='%s' gmin=%d gmax=%d (post-validate)", i,
                types[i].id, types[i].name, types[i].group_min, types[i].group_max);
            /* Copy first error and annotate id for easier debugging */
            *result = local;
            if (result->error_count < ROGUE_SCHEMA_MAX_VALIDATION_ERRORS)
            {
                size_t idx = result->error_count++;
                result->errors[idx].type = ROGUE_SCHEMA_ERROR_CUSTOM_VALIDATION_FAILED;
                snprintf(result->errors[idx].field_path, sizeof(result->errors[idx].field_path),
                         "entity[%d]", i);
                snprintf(result->errors[idx].message, sizeof(result->errors[idx].message),
                         "Validation failed for id='%s' name='%s'", types[i].id, types[i].name);
            }
            json_free(obj);
            return false;
        }
        json_free(obj);
    }
    result->is_valid = true;
    return true;
}

bool rogue_entities_validate_assets_default(RogueSchemaValidationResult* result, int* out_count)
{
    if (!result)
        return false;
    RogueEnemyTypeDef types[ROGUE_MAX_ENEMY_TYPES];
    int count = 0;
    const char* paths[] = {"../assets/enemies", "../../assets/enemies", "../../../assets/enemies",
                           "../../../../assets/enemies"};
    int ok = 0;
    for (size_t pi = 0; pi < sizeof(paths) / sizeof(paths[0]); ++pi)
    {
        count = 0;
        if (rogue_enemy_types_load_directory_json(paths[pi], types, ROGUE_MAX_ENEMY_TYPES,
                                                  &count) &&
            count > 0)
        {
            ok = 1;
            break;
        }
    }
    if (!ok)
        return false;
    bool valid = rogue_entities_validate_types(types, count, result);
    if (out_count)
        *out_count = count;
    return valid;
}
