/**
 * @file schema_skills.c
 * @brief Schema Validation for Skill Definitions.
 *
 * This module provides schema building and validation for RogueSkillDef structures used to define
 * skills in the Rogue game, including names, icons, ranks, cooldowns, costs, cast times, effects,
 * animations, audio, and advanced mechanics like combo building/spending, AoE shapes, projectiles,
 * and multi-effect nodes (up to 4 effects with delays, repeats, durations, conditions). Converts
 * defs to JSON for validation with constraints on ranges like max_rank (1-10), cooldowns, socket
 * counts, etc. Supports validation of individual JSON or arrays of defs. Allows additional fields
 * for forward compatibility. Includes Phase 1.1/1.2 notes for extended fields and effect
 * composition.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 */

#include "schema_skills.h"
#include "../util/json_parser.h"
#include "json_envelope.h"
#include "json_io.h"
#include <string.h>

/**
 * @brief Converts a RogueSkillDef to a JSON object for schema validation.
 *
 * Maps core fields like name, icon, max_rank, skill_strength, skill_type (if set), cooldowns,
 * is_passive, tags, synergy details, costs (mana, action points), charges, cast_time, buffers,
 * cast_type, combo flags, effect_spec_id. For Phase 1.2 effect composition nodes (optional, up to 3
 * additional effects): maps effect2/3/4_spec_id, delay/repeat_count/interval/duration_ms, health
 * condition. Optional extended fields (Phase 1.1) for visuals (cast/projectile/impact/aoe sprites,
 * frame_count/duration/loops, grid w/h), audio (sound IDs, volume 0-100, pitch variance), AoE
 * (shape 0-4, radius, angle), projectile (velocity, trajectory 0-3, pierce 0-32, homing strength).
 * Only includes non-empty/non-zero fields.
 *
 * @param d Pointer to the skill definition (may be NULL, returns NULL).
 * @return Allocated JSON object on success, NULL on failure (e.g., allocation error).
 */
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
    if (d->skill_type)
        json_object_set(obj, "skill_type", json_create_integer(d->skill_type));
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
    /* Phase 1.2 – Effect composition nodes (optional) */
    if (d->effect_node_count > 0)
    {
        if (d->effect_nodes[0].effect_spec_id >= 0)
            json_object_set(obj, "effect2_spec_id",
                            json_create_integer(d->effect_nodes[0].effect_spec_id));
        if (d->effect_nodes[0].delay_ms != 0.0f)
            json_object_set(obj, "effect2_delay_ms",
                            json_create_number(d->effect_nodes[0].delay_ms));
        if (d->effect_nodes[0].repeat_count)
            json_object_set(obj, "effect2_repeat_count",
                            json_create_integer(d->effect_nodes[0].repeat_count));
        if (d->effect_nodes[0].repeat_interval_ms != 0.0f)
            json_object_set(obj, "effect2_repeat_interval_ms",
                            json_create_number(d->effect_nodes[0].repeat_interval_ms));
        if (d->effect_nodes[0].duration_ms != 0.0f)
            json_object_set(obj, "effect2_duration_ms",
                            json_create_number(d->effect_nodes[0].duration_ms));
        if (d->effect_nodes[0].require_player_health_below_pct)
            json_object_set(
                obj, "effect2_require_player_health_below_pct",
                json_create_integer(d->effect_nodes[0].require_player_health_below_pct));
    }
    if (d->effect_node_count > 1)
    {
        if (d->effect_nodes[1].effect_spec_id >= 0)
            json_object_set(obj, "effect3_spec_id",
                            json_create_integer(d->effect_nodes[1].effect_spec_id));
        if (d->effect_nodes[1].delay_ms != 0.0f)
            json_object_set(obj, "effect3_delay_ms",
                            json_create_number(d->effect_nodes[1].delay_ms));
        if (d->effect_nodes[1].repeat_count)
            json_object_set(obj, "effect3_repeat_count",
                            json_create_integer(d->effect_nodes[1].repeat_count));
        if (d->effect_nodes[1].repeat_interval_ms != 0.0f)
            json_object_set(obj, "effect3_repeat_interval_ms",
                            json_create_number(d->effect_nodes[1].repeat_interval_ms));
        if (d->effect_nodes[1].duration_ms != 0.0f)
            json_object_set(obj, "effect3_duration_ms",
                            json_create_number(d->effect_nodes[1].duration_ms));
        if (d->effect_nodes[1].require_player_health_below_pct)
            json_object_set(
                obj, "effect3_require_player_health_below_pct",
                json_create_integer(d->effect_nodes[1].require_player_health_below_pct));
    }
    if (d->effect_node_count > 2)
    {
        if (d->effect_nodes[2].effect_spec_id >= 0)
            json_object_set(obj, "effect4_spec_id",
                            json_create_integer(d->effect_nodes[2].effect_spec_id));
        if (d->effect_nodes[2].delay_ms != 0.0f)
            json_object_set(obj, "effect4_delay_ms",
                            json_create_number(d->effect_nodes[2].delay_ms));
        if (d->effect_nodes[2].repeat_count)
            json_object_set(obj, "effect4_repeat_count",
                            json_create_integer(d->effect_nodes[2].repeat_count));
        if (d->effect_nodes[2].repeat_interval_ms != 0.0f)
            json_object_set(obj, "effect4_repeat_interval_ms",
                            json_create_number(d->effect_nodes[2].repeat_interval_ms));
        if (d->effect_nodes[2].duration_ms != 0.0f)
            json_object_set(obj, "effect4_duration_ms",
                            json_create_number(d->effect_nodes[2].duration_ms));
        if (d->effect_nodes[2].require_player_health_below_pct)
            json_object_set(
                obj, "effect4_require_player_health_below_pct",
                json_create_integer(d->effect_nodes[2].require_player_health_below_pct));
    }
    /* Optional extended fields (included when non-empty/non-zero) */
    if (d->cast_sprite_sheet && d->cast_sprite_sheet[0])
        json_object_set(obj, "cast_sprite_sheet", json_create_string(d->cast_sprite_sheet));
    if (d->projectile_sprite && d->projectile_sprite[0])
        json_object_set(obj, "projectile_sprite", json_create_string(d->projectile_sprite));
    if (d->impact_sprite && d->impact_sprite[0])
        json_object_set(obj, "impact_sprite", json_create_string(d->impact_sprite));
    if (d->aoe_sprite && d->aoe_sprite[0])
        json_object_set(obj, "aoe_sprite", json_create_string(d->aoe_sprite));
    if (d->frame_count)
        json_object_set(obj, "frame_count", json_create_integer(d->frame_count));
    if (d->frame_duration_ms != 0.0f)
        json_object_set(obj, "frame_duration_ms", json_create_number(d->frame_duration_ms));
    if (d->animation_loops)
        json_object_set(obj, "animation_loops", json_create_integer(d->animation_loops));
    if (d->grid_width)
        json_object_set(obj, "grid_width", json_create_integer(d->grid_width));
    if (d->grid_height)
        json_object_set(obj, "grid_height", json_create_integer(d->grid_height));
    if (d->cast_sound_id && d->cast_sound_id[0])
        json_object_set(obj, "cast_sound_id", json_create_string(d->cast_sound_id));
    if (d->impact_sound_id && d->impact_sound_id[0])
        json_object_set(obj, "impact_sound_id", json_create_string(d->impact_sound_id));
    if (d->loop_sound_id && d->loop_sound_id[0])
        json_object_set(obj, "loop_sound_id", json_create_string(d->loop_sound_id));
    if (d->sound_volume)
        json_object_set(obj, "sound_volume", json_create_integer(d->sound_volume));
    if (d->sound_pitch_variance != 0.0f)
        json_object_set(obj, "sound_pitch_variance", json_create_number(d->sound_pitch_variance));
    if (d->aoe_shape)
        json_object_set(obj, "aoe_shape", json_create_integer(d->aoe_shape));
    if (d->aoe_radius != 0.0f)
        json_object_set(obj, "aoe_radius", json_create_number(d->aoe_radius));
    if (d->aoe_angle != 0.0f)
        json_object_set(obj, "aoe_angle", json_create_number(d->aoe_angle));
    if (d->projectile_velocity != 0.0f)
        json_object_set(obj, "projectile_velocity", json_create_number(d->projectile_velocity));
    if (d->trajectory_type)
        json_object_set(obj, "trajectory_type", json_create_integer(d->trajectory_type));
    if (d->pierce_count)
        json_object_set(obj, "pierce_count", json_create_integer(d->pierce_count));
    if (d->homing_strength != 0.0f)
        json_object_set(obj, "homing_strength", json_create_number(d->homing_strength));
    return obj;
}

