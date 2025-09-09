#include "../../src/util/cfg_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Resolve the assets base directory relative to current working directory.
   Tries a few likely candidates (build/, build/tests/Release, repo root). */
static void resolve_assets_base(char* out_base, size_t out_sz)
{
    const char* candidates[] = {"assets", "../assets", "../../assets", "../../../assets",
                                "../../../../assets"};
    const char* probe_file = "affixes.cfg"; /* cheap probe */

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", candidates[i], probe_file);
        FILE* f = fopen(path, "r");
        if (f)
        {
            fclose(f);
            strncpy(out_base, candidates[i], out_sz - 1);
            out_base[out_sz - 1] = '\0';
            return;
        }
    }

    /* Fallback: default to ../assets (expected when CWD is build/) */
    strncpy(out_base, "../assets", out_sz - 1);
    out_base[out_sz - 1] = '\0';
}

int main(void)
{
    printf("=== CFG File Analysis Test ===\n\n");

    char assets_base[256];
    resolve_assets_base(assets_base, sizeof(assets_base));

    const char* cfg_names[] = {"affixes.cfg",
                               "biome_assets.cfg",
                               "encounters.cfg",
                               "enemies.cfg",
                               "equipment_test_sockets.cfg",
                               "gems_test.cfg",
                               "hud_layout.cfg",
                               "mining_nodes.cfg",
                               "plants.cfg",
                               "player_anim.cfg",
                               "player_sheets.cfg",
                               "projectiles.cfg",
                               "resource_nodes.cfg",
                               "skills_uhf87f.cfg",
                               "sounds.cfg",
                               "test_equipment_items.cfg",
                               "test_items.cfg",
                               "test_loot_tables.cfg",
                               "tiles.cfg",
                               "trees.cfg",
                               "ui_theme_default.cfg"};

    int num_files = sizeof(cfg_names) / sizeof(cfg_names[0]);
    int successful_analyses = 0;

    for (int i = 0; i < num_files; i++)
    {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", assets_base, cfg_names[i]);
        printf("Analyzing: %s\n", fullpath);

        RogueCfgFileAnalysis* analysis = rogue_cfg_analyze_file(fullpath);
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
