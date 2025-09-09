/* Validate JSON failure does not corrupt registry and loading from CFG still works. */
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h> /* _mkdir, _rmdir */
#endif

static int make_dir(const char* path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static int remove_dir(const char* path)
{
#if defined(_WIN32)
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

static int write_file(const char* path, const char* contents)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);
    return 1;
}

int main(void)
{
    /* Create a dir with only malformed JSON to force JSON loader to add nothing. */
    const char* dir = "tmp_items_dir_bad_only";
    const char* f_bad = "tmp_items_dir_bad_only/bad.json";
    make_dir(dir);

    /* Craft a JSON array with an object missing the required 'id' field so the loader
       will not add any entries (sanitization won't fix missing identifiers). */
    const char* bad =
        "[\n"
        " {\"name\":\"Bad\",\"category\":2,\"stack_max\":1,"
        "\"base_value\":25,\"base_damage_min\":3,\"base_damage_max\":7,\"base_armor\":0,"
        "\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_"
        "th\":1,\"rarity\":1,\"flags\":0}\n"
        "]"; /* missing 'id' => not added */

    if (!write_file(f_bad, bad))
    {
        fprintf(stderr, "failed to write temp malformed json\n");
        return 1;
    }

    rogue_item_defs_reset();
    int before = rogue_item_defs_count();
    int added_json = rogue_item_defs_load_directory_json(dir);
    int after = rogue_item_defs_count();

    /* Expect no additions from malformed-only directory */
    if (added_json > 0 || after != before)
    {
        fprintf(stderr,
                "unexpected additions from malformed json directory: added=%d before=%d after=%d\n",
                added_json, before, after);
        remove(f_bad);
        remove_dir(dir);
        return 2;
    }

    /* Now load from the known-good CFG file in assets and ensure we get items. */
    const char* candidates[] = {"assets/test_items.cfg", "../assets/test_items.cfg",
                                "../../assets/test_items.cfg", NULL};
    int loaded_cfg = 0;
    for (int i = 0; candidates[i]; i++)
    {
        int r = rogue_item_defs_load_from_cfg(candidates[i]);
        if (r > 0)
        {
            loaded_cfg = r;
            break;
        }
    }
    if (loaded_cfg <= 0)
    {
        fprintf(stderr, "failed to load fallback cfg items\n");
        remove(f_bad);
        remove_dir(dir);
        return 3;
    }

    if (rogue_item_def_index_fast("gold_coin") < 0 && rogue_item_def_index_fast("bandage") < 0)
    {
        fprintf(stderr, "expected known cfg ids to be present after fallback load\n");
        remove(f_bad);
        remove_dir(dir);
        return 4;
    }

    remove(f_bad);
    remove_dir(dir);
    printf("items JSON fallback to CFG OK (cfg_added=%d)\n", loaded_cfg);
    return 0;
}
