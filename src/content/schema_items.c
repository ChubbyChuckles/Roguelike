/**
 * @file schema_items.c
 * @brief Schema Validation for Item Definitions.
 *
 * This module provides schema building and validation for RogueItemDef structures used to define
 * item properties like categories, stats, sprites, rarities, implicit modifiers, and sockets in the
 * Rogue game. It converts item definitions to JSON for validation against a built schema with
 * constraints on fields like category range, stack max, sprite dimensions, rarity, and socket
 * counts. Supports validation of individual JSON objects or arrays of defs. Allows additional
 * fields for forward compatibility.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 */

#include "schema_items.h"
#include "../core/integration/json_schema.h"
#include "../util/json_parser.h"
#include "json_envelope.h"
#include "json_io.h"
#include <string.h>

/**
 * @brief Converts a RogueItemDef to a JSON object for schema validation.
 *
 * Maps all fields from the item definition to JSON values, including id, name, category, level_req,
 * stack_max, base stats (value, damage min/max, armor), sprite details (sheet path, tx/ty/tw/th),
 * rarity, flags, implicit modifiers (strength, dexterity, vitality, intelligence, armor flat,
 * resists for physical/fire/cold/lightning/poison/status), set_id, and socket min/max.
 *
 * @param d Pointer to the item definition (may be NULL, returns NULL).
 * @return Allocated JSON object on success, NULL on failure (e.g., allocation error).
 */
static RogueJsonValue* itemdef_to_json(const RogueItemDef* d)
{
    if (!d)
        return NULL;
    RogueJsonValue* obj = json_create_object();
    if (!obj)
        return NULL;
    json_object_set(obj, "id", json_create_string(d->id));
    json_object_set(obj, "name", json_create_string(d->name));
    json_object_set(obj, "category", json_create_integer(d->category));
    json_object_set(obj, "level_req", json_create_integer(d->level_req));
    json_object_set(obj, "stack_max", json_create_integer(d->stack_max));
    json_object_set(obj, "base_value", json_create_integer(d->base_value));
    json_object_set(obj, "base_damage_min", json_create_integer(d->base_damage_min));
    json_object_set(obj, "base_damage_max", json_create_integer(d->base_damage_max));
    json_object_set(obj, "base_armor", json_create_integer(d->base_armor));
    json_object_set(obj, "sprite_sheet", json_create_string(d->sprite_sheet));
    json_object_set(obj, "sprite_tx", json_create_integer(d->sprite_tx));
    json_object_set(obj, "sprite_ty", json_create_integer(d->sprite_ty));
    json_object_set(obj, "sprite_tw", json_create_integer(d->sprite_tw));
    json_object_set(obj, "sprite_th", json_create_integer(d->sprite_th));
    json_object_set(obj, "rarity", json_create_integer(d->rarity));
    json_object_set(obj, "flags", json_create_integer(d->flags));
    /* Implicit modifiers */
    json_object_set(obj, "implicit_strength", json_create_integer(d->implicit_strength));
    json_object_set(obj, "implicit_dexterity", json_create_integer(d->implicit_dexterity));
    json_object_set(obj, "implicit_vitality", json_create_integer(d->implicit_vitality));
    json_object_set(obj, "implicit_intelligence", json_create_integer(d->implicit_intelligence));
    json_object_set(obj, "implicit_armor_flat", json_create_integer(d->implicit_armor_flat));
    json_object_set(obj, "implicit_resist_physical",
                    json_create_integer(d->implicit_resist_physical));
    json_object_set(obj, "implicit_resist_fire", json_create_integer(d->implicit_resist_fire));
    json_object_set(obj, "implicit_resist_cold", json_create_integer(d->implicit_resist_cold));
    json_object_set(obj, "implicit_resist_lightning",
                    json_create_integer(d->implicit_resist_lightning));
    json_object_set(obj, "implicit_resist_poison", json_create_integer(d->implicit_resist_poison));
    json_object_set(obj, "implicit_resist_status", json_create_integer(d->implicit_resist_status));
    json_object_set(obj, "set_id", json_create_integer(d->set_id));
    json_object_set(obj, "socket_min", json_create_integer(d->socket_min));
    json_object_set(obj, "socket_max", json_create_integer(d->socket_max));
    return obj;
}

