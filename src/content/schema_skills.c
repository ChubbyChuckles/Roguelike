#include "schema_skills.h"
#include "../util/json_parser.h"
#include "json_envelope.h"
#include "json_io.h"
#include <string.h>

static RogueJsonValue* skilldef_to_json(const RogueSkillDef* d)
{
    if (!d)
        return NULL;
    RogueJsonValue* obj = json_create_object();
    if (!obj)
        return NULL;
    json_object_set(obj, "name", json_create_string(d->name ? d->name : ""));
    json_object_set(obj, "icon", json_create_string(d->icon ? d->icon : ""));
    json_object_set(obj, "max_rank", json_create_integer(d->max_rank));
    json_object_set(obj, "skill_strength", json_create_integer(d->skill_strength));
    json_object_set(obj, "base_cooldown_ms", json_create_number(d->base_cooldown_ms));
    json_object_set(obj, "cooldown_reduction_ms_per_rank",
                    json_create_number(d->cooldown_reduction_ms_per_rank));
    json_object_set(obj, "is_passive", json_create_integer(d->is_passive));
    json_object_set(obj, "tags", json_create_integer(d->tags));
    json_object_set(obj, "synergy_id", json_create_integer(d->synergy_id));
    json_object_set(obj, "synergy_value_per_rank", json_create_integer(d->synergy_value_per_rank));
    json_object_set(obj, "resource_cost_mana", json_create_integer(d->resource_cost_mana));
    json_object_set(obj, "action_point_cost", json_create_integer(d->action_point_cost));
    json_object_set(obj, "max_charges", json_create_integer(d->max_charges));
    json_object_set(obj, "charge_recharge_ms", json_create_number(d->charge_recharge_ms));
    json_object_set(obj, "cast_time_ms", json_create_number(d->cast_time_ms));
    json_object_set(obj, "input_buffer_ms", json_create_integer(d->input_buffer_ms));
    json_object_set(obj, "min_weave_ms", json_create_integer(d->min_weave_ms));
    json_object_set(obj, "early_cancel_min_pct", json_create_integer(d->early_cancel_min_pct));
    json_object_set(obj, "cast_type", json_create_integer(d->cast_type));
    json_object_set(obj, "combo_builder", json_create_integer(d->combo_builder));
    json_object_set(obj, "combo_spender", json_create_integer(d->combo_spender));
    json_object_set(obj, "effect_spec_id", json_create_integer(d->effect_spec_id));
    return obj;
}

bool rogue_skills_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "skills", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for skill definitions",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* forward-compat */

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 127);

    RogueSchemaField* f_icon = rogue_schema_add_field(out_schema, "icon", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(f_icon, 0, 255);

    RogueSchemaField* f_maxr =
        rogue_schema_add_field(out_schema, "max_rank", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_maxr, true);
    rogue_schema_field_set_range(f_maxr, 1, 10);

    RogueSchemaField* f_strength =
        rogue_schema_add_field(out_schema, "skill_strength", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_strength, 0, 10);

    rogue_schema_add_field(out_schema, "base_cooldown_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "cooldown_reduction_ms_per_rank", ROGUE_SCHEMA_TYPE_NUMBER);

    RogueSchemaField* f_passive =
        rogue_schema_add_field(out_schema, "is_passive", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_passive, 0, 1);

    rogue_schema_add_field(out_schema, "tags", ROGUE_SCHEMA_TYPE_INTEGER);

    RogueSchemaField* f_syn =
        rogue_schema_add_field(out_schema, "synergy_id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_syn, -1, 31);
    rogue_schema_add_field(out_schema, "synergy_value_per_rank", ROGUE_SCHEMA_TYPE_INTEGER);

    rogue_schema_add_field(out_schema, "resource_cost_mana", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "action_point_cost", ROGUE_SCHEMA_TYPE_INTEGER);

    RogueSchemaField* f_maxc =
        rogue_schema_add_field(out_schema, "max_charges", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_maxc, 0, 10);
    rogue_schema_add_field(out_schema, "charge_recharge_ms", ROGUE_SCHEMA_TYPE_NUMBER);

    rogue_schema_add_field(out_schema, "cast_time_ms", ROGUE_SCHEMA_TYPE_NUMBER);

    RogueSchemaField* f_ib =
        rogue_schema_add_field(out_schema, "input_buffer_ms", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_ib, 0, 10000);
    RogueSchemaField* f_mw =
        rogue_schema_add_field(out_schema, "min_weave_ms", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_mw, 0, 10000);
    RogueSchemaField* f_ec =
        rogue_schema_add_field(out_schema, "early_cancel_min_pct", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_ec, 0, 100);

    RogueSchemaField* f_ct =
        rogue_schema_add_field(out_schema, "cast_type", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_ct, 0, 2);

    RogueSchemaField* f_cb =
        rogue_schema_add_field(out_schema, "combo_builder", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_cb, 0, 1);
    RogueSchemaField* f_cs =
        rogue_schema_add_field(out_schema, "combo_spender", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_cs, 0, 1);

    rogue_schema_add_field(out_schema, "effect_spec_id", ROGUE_SCHEMA_TYPE_INTEGER);

    return true;
}

bool rogue_skills_validate_skill_json(const RogueJsonValue* json,
                                      RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_skills_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = true;
    return ok;
}

bool rogue_skills_validate_defs(const struct RogueSkillDef* defs, int count,
                                RogueSchemaValidationResult* result)
{
    if (!defs || count <= 0 || !result)
        return false;
    RogueSchema schema;
    if (!rogue_skills_build_schema(&schema))
        return false;
    for (int i = 0; i < count; ++i)
    {
        RogueJsonValue* obj = skilldef_to_json(&defs[i]);
        if (!obj)
            return false;
        RogueSchemaValidationResult local = {0};
        bool ok = rogue_schema_validate_json(&schema, obj, &local);
        json_free(obj);
        if (!ok)
        {
            *result = local;
            return false;
        }
    }
    result->is_valid = true;
    return true;
}
