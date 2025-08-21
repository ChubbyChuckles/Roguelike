#define SDL_MAIN_HANDLED
#include "entities/enemy.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueEnemyTypeDef types[ROGUE_MAX_ENEMY_TYPES];
    int count = 0;
    const char* paths[] = {"../assets/enemies", "../../assets/enemies", "../../../assets/enemies",
                           "../../../../assets/enemies"};
    int ok = 0;
    for (size_t pi = 0; pi < sizeof(paths) / sizeof(paths[0]); ++pi)
    {
        count = 0;
        if (rogue_enemy_types_load_directory_json(paths[pi], types, ROGUE_MAX_ENEMY_TYPES,
                                                  &count) &&
            count >= 2)
        {
            ok = 1;
            break;
        }
    }
    if (!ok || count < 2)
    {
        printf("FAIL expected >=2 enemy types json loaded count=%d\n", count);
        return 1;
    }
    int found_grunt = 0, found_elite = 0;
    for (int i = 0; i < count; i++)
    {
        if (strcmp(types[i].id, "goblin_grunt") == 0 || strcmp(types[i].id, "orc_grunt") == 0 ||
            strcmp(types[i].id, "skeleton_grunt") == 0)
        {
            found_grunt = 1; /* basic grunt style enemy */
        }
        if (strcmp(types[i].id, "goblin_elite") == 0 || strcmp(types[i].id, "orc_warrior") == 0 ||
            strcmp(types[i].id, "skeleton_warrior") == 0)
        {
            found_elite = 1; /* higher tier / elite style */
        }
    }
    if (!found_grunt || !found_elite)
    {
        printf("FAIL missing grunt=%d elite=%d\n", found_grunt, found_elite);
        return 1;
    }
    printf("OK test_enemy_json_loader (types=%d)\n", count);
    return 0;
}
