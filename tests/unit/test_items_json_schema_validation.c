/* Test that malformed JSON files are rolled back by schema validation when using
   rogue_item_defs_load_directory_json. One good file + one malformed file should yield only
   the valid defs, and the malformed file should not pollute the registry. */
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
    const char* dir = "tmp_items_dir_invalid";
    const char* f_ok = "tmp_items_dir_invalid/ok.json";
    const char* f_bad = "tmp_items_dir_invalid/bad.json";
    make_dir(dir);

    /* Valid file: a single minimal item */
    const char* ok =
        "[\n"
        " {\"id\":\"ok_item\",\"name\":\"Ok Item\",\"category\":0,\"stack_max\":1,"
        "\"base_value\":1,\"base_damage_min\":0,\"base_damage_max\":0,\"base_armor\":0,"
        "\"sprite_sheet\":\"s.png\",\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":"
        "1,\"rarity\":0,\"flags\":0}\n]";

    /* Malformed w.r.t schema: sprite_tw = 0 (must be >=1), plus missing name */
    const char* bad =
        "[\n"
        " {\"id\":\"bad_item\",\"category\":1,\"stack_max\":1,\"base_value\":1,"
        "\"base_damage_min\":0,\"base_damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"s.png\","
        "\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":0,\"sprite_th\":1,\"rarity\":0,\"flags\":0}"
        "\n]";

    if (!write_file(f_ok, ok) || !write_file(f_bad, bad))
    {
        fprintf(stderr, "failed to write temp json files\n");
        return 1;
    }

    rogue_item_defs_reset();
    int before = rogue_item_defs_count();
    int loaded = rogue_item_defs_load_directory_json(dir);
    int after = rogue_item_defs_count();
    if (loaded <= 0)
    {
        fprintf(stderr, "expected some items to load, got %d\n", loaded);
        remove(f_ok);
        remove(f_bad);
        remove_dir(dir);
        return 2;
    }
    /* Only the ok item should be present; bad.json should have been rolled back */
    if (rogue_item_def_index_fast("ok_item") < 0)
    {
        fprintf(stderr, "missing ok_item after load\n");
        remove(f_ok);
        remove(f_bad);
        remove_dir(dir);
        return 3;
    }
    if (rogue_item_def_index_fast("bad_item") >= 0)
    {
        fprintf(stderr, "bad_item should not be present due to schema violation\n");
        remove(f_ok);
        remove(f_bad);
        remove_dir(dir);
        return 4;
    }
    if (after < before + 1)
    {
        fprintf(stderr, "expected exactly one valid item added, counts before=%d after=%d\n",
                before, after);
        remove(f_ok);
        remove(f_bad);
        remove_dir(dir);
        return 5;
    }

    remove(f_ok);
    remove(f_bad);
    remove_dir(dir);
    printf("items JSON schema rollback OK (loaded=%d)\n", loaded);
    return 0;
}
