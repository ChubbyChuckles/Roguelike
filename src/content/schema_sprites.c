/* schema_sprites.c - Phase 2: Sprite sheet + animation schema implementation */
#include "schema_sprites.h"
#include "../util/json_parser.h"
#include <string.h>

/* Build sprite sheet schema:
   required: name (1-63), texture_file (1-255), texture_width (>=1), texture_height (>=1), sprites
   (array >=1) sprites[] objects: id (1-63), x>=0, y>=0, width>=1, height>=1, optional pivot_x
   (0..1), pivot_y (0..1) optional animations[]: name (1-63), frames>=1 array of { sprite_id (1-63),
   duration_ms>=0 }, loop bool
*/
bool rogue_sprites_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "sprites", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for sprite sheet + animation definitions",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* forward compat */

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 63);

    RogueSchemaField* f_tex_file =
        rogue_schema_add_field(out_schema, "texture_file", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_tex_file, true);
    rogue_schema_field_set_string_length(f_tex_file, 1, 255);

    RogueSchemaField* f_tw =
        rogue_schema_add_field(out_schema, "texture_width", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_tw, true);
    rogue_schema_field_set_range(f_tw, 1, 16384);
    RogueSchemaField* f_th =
        rogue_schema_add_field(out_schema, "texture_height", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_th, true);
    rogue_schema_field_set_range(f_th, 1, 16384);

    /* sprites array */
    RogueSchemaField* f_sprites =
        rogue_schema_add_field(out_schema, "sprites", ROGUE_SCHEMA_TYPE_ARRAY);
    rogue_schema_field_set_required(f_sprites, true);
    f_sprites->validation.constraints.array.has_min_items = true;
    f_sprites->validation.constraints.array.min_items = 1;
    static RogueSchema sprite_item_schema; /* static lifetime */
    memset(&sprite_item_schema, 0, sizeof(sprite_item_schema));
    strncpy(sprite_item_schema.name, "sprite", sizeof(sprite_item_schema.name) - 1);
    sprite_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* sp_id =
        rogue_schema_add_field(&sprite_item_schema, "id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(sp_id, true);
    rogue_schema_field_set_string_length(sp_id, 1, 63);
    RogueSchemaField* sp_x =
        rogue_schema_add_field(&sprite_item_schema, "x", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_x, 0, 1000000);
    RogueSchemaField* sp_y =
        rogue_schema_add_field(&sprite_item_schema, "y", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_y, 0, 1000000);
    RogueSchemaField* sp_w =
        rogue_schema_add_field(&sprite_item_schema, "width", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_w, 1, 1000000);
    RogueSchemaField* sp_h =
        rogue_schema_add_field(&sprite_item_schema, "height", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(sp_h, 1, 1000000);
    /* Optional pivots as number: we reuse integer type for simplicity until number range helper
     * added */
    rogue_schema_add_field(&sprite_item_schema, "pivot_x", ROGUE_SCHEMA_TYPE_NUMBER);
    rogue_schema_add_field(&sprite_item_schema, "pivot_y", ROGUE_SCHEMA_TYPE_NUMBER);
    f_sprites->array_item_schema = (RogueSchemaField*) &sprite_item_schema.fields[0];
    f_sprites->nested_schema = &sprite_item_schema;

    /* animations array (optional) */
    RogueSchemaField* f_anims =
        rogue_schema_add_field(out_schema, "animations", ROGUE_SCHEMA_TYPE_ARRAY);
    f_anims->validation.constraints.array.has_min_items = false; /* optional */
    static RogueSchema anim_item_schema;
    memset(&anim_item_schema, 0, sizeof(anim_item_schema));
    strncpy(anim_item_schema.name, "animation", sizeof(anim_item_schema.name) - 1);
    anim_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* an_name =
        rogue_schema_add_field(&anim_item_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(an_name, true);
    rogue_schema_field_set_string_length(an_name, 1, 63);
    RogueSchemaField* an_frames =
        rogue_schema_add_field(&anim_item_schema, "frames", ROGUE_SCHEMA_TYPE_ARRAY);
    an_frames->validation.constraints.array.has_min_items = true;
    an_frames->validation.constraints.array.min_items = 1;
    /* frame object schema */
    static RogueSchema frame_item_schema;
    memset(&frame_item_schema, 0, sizeof(frame_item_schema));
    strncpy(frame_item_schema.name, "frame", sizeof(frame_item_schema.name) - 1);
    frame_item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* fr_sprite =
        rogue_schema_add_field(&frame_item_schema, "sprite_id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(fr_sprite, true);
    rogue_schema_field_set_string_length(fr_sprite, 1, 63);
    RogueSchemaField* fr_dur =
        rogue_schema_add_field(&frame_item_schema, "duration_ms", ROGUE_SCHEMA_TYPE_NUMBER);
    /* loop flag */
    rogue_schema_add_field(&anim_item_schema, "loop", ROGUE_SCHEMA_TYPE_BOOLEAN);
    an_frames->array_item_schema = (RogueSchemaField*) &frame_item_schema.fields[0];
    an_frames->nested_schema = &frame_item_schema;
    f_anims->array_item_schema = (RogueSchemaField*) &anim_item_schema.fields[0];
    f_anims->nested_schema = &anim_item_schema;

    return true;
}

bool rogue_sprites_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_sprites_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = true;
    return ok;
}
