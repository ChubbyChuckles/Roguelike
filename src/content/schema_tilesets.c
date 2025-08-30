#include "schema_tilesets.h"
#include "../util/json_parser.h"
#include "../util/log.h"
#include "json_envelope.h"
#include "json_io.h"
#include <stdio.h>
#include <string.h>

/* Build tilesets schema: { id: string, tile_size: int>0, atlas: string, tiles: array of {name, col,
 * row} } */
bool rogue_tilesets_build_schema(RogueSchema* out_schema)
{
    if (!out_schema)
        return false;
    memset(out_schema, 0, sizeof(*out_schema));
    strncpy(out_schema->name, "tilesets", sizeof(out_schema->name) - 1);
    strncpy(out_schema->description, "Schema for tileset definitions",
            sizeof(out_schema->description) - 1);
    out_schema->version = ROGUE_SCHEMA_VERSION_CURRENT;
    out_schema->strict_mode = true;
    out_schema->allow_additional_fields = true;

    RogueSchemaField* f_id = rogue_schema_add_field(out_schema, "id", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_id, true);
    rogue_schema_field_set_string_length(f_id, 1, 63);

    RogueSchemaField* f_tile_size =
        rogue_schema_add_field(out_schema, "tile_size", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_tile_size, true);
    rogue_schema_field_set_range(f_tile_size, 1, 4096);

    RogueSchemaField* f_atlas =
        rogue_schema_add_field(out_schema, "atlas", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_atlas, true);
    rogue_schema_field_set_string_length(f_atlas, 1, 255);

    /* tiles: array of objects */
    RogueSchemaField* f_tiles =
        rogue_schema_add_field(out_schema, "tiles", ROGUE_SCHEMA_TYPE_ARRAY);
    rogue_schema_field_set_required(f_tiles, true);
    f_tiles->validation.constraints.array.has_min_items = true;
    f_tiles->validation.constraints.array.min_items = 1;
    /* describe item schema */
    static RogueSchema item_schema; /* static to keep memory around */
    memset(&item_schema, 0, sizeof(item_schema));
    strncpy(item_schema.name, "tile", sizeof(item_schema.name) - 1);
    item_schema.version = ROGUE_SCHEMA_VERSION_CURRENT;
    RogueSchemaField* f_name =
        rogue_schema_add_field(&item_schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(f_name, true);
    rogue_schema_field_set_string_length(f_name, 1, 63);
    RogueSchemaField* f_col =
        rogue_schema_add_field(&item_schema, "col", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_col, true);
    rogue_schema_field_set_range(f_col, 0, 4096);
    RogueSchemaField* f_row =
        rogue_schema_add_field(&item_schema, "row", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(f_row, true);
    rogue_schema_field_set_range(f_row, 0, 4096);
    f_tiles->array_item_schema = (RogueSchemaField*) &item_schema.fields[0]; /* hint presence */
    f_tiles->nested_schema = &item_schema;

    return true;
}

bool rogue_tilesets_validate_json(const RogueJsonValue* json, RogueSchemaValidationResult* result)
{
    if (!json || !result)
        return false;
    RogueSchema schema;
    if (!rogue_tilesets_build_schema(&schema))
        return false;
    memset(result, 0, sizeof(*result));
    bool ok = rogue_schema_validate_json(&schema, json, result);
    if (ok)
        result->is_valid = true;
    return ok;
}

/* Minimal legacy tiles.cfg line parser: recognizes segments like 'TILE, NAME, path, col, row' */
static int parse_tiles_cfg_to_json(const char* path, int tile_size, RogueJsonValue** out_json,
                                   int* out_count)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0;
    char line[1024];
    int count = 0;
    RogueJsonValue* obj = json_create_object();
    RogueJsonValue* tiles = json_create_array();
    json_object_set(obj, "id", json_create_string("default"));
    json_object_set(obj, "tile_size", json_create_integer(tile_size > 0 ? tile_size : 64));
    /* Track the last seen atlas path; not strictly correct for multiple atlases, but matches
     * current cfg. */
    char last_atlas[256];
    last_atlas[0] = '\0';
    while (fgets(line, sizeof line, f))
    {
        const char* s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == '\0' || *s == '\n' || *s == '#')
            continue;
        if (strncmp(s, "TILE", 4) != 0)
            continue;
        s += 4;
        while (*s == ' ' || *s == '\t' || *s == ',')
            s++;
        char name[64] = {0};
        int i = 0;
        while (s[i] && s[i] != ',' && s[i] != '\n' && i < (int) sizeof(name) - 1)
        {
            name[i] = s[i];
            i++;
        }
        if (s[i] != ',')
            continue; /* malformed */
        s += i + 1;
        while (*s == ' ' || *s == '\t')
            s++;
        char atlas[256] = {0};
        int j = 0;
        while (s[j] && s[j] != ',' && s[j] != '\n' && j < (int) sizeof(atlas) - 1)
        {
            atlas[j] = s[j];
            j++;
        }
        if (s[j] != ',')
            continue; /* malformed */
        s += j + 1;
        while (*s == ' ' || *s == '\t')
            s++;
        int col = atoi(s);
        while (*s && *s != ',' && *s != '\n')
            s++;
        if (*s != ',')
            continue;
        s++;
        while (*s == ' ' || *s == '\t')
            s++;
        int row = atoi(s);

        RogueJsonValue* tile = json_create_object();
        json_object_set(tile, "name", json_create_string(name));
        json_object_set(tile, "col", json_create_integer(col));
        json_object_set(tile, "row", json_create_integer(row));
        json_array_add(tiles, tile);
        count++;
        if (atlas[0])
        {
            /* remember last atlas */
            strncpy(last_atlas, atlas, sizeof(last_atlas) - 1);
            last_atlas[sizeof(last_atlas) - 1] = '\0';
        }
    }
    fclose(f);
    if (last_atlas[0] == '\0')
        strncpy(last_atlas, "assets/art/tiles.png", sizeof(last_atlas) - 1);
    json_object_set(obj, "atlas", json_create_string(last_atlas));
    json_object_set(obj, "tiles", tiles);
    *out_json = obj;
    if (out_count)
        *out_count = count;
    return 1;
}

bool rogue_tilesets_validate_cfg_file(const char* path, int tile_size,
                                      RogueSchemaValidationResult* result, int* out_count)
{
    if (!path || !result)
        return false;
    RogueJsonValue* json = NULL;
    int count = 0;
    if (!parse_tiles_cfg_to_json(path, tile_size, &json, &count))
        return false;
    bool ok = rogue_tilesets_validate_json(json, result);
    json_free(json);
    if (out_count)
        *out_count = count;
    return ok;
}

bool rogue_tilesets_validate_assets_default(RogueSchemaValidationResult* result, int* out_count)
{
    const char* paths[] = {"assets/tiles.cfg", "../assets/tiles.cfg", "../../assets/tiles.cfg",
                           "../../../assets/tiles.cfg"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
    {
        int count = 0;
        if (rogue_tilesets_validate_cfg_file(paths[i], 64, result, &count))
        {
            if (out_count)
                *out_count = count;
            return true;
        }
    }
    return false;
}
