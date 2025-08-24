#include "../../src/util/asset_dep.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static void write_file(const char* path, const char* contents)
{
    FILE* f = fopen(path, "wb");
    assert(f);
    fputs(contents, f);
    fclose(f);
}

int main(void)
{
    const char* a = "dep_a.tmp";
    const char* b = "dep_b.tmp";
    const char* c = "dep_c.tmp";
    write_file(a, "A1\n");
    write_file(b, "B1\n");
    write_file(c, "C1\n");
    int rc;
    rc = rogue_asset_dep_register("A", a, NULL, 0);
    assert(rc == 0);
    const char* deps_b[1] = {"A"};
    rc = rogue_asset_dep_register("B", b, deps_b, 1);
    assert(rc == 0);
    const char* deps_c[2] = {"A", "B"};
    rc = rogue_asset_dep_register("C", c, deps_c, 2);
    assert(rc == 0);
    unsigned long long h1 = 0;
    assert(rogue_asset_dep_hash("C", &h1) == 0);
    /* Touch leaf file and ensure hash changes at root */
    write_file(b, "B2\n");
    rogue_asset_dep_invalidate("B");
    unsigned long long h2 = 0;
    assert(rogue_asset_dep_hash("C", &h2) == 0);
    assert(h1 != h2);
    /* Cycle detection: attempt to create cycle should fail (-1) */
    const char* cycle_dep[1] = {"C"};
    int should_fail = rogue_asset_dep_register("A_again", a, cycle_dep, 1);
    assert(should_fail < 0);
    return 0;
}
