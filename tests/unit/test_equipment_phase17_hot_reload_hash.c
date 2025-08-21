/* Phase 17.5 (partial forward): Determinism hash test for set registry post hot reload */
#include "core/equipment/equipment_content.h"
#include "util/hot_reload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_file(const char* path, const char* text)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static void assert_true(int c, const char* m)
{
    if (!c)
    {
        printf("FAIL:%s\n", m);
        exit(1);
    }
}

int main()
{
    rogue_sets_reset();
    rogue_hot_reload_reset();
    const char* path = "tmp_equipment_sets_hot_reload_hash.json";
    const char* v1 = "[ { \"set_id\": 500, \"bonuses\": [ { \"pieces\":2, \"strength\":1 }, { "
                     "\"pieces\":3, \"strength\":2 } ] } ]";
    assert_true(write_file(path, v1) == 0, "write v1");
    assert_true(rogue_equipment_sets_register_hot_reload("sets_hash", path) == 0, "register");
    unsigned int empty_hash = rogue_sets_state_hash();
    assert_true(rogue_hot_reload_force("sets_hash") == 0, "force load");
    unsigned int h1 = rogue_sets_state_hash();
    assert_true(h1 != empty_hash, "hash changed after load");
    /* Reload without change should produce identical hash */
    assert_true(rogue_hot_reload_force("sets_hash") == 0, "force reload same");
    unsigned int h2 = rogue_sets_state_hash();
    assert_true(h1 == h2, "hash stable across identical reload");
    /* Modify file -> hash should update */
    const char* v2 = "[ { \"set_id\": 500, \"bonuses\": [ { \"pieces\":2, \"strength\":1 }, { "
                     "\"pieces\":3, \"strength\":3 } ] } ]"; /* second bonus strength 2->3 */
    assert_true(write_file(path, v2) == 0, "write v2");
    assert_true(rogue_hot_reload_tick() == 1, "tick detects change");
    unsigned int h3 = rogue_sets_state_hash();
    assert_true(h3 != h1, "hash changed after modification");
    remove(path);
    printf("Phase17.5 (hash) OK: h1=%u h3=%u\n", h1, h3);
    return 0;
}
