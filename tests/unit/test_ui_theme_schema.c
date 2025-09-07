/* Unit: test_ui_theme_schema - validates ui theme JSON schema */
#include "../../src/content/schema_theme.h"
#include "../../src/util/json_parser.h"
#include <stdio.h>

static RogueJsonValue* build_valid_theme_json(void)
{
    RogueJsonValue* root = json_create_object();
    json_object_set(root, "name", json_create_string("default_theme"));
    json_object_set(root, "panel_bg", json_create_integer(0x202028FF));
    json_object_set(root, "panel_border", json_create_integer(0x404050FF));
    json_object_set(root, "text_normal", json_create_integer(0xFFFFFFFF));
    json_object_set(root, "font_size_base", json_create_integer(14));
    json_object_set(root, "dpi_scale_x100", json_create_integer(100));
    return root;
}

static RogueJsonValue* build_invalid_theme_json(void)
{
    RogueJsonValue* root = json_create_object();
    /* missing name, invalid dpi */
    json_object_set(root, "dpi_scale_x100", json_create_integer(10));
    return root;
}

int main(void)
{
    RogueSchemaValidationResult r = {0};
    RogueJsonValue* good = build_valid_theme_json();
    if (!rogue_theme_validate_json(good, &r) || !r.is_valid)
    {
        printf("FAIL: expected valid theme json (errors=%u)\n", r.error_count);
        return 1;
    }
    json_free(good);

    RogueJsonValue* bad = build_invalid_theme_json();
    r = (RogueSchemaValidationResult){0};
    if (rogue_theme_validate_json(bad, &r))
    {
        printf("FAIL: expected invalid theme json to fail validation\n");
        return 1;
    }
    json_free(bad);
    printf("OK test_ui_theme_schema\n");
    return 0;
}
