#include "../../src/util/cfg_parser.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("=== CFG File Analysis Test ===\n\n");

    const char* cfg_files[] = {"../../../assets/affixes.cfg",
                               "../../../assets/biome_assets.cfg",
                               "../../../assets/encounters.cfg",
                               "../../../assets/enemies.cfg",
                               "../../../assets/equipment_test_sockets.cfg",
                               "../../../assets/gems_test.cfg",
                               "../../../assets/hud_layout.cfg",
                               "../../../assets/mining_nodes.cfg",
                               "../../../assets/plants.cfg",
                               "../../../assets/player_anim.cfg",
                               "../../../assets/player_sheets.cfg",
                               "../../../assets/projectiles.cfg",
                               "../../../assets/resource_nodes.cfg",
                               "../../../assets/skills_uhf87f.cfg",
                               "../../../assets/sounds.cfg",
                               "../../../assets/test_equipment_items.cfg",
                               "../../../assets/test_items.cfg",
                               "../../../assets/test_loot_tables.cfg",
                               "../../../assets/tiles.cfg",
                               "../../../assets/trees.cfg",
                               "../../../assets/ui_theme_default.cfg"};

    int num_files = sizeof(cfg_files) / sizeof(cfg_files[0]);
    int successful_analyses = 0;

    for (int i = 0; i < num_files; i++)
    {
        printf("Analyzing: %s\n", cfg_files[i]);

        RogueCfgFileAnalysis* analysis = rogue_cfg_analyze_file(cfg_files[i]);
        if (analysis && analysis->validation_error_count == 0)
        {
            printf("  Category: %s\n", rogue_cfg_category_to_string(analysis->category));
            printf("  Format: %s\n", rogue_cfg_format_to_string(analysis->format));
            printf("  Data lines: %d\n", analysis->data_lines);
            printf("  Comment lines: %d\n", analysis->comment_lines);
            printf("  Fields detected: %d\n", analysis->field_count);

            if (analysis->field_count > 0)
            {
                printf("  First few field types:");
                for (int j = 0; j < analysis->field_count && j < 5; j++)
                {
                    printf(" %s", rogue_cfg_data_type_to_string(analysis->fields[j].type));
                }
                printf("\n");
            }

            successful_analyses++;
        }
        else
        {
            printf("  ERROR: Failed to analyze file\n");
            if (analysis)
            {
                printf("  Validation errors: %d\n", analysis->validation_error_count);
            }
        }

        if (analysis)
        {
            free(analysis);
        }
        printf("\n");
    }

    printf("=== Summary ===\n");
    printf("Total files: %d\n", num_files);
    printf("Successfully analyzed: %d\n", successful_analyses);
    printf("Success rate: %.1f%%\n", (float) successful_analyses / num_files * 100.0f);

    return 0;
}
