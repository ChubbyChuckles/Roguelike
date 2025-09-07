/* Unit: test_schema_versioning_basic - validates schema_version field presence and migration hook
 */
#include "../../src/core/integration/json_schema.h"
#include "../../src/util/json_parser.h"
#include <stdio.h>
#include <string.h>

/* Simple migration: for schema 'ui_theme' from v1->v2 add panel_border if missing */
static bool migrate_theme_v1_to_v2(RogueJsonValue* json)
{
    if (!json || json->type != JSON_OBJECT)
        return false;
    /* If panel_border missing, add a default color */
    const char* key = "panel_border";
    for (size_t i = 0; i < json->data.object_value.count; i++)
    {
        if (strcmp(json->data.object_value.keys[i], key) == 0)
            return true; /* already present */
    }
    json_object_set(json, key, json_create_integer(0x404050FF));
    return true;
}

int main(void)
{
    /* Register migration step */
    RogueSchemaMigrationStep step = {0};
    strncpy(step.schema_name, "ui_theme", sizeof(step.schema_name) - 1);
    step.from_version = 1;
    step.to_version = 2;
    step.migrate = migrate_theme_v1_to_v2;
    if (!rogue_schema_register_migration(&step))
    {
        printf("FAIL register migration\n");
        return 1;
    }

    /* Build minimal theme json at version 1 */
    RogueJsonValue* obj = json_create_object();
    json_object_set(obj, "name", json_create_string("v1_theme"));
    json_object_set(obj, "schema_version", json_create_integer(1));
    /* missing panel_border intentionally */

    /* Apply migrations to target version 2 */
    if (!rogue_schema_apply_registered_migrations("ui_theme", 1, 2, obj))
    {
        printf("FAIL apply migration\n");
        json_free(obj);
        return 1;
    }

    /* Confirm panel_border now exists */
    const RogueJsonValue* border = NULL;
    for (size_t i = 0; i < obj->data.object_value.count; i++)
    {
        if (strcmp(obj->data.object_value.keys[i], "panel_border") == 0)
        {
            border = obj->data.object_value.values[i];
            break;
        }
    }
    if (!border || border->type != JSON_INTEGER)
    {
        printf("FAIL migration did not add panel_border\n");
        json_free(obj);
        return 1;
    }

    json_free(obj);
    printf("OK test_schema_versioning_basic\n");
    return 0;
}