/**
 * @brief Builds the schema for validating skill definitions.
 *
 * Initializes a RogueSchema with required name (string 1-127), max_rank (1-10 integer), optional
 * icon (0-255 string), skill_strength (0-10), skill_type (0-9), cooldowns (numbers), is_passive
 * (0-1), tags, synergy_id (-1-31), synergy_value_per_rank, costs (integers), max_charges (0-10),
 * charge_recharge_ms (number), cast_time_ms (number), input_buffer/min_weave_ms (0-10000),
 * early_cancel_pct (0-100), cast_type (0-2), combo_builder/spender (0-1), effect_spec_id (integer).
 * Phase 1.1 extended optional fields for visuals (sprite sheets, frame_count 0-1024, duration/loops
 * 0-1, grid 0-1024), audio (sound IDs, volume 0-100, pitch variance number), AoE (shape 0-4,
 * radius/angle number), projectile (velocity number, trajectory 0-3, pierce 0-32, homing number).
 * Phase 1.2 for effect2/3/4 (spec_id, delay/repeat_count/interval/duration_ms number/integer,
 * health_below_pct 0-100). Strict mode true, allows additional fields for forward-compat. Version
 * current. Description: Schema for skill definitions.
 *
 * @param[out] out_schema Pointer to receive the built schema (required).
 * @return true on success (schema built), false on invalid out_schema.
 */
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

    /* Optional skill_type enum; 0=UNKNOWN (auto), 1..9 mapped to RogueSkillType */
    RogueSchemaField* f_type =
        rogue_schema_add_field(out_schema, "skill_type", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_type, 0, 9);

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

    /* Phase 1.1 – Extended, optional fields to support visual/audio/animation and mechanics. */
    /* Visual sprite assets (paths relative to project root or assets/) */
    rogue_schema_add_field(out_schema, "cast_sprite_sheet", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "projectile_sprite", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "impact_sprite", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "aoe_sprite", ROGUE_SCHEMA_TYPE_STRING);

    /* Animation parameters (sprite-sheet based); all optional */
    RogueSchemaField* f_frames =
        rogue_schema_add_field(out_schema, "frame_count", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_frames, 0, 1024);
    rogue_schema_add_field(out_schema, "frame_duration_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_anim_loops =
        rogue_schema_add_field(out_schema, "animation_loops", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_anim_loops, 0, 1);
    RogueSchemaField* f_grid_w =
        rogue_schema_add_field(out_schema, "grid_width", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_grid_w, 0, 1024);
    RogueSchemaField* f_grid_h =
        rogue_schema_add_field(out_schema, "grid_height", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_grid_h, 0, 1024);

    /* Audio hooks (IDs refer to sounds registry/labels); volume 0..100 (%), pitch variance in
     * semitones */
    rogue_schema_add_field(out_schema, "cast_sound_id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "impact_sound_id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(out_schema, "loop_sound_id", ROGUE_SCHEMA_TYPE_STRING);
    RogueSchemaField* f_svol =
        rogue_schema_add_field(out_schema, "sound_volume", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_svol, 0, 100);
    rogue_schema_add_field(out_schema, "sound_pitch_variance", ROGUE_SCHEMA_TYPE_NUMBER);

    /* AoE parameters */
    RogueSchemaField* f_aoe_shape =
        rogue_schema_add_field(out_schema, "aoe_shape", ROGUE_SCHEMA_TYPE_INTEGER);
    /* 0=none,1=circle,2=cone,3=line,4=poly (reserved) */
    rogue_schema_field_set_range(f_aoe_shape, 0, 4);
    RogueSchemaField* f_aoe_radius =
        rogue_schema_add_field(out_schema, "aoe_radius", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_aoe_angle =
        rogue_schema_add_field(out_schema, "aoe_angle", ROGUE_SCHEMA_TYPE_NUMBER);

    /* Projectile parameters */
    rogue_schema_add_field(out_schema, "projectile_velocity", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_traj =
        rogue_schema_add_field(out_schema, "trajectory_type", ROGUE_SCHEMA_TYPE_INTEGER);
    /* 0=linear,1=arc,2=homing,3=scatter */
    rogue_schema_field_set_range(f_traj, 0, 3);
    RogueSchemaField* f_pierce =
        rogue_schema_add_field(out_schema, "pierce_count", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_pierce, 0, 32);
    RogueSchemaField* f_home =
        rogue_schema_add_field(out_schema, "homing_strength", ROGUE_SCHEMA_TYPE_NUMBER);
    (void) f_aoe_radius;
    (void) f_aoe_angle;
    (void) f_home;

    /* Phase 1.2 – Effect composition nodes (effect2/effect3/effect4) */
    rogue_schema_add_field(out_schema, "effect2_spec_id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect2_delay_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect2_repeat_count", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect2_repeat_interval_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect2_duration_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_e2hp = rogue_schema_add_field(
        out_schema, "effect2_require_player_health_below_pct", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_e2hp, 0, 100);
    rogue_schema_add_field(out_schema, "effect3_spec_id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect3_delay_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect3_repeat_count", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect3_repeat_interval_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect3_duration_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_e3hp = rogue_schema_add_field(
        out_schema, "effect3_require_player_health_below_pct", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_e3hp, 0, 100);
    rogue_schema_add_field(out_schema, "effect4_spec_id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect4_delay_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect4_repeat_count", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "effect4_repeat_interval_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(out_schema, "effect4_duration_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    RogueSchemaField* f_e4hp = rogue_schema_add_field(
        out_schema, "effect4_require_player_health_below_pct", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_e4hp, 0, 100);

    return true;
}

/**
 * @brief Validates a single skill JSON object against the skills schema.
 *
 * Builds the schema internally, zeros the result, performs validation. Sets result->is_valid true
 * on success.
 *
 * @param json JSON object representing a skill (required).
 * @param[out] result Pointer to receive validation results and errors (required).
 * @return true if schema built and validation succeeds, false otherwise.
 */
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

/**
 * @brief Validates an array of RogueSkillDef against the skills schema.
 *
 * Builds the schema internally, converts each def to JSON, validates individually. Stops on first
 * failure, copies local result to output. Sets result->is_valid true only if all pass.
 *
 * @param defs Array of skill definitions to validate (required, count > 0).
 * @param count Number of skills in the array.
 * @param[out] result Pointer to receive validation results and errors (required).
 * @return true if all defs validate successfully, false otherwise.
 */
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
