/* Test JSON directory loader: create a temp dir with a couple of valid item JSON files,
   load them via rogue_item_defs_load_directory_json, and verify indices. */
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h> /* _mkdir, _rmdir */
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
    const char* dir = "tmp_items_dir";
    const char* f1 = "tmp_items_dir/a.json";
    const char* f2 = "tmp_items_dir/b.json";

    /* Create temp directory */
    make_dir(dir);

    /* Two valid item defs */
    const char* j1 = "[\n"
                     " {\"id\":\"tmp_json_sword\",\"name\":\"Tmp JSON Sword\",\"category\":2,"
                     "\"level_req\":1,\"stack_max\":1,\"base_value\":25,\"base_damage_min\":3,"
                     "\"base_damage_max\":6,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\","
                     "\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,"
                     "\"flags\":0}\n]";
    const char* j2 = "[\n"
                     " {\"id\":\"tmp_json_helm\",\"name\":\"Tmp JSON Helm\",\"category\":3,"
                     "\"level_req\":1,\"stack_max\":1,\"base_value\":15,\"base_damage_min\":0,"
                     "\"base_damage_max\":0,\"base_armor\":3,\"sprite_sheet\":\"sheet.png\","
                     "\"sprite_tx\":1,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,"
                     "\"flags\":0}\n]";

    if (!write_file(f1, j1) || !write_file(f2, j2))
    {
        fprintf(stderr, "failed to write temp json files\n");
        return 1;
    }

    rogue_item_defs_reset();
    int loaded = rogue_item_defs_load_directory_json(dir);
    if (loaded < 2)
    {
        fprintf(stderr, "expected >=2 loaded, got %d\n", loaded);
        /* Cleanup */
        remove(f1);
        remove(f2);
        remove_dir(dir);
        return 2;
    }
    if (rogue_item_def_index_fast("tmp_json_sword") < 0 ||
        rogue_item_def_index_fast("tmp_json_helm") < 0)
    {
        fprintf(stderr, "missing expected ids after directory load\n");
        /* Cleanup */
        remove(f1);
        remove(f2);
        remove_dir(dir);
        return 3;
    }

    /* Cleanup temp files/dir */
    remove(f1);
    remove(f2);
    remove_dir(dir);
    printf("items JSON directory load OK (loaded=%d)\n", loaded);
    return 0;
}
