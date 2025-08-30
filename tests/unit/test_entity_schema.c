// Ensure SDL doesn't redefine main on Windows so we don't need SDL2main.lib
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/content/schema_entities.h"
#include "../../src/entities/enemy.h"
#include <stdio.h>

int main(void)
{
    /* Ensure headless validation doesn't try to load textures */
    rogue_enemy_loader_set_skip_textures(1);
    RogueSchemaValidationResult res = {0};
    int count = 0;
    /* Debug: load directly and print to pinpoint issues */
    RogueEnemyTypeDef types[ROGUE_MAX_ENEMY_TYPES];
    const char* paths[] = {"../assets/enemies", "../../assets/enemies", "../../../assets/enemies",
                           "../../../../assets/enemies"};
    int loaded = 0;
    for (int i = 0; i < 4; ++i)
    {
        loaded = 0;
        if (rogue_enemy_types_load_directory_json(paths[i], types, ROGUE_MAX_ENEMY_TYPES,
                                                  &loaded) &&
            loaded > 0)
            break;
    }
    if (loaded <= 0)
    {
        printf("FAIL entity schema: could not load enemy types from assets\\n");
        return 1;
    }
    for (int i = 0; i < loaded; ++i)
    {
        printf("loaded[%d]: id='%s' name='%s' gmin=%d gmax=%d\n", i, types[i].id, types[i].name,
               types[i].group_min, types[i].group_max);
    }
    count = loaded;
    if (!rogue_entities_validate_types(types, count, &res))
    {
        printf("FAIL entity schema validation (count=%d, errors=%u)\n", count, res.error_count);
        for (unsigned i = 0; i < res.error_count; ++i)
        {
            printf("  error[%u]: %s at %s\n", i, res.errors[i].message, res.errors[i].field_path);
        }
        return 1;
    }
    if (count <= 0)
    {
        printf("FAIL entity schema: no enemy types loaded\n");
        return 1;
    }
    printf("OK test_entity_schema (types=%d)\n", count);
    return 0;
}