/**
 * @brief Builds the schema for validating item base definitions.
 *
 * Initializes a RogueSchema with required fields id (string 1-ROGUE_MAX_ITEM_ID_LEN-1), name
 * (1-ROGUE_MAX_ITEM_NAME_LEN-1), category (0 to ROGUE_ITEM__COUNT-1 integer), optional level_req,
 * stack_max (1-999999), base_value/damage_min/max/armor (integers), sprite_sheet (string 0-127),
 * sprite_tx/ty (integers), sprite_tw/th (1-4096), rarity (0-10), flags, implicit modifiers
 * (strength/dexterity/vitality/intelligence/armor_flat/resists), set_id, socket_min/max (0-6).
 * Strict mode true, allows additional fields for forward-compat. Version current. Description:
 * Schema for item base definitions.
 *
 * @param[out] out_schema Pointer to receive the built schema (required).
 * @return true on success (schema built), false on invalid out_schema.
 */
bool rogue_items_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "items", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for item base definitions",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* permit forward-compat fields */

    RogueSchemaField* f_id = rogue_schema_add_field(out_schema, "id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_id, true);
    rogue_schema_field_set_string_length(f_id, 1, ROGUE_MAX_ITEM_ID_LEN - 1);
    /* Optional schema_version for future migrations (1..1024) */
    RogueSchemaField* f_schema_version =
        rogue_schema_add_field(out_schema, "schema_version", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_schema_version)
        rogue_schema_field_set_range(f_schema_version, 1, 1024);

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, ROGUE_MAX_ITEM_NAME_LEN - 1);

    RogueSchemaField* f_cat =
        rogue_schema_add_field(out_schema, "category", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_cat, true);
    rogue_schema_field_set_range(f_cat, 0, ROGUE_ITEM__COUNT - 1);

    rogue_schema_add_field(out_schema, "level_req", ROGUE_SCHEMA_TYPE_INTEGER);

    RogueSchemaField* f_stack =
        rogue_schema_add_field(out_schema, "stack_max", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_stack, 1, 999999);

    rogue_schema_add_field(out_schema, "base_value", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "base_damage_min", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "base_damage_max", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "base_armor", ROGUE_SCHEMA_TYPE_INTEGER);

    RogueSchemaField* f_sheet =
        rogue_schema_add_field(out_schema, "sprite_sheet", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(f_sheet, 0, 127);
    rogue_schema_add_field(out_schema, "sprite_tx", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "sprite_ty", ROGUE_SCHEMA_TYPE_INTEGER);
    RogueSchemaField* f_tw =
        rogue_schema_add_field(out_schema, "sprite_tw", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_tw, 1, 4096);
    RogueSchemaField* f_th =
        rogue_schema_add_field(out_schema, "sprite_th", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_th, 1, 4096);

    RogueSchemaField* f_rarity =
        rogue_schema_add_field(out_schema, "rarity", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_rarity, 0, 10);
    rogue_schema_add_field(out_schema, "flags", ROGUE_SCHEMA_TYPE_INTEGER);

    /* Implicit block */
    rogue_schema_add_field(out_schema, "implicit_strength", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_dexterity", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_vitality", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_intelligence", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_armor_flat", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_physical", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_fire", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_cold", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_lightning", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_poison", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_add_field(out_schema, "implicit_resist_status", ROGUE_SCHEMA_TYPE_INTEGER);

    rogue_schema_add_field(out_schema, "set_id", ROGUE_SCHEMA_TYPE_INTEGER);

    RogueSchemaField* f_sock_min =
        rogue_schema_add_field(out_schema, "socket_min", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_sock_min, 0, 6);
    RogueSchemaField* f_sock_max =
        rogue_schema_add_field(out_schema, "socket_max", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(f_sock_max, 0, 6);

    return true;
}

/**
 * @brief Validates a single item JSON object against the items schema.
 *
 * Builds the schema internally, zeros the result, performs validation. Sets result->is_valid true
 * on success.
 *
 * @param json JSON object representing an item (required).
 * @param[out] result Pointer to receive validation results and errors (required).
 * @return true if schema built and validation succeeds, false otherwise.
 */
bool rogue_items_validate_item_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_items_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = true;
    return ok;
}

/**
 * @brief Validates an array of RogueItemDef against the items schema.
 *
 * Builds the schema internally, converts each def to JSON, validates individually. Stops on first
 * failure, copies local result to output. Sets result->is_valid true only if all pass.
 *
 * @param defs Array of item definitions to validate (required, count > 0).
 * @param count Number of items in the array.
 * @param[out] result Pointer to receive validation results and errors (required).
 * @return true if all defs validate successfully, false otherwise.
 */
bool rogue_items_validate_defs(const RogueItemDef* defs, int count,
                               RogueSchemaValidationResult* result)
{
    if (!defs || count <= 0 || !result)
        return false;
    RogueSchema schema;
    if (!rogue_items_build_schema(&schema))
        return false;
    for (int i = 0; i < count; ++i)
    {
        RogueJsonValue* obj = itemdef_to_json(&defs[i]);
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
