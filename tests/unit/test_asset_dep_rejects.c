#include "../../src/util/asset_dep.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_file(const char* path, const char* contents)
{
    FILE* f = fopen(path, "wb");
    assert(f);
    fputs(contents, f);
    fclose(f);
}

int main(void)
{
    rogue_asset_dep_reset();
    rogue_asset_dep_clear_last_reject();

    /* Set up three files */
    const char* a = "dep_ra.tmp";
    const char* b = "dep_rb.tmp";
    const char* c = "dep_rc.tmp";
    write_file(a, "A1\n");
    write_file(b, "B1\n");
    write_file(c, "C1\n");

    /* Register base A */
    assert(rogue_asset_dep_register("grp/A", a, NULL, 0) == 0);

    /* Register B -> A */
    const char* deps_b[1] = {"grp/A"};
    assert(rogue_asset_dep_register("grp/B", b, deps_b, 1) == 0);

    /* Register C -> A,B */
    const char* deps_c[2] = {"grp/A", "grp/B"};
    assert(rogue_asset_dep_register("grp/C", c, deps_c, 2) == 0);

    /* Verify count and deps */
    assert(rogue_asset_dep_count() >= 3);
    const char* dep_ids[8];
    int dc = rogue_asset_dep_get_deps("grp/C", dep_ids, 8);
    assert(dc == 2);
    int sawA = 0, sawB = 0;
    for (int i = 0; i < dc; ++i)
    {
        if (dep_ids[i] && strcmp(dep_ids[i], "grp/A") == 0)
            sawA = 1;
        if (dep_ids[i] && strcmp(dep_ids[i], "grp/B") == 0)
            sawB = 1;
    }
    assert(sawA && sawB);

    /* Cycle rejection: create a self-dependency (A loop) which must be rejected as a cycle */
    const char* loop_path = "dep_loop.tmp";
    write_file(loop_path, "L1\n");
    const char* deps_a_cycle[1] = {"grp/LOOP"};
    int cyc_rc = rogue_asset_dep_register("grp/LOOP", loop_path, deps_a_cycle, 1);
    assert(cyc_rc < 0);
    char kind[32] = {0}, nid[64] = {0}, dep[64] = {0};
    int has = rogue_asset_dep_get_last_reject(kind, (int) sizeof kind, nid, (int) sizeof nid, dep,
                                              (int) sizeof dep);
    assert(has == 1);
    assert(strcmp(kind, "cycle") == 0);
    assert(strcmp(nid, "grp/LOOP") == 0);

    /* Clear and trigger a path_conflict: make D -> C with same path as A in C's ancestry */
    rogue_asset_dep_clear_last_reject();
    const char* deps_d[1] = {"grp/C"};
    int pc_rc = rogue_asset_dep_register("grp/D", a, deps_d, 1);
    assert(pc_rc < 0);
    memset(kind, 0, sizeof kind);
    memset(nid, 0, sizeof nid);
    memset(dep, 0, sizeof dep);
    has = rogue_asset_dep_get_last_reject(kind, (int) sizeof kind, nid, (int) sizeof nid, dep,
                                          (int) sizeof dep);
    assert(has == 1);
    assert(strcmp(kind, "path_conflict") == 0);
    assert(strcmp(nid, "grp/D") == 0);

    /* Invalidate B and ensure C's hash changes */
    unsigned long long h1 = 0, h2 = 0;
    assert(rogue_asset_dep_hash("grp/C", &h1) == 0);
    write_file(b, "B2\n");
    rogue_asset_dep_invalidate("grp/B");
    assert(rogue_asset_dep_hash("grp/C", &h2) == 0);
    assert(h1 != h2);

    return 0;
}
