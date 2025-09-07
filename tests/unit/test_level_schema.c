/* Unit: test_level_schema
   Validates the Level/Map schema with positive and negative JSON samples. */

#include "../../src/content/schema_levels.h"
#include "../../src/util/json_parser.h"
#include <stdio.h>

static RogueJsonValue* build_valid_level_json(void)
{
    RogueJsonValue* root = json_create_object();
    json_object_set(root, "id", json_create_string("tutorial_entrance"));
    json_object_set(root, "w", json_create_integer(64));
    json_object_set(root, "h", json_create_integer(32));
    json_object_set(root, "tile_size", json_create_integer(64));
    /* Minimal RLE tiles string (example: 64*32 empty tiles encoded as E*2048) */
    json_object_set(root, "tiles", json_create_string("E*2048"));
    /* spawns */
    RogueJsonValue* spawns = json_create_array();
    RogueJsonValue* s0 = json_create_object();
    json_object_set(s0, "type", json_create_string("player_start"));
    json_object_set(s0, "x", json_create_integer(5));
    json_object_set(s0, "y", json_create_integer(4));
    json_array_add(spawns, s0);
    json_object_set(root, "spawns", spawns);
    /* environment tags */
    RogueJsonValue* env = json_create_array();
    RogueJsonValue* t0 = json_create_object();
    json_object_set(t0, "value", json_create_string("dungeon"));
    json_array_add(env, t0);
    json_object_set(root, "environment_tags", env);
    return root;
}

static RogueJsonValue* build_invalid_level_json(void)
{
    RogueJsonValue* root = json_create_object();
    /* missing required id, w, h, tiles; also invalid w */
    json_object_set(root, "w", json_create_integer(0));
    return root;
}

int main(void)
{
    RogueSchemaValidationResult r = {0};
    RogueJsonValue* good = build_valid_level_json();
    if (!rogue_levels_validate_json(good, &r) || !r.is_valid)
    {
        printf("FAIL: expected valid level json (errors=%u)\n", r.error_count);
        return 1;
    }
    json_free(good);

    RogueJsonValue* bad = build_invalid_level_json();
    r = (RogueSchemaValidationResult){0};
    if (rogue_levels_validate_json(bad, &r))
    {
        printf("FAIL: expected invalid level json to fail validation\n");
        return 1;
    }
    json_free(bad);
    printf("OK test_level_schema\n");
    return 0;
}
