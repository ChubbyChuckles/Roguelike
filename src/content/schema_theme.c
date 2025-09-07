/* schema_theme.c - Phase 2: UI Theme/Layout JSON schema implementation
   Required: name (1-63)
   Optional color fields (ARGB hex strings or numeric RGBA?), for now use 32-bit RGBA unsigned ints:
     panel_bg, panel_border, text_normal, text_accent, button_bg, button_bg_hot, button_text,
     slider_track, slider_fill, tooltip_bg, alert_text
   Optional integer metrics: font_size_base (>=4..256), padding_small (>=0..128),
     padding_large (>=0..512), dpi_scale_x100 (50..400)
   Rationale: mirrors RogueUIThemePack for JSON-driven theming.
*/
#include "schema_theme.h"
#include <string.h>

bool rogue_theme_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "ui_theme", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for UI theme palette + metrics",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT; /* track with other schemas */
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true; /* forward compatible for future layout */

    RogueSchemaField* f_name = rogue_schema_add_field(out_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 63);

    /* Helper macro for optional 32-bit color integers */
#define THEME_COLOR_FIELD(id) rogue_schema_add_field(out_schema, id, ROGUE_SCHEMA_TYPE_INTEGER)
    THEME_COLOR_FIELD("panel_bg");
    THEME_COLOR_FIELD("panel_border");
    THEME_COLOR_FIELD("text_normal");
    THEME_COLOR_FIELD("text_accent");
    THEME_COLOR_FIELD("button_bg");
    THEME_COLOR_FIELD("button_bg_hot");
    THEME_COLOR_FIELD("button_text");
    THEME_COLOR_FIELD("slider_track");
    THEME_COLOR_FIELD("slider_fill");
    THEME_COLOR_FIELD("tooltip_bg");
    THEME_COLOR_FIELD("alert_text");
#undef THEME_COLOR_FIELD

    /* Metrics */
    RogueSchemaField* f_font =
        rogue_schema_add_field(out_schema, "font_size_base", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_font)
        rogue_schema_field_set_range(f_font, 4, 256);
    RogueSchemaField* f_pad_s =
        rogue_schema_add_field(out_schema, "padding_small", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_pad_s)
        rogue_schema_field_set_range(f_pad_s, 0, 128);
    RogueSchemaField* f_pad_l =
        rogue_schema_add_field(out_schema, "padding_large", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_pad_l)
        rogue_schema_field_set_range(f_pad_l, 0, 512);
    RogueSchemaField* f_dpi =
        rogue_schema_add_field(out_schema, "dpi_scale_x100", ROGUE_SCHEMA_TYPE_INTEGER);
    if (f_dpi)
        rogue_schema_field_set_range(f_dpi, 50, 400);
    return true;
}

bool rogue_theme_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_theme_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = (result->error_count == 0);
    return ok && result->is_valid;
}
