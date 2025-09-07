#include "../../src/content/schema_audio.h"
#include "../../src/content/schema_sprites.h"
#include "../../src/util/json_parser.h"
#include <stdio.h>
#include <string.h>

static RogueJsonValue* build_valid_sprite_json(void)
{
    RogueJsonValue* root = json_create_object();
    json_object_set(root, "name", json_create_string("player"));
    json_object_set(root, "texture_file", json_create_string("assets/player.png"));
    json_object_set(root, "texture_width", json_create_integer(256));
    json_object_set(root, "texture_height", json_create_integer(128));
    RogueJsonValue* sprites = json_create_array();
    RogueJsonValue* s0 = json_create_object();
    json_object_set(s0, "id", json_create_string("idle_01"));
    json_object_set(s0, "x", json_create_integer(0));
    json_object_set(s0, "y", json_create_integer(0));
    json_object_set(s0, "width", json_create_integer(32));
    json_object_set(s0, "height", json_create_integer(32));
    /* json_array_push does not exist in current json API; use json_array_add */
    json_array_add(sprites, s0);
    json_object_set(root, "sprites", sprites);
    return root;
}

static RogueJsonValue* build_invalid_sprite_json(void)
{
    RogueJsonValue* root = json_create_object();
    /* missing required fields like name, texture_file, sprites */
    json_object_set(root, "texture_width", json_create_integer(0)); /* invalid range */
    return root;
}

static RogueJsonValue* build_valid_audio_json(void)
{
    RogueJsonValue* root = json_create_object();
    json_object_set(root, "name", json_create_string("ui_click"));
    json_object_set(root, "file", json_create_string("assets/audio/ui/click.ogg"));
    json_object_set(root, "volume", json_create_number(0.75));
    json_object_set(root, "loop", json_create_boolean(false));
    return root;
}

static RogueJsonValue* build_invalid_audio_json(void)
{
    RogueJsonValue* root = json_create_object();
    json_object_set(root, "file", json_create_string("missing_name.ogg")); /* missing name */
    return root;
}

int main(void)
{
    /* Sprite valid */
    RogueJsonValue* sv = build_valid_sprite_json();
    RogueSchemaValidationResult r = {0};
    if (!rogue_sprites_validate_json(sv, &r) || !r.is_valid)
    {
        printf("FAIL: expected valid sprite json (errors=%u)\n", r.error_count);
        return 1;
    }
    json_free(sv);

    /* Sprite invalid */
    RogueJsonValue* si = build_invalid_sprite_json();
    r = (RogueSchemaValidationResult){0};
    if (rogue_sprites_validate_json(si, &r))
    {
        printf("FAIL: expected invalid sprite json to fail\n");
        return 1;
    }
    json_free(si);

    /* Audio valid */
    RogueJsonValue* av = build_valid_audio_json();
    r = (RogueSchemaValidationResult){0};
    if (!rogue_audio_validate_json(av, &r) || !r.is_valid)
    {
        printf("FAIL: expected valid audio json (errors=%u)\n", r.error_count);
        for (unsigned i = 0; i < r.error_count; i++)
        {
            printf("  err[%u] type=%d field=%s msg=%s\n", i, r.errors[i].type,
                   r.errors[i].field_path, r.errors[i].message);
        }
        return 1;
    }
    json_free(av);

    /* Audio invalid */
    RogueJsonValue* ai = build_invalid_audio_json();
    r = (RogueSchemaValidationResult){0};
    if (rogue_audio_validate_json(ai, &r))
    {
        printf("FAIL: expected invalid audio json to fail\n");
        return 1;
    }
    json_free(ai);

    printf("OK test_sprite_audio_schema\n");
    return 0;
}
