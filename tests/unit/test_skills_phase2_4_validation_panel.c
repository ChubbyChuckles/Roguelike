/* Unit: validation helpers for Skills Visuals Validation Panel */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "../../src/core/skills/skill_asset_validation.h"

static void write_file(const char* path, const char* bytes)
{
    FILE* f = fopen(path, "wb");
    assert(f);
    fwrite(bytes, 1, strlen(bytes), f);
    fclose(f);
}

static void test_missing_corrupt_ext()
{
#ifdef _WIN32
    _mkdir("test_assets");
#else
    mkdir("test_assets", 0755);
#endif
    write_file("test_assets/corrupt_sprite.png", "NOTPNG");
    write_file("test_assets/readme.txt", "hello");
    int missing, load_failed, dim_err, w, h, ext_warn;
    rogue_skill_asset_validate("test_assets/does_not_exist_98765.png", &missing, &load_failed,
                               &dim_err, &w, &h, &ext_warn);
    assert(missing == 1 && load_failed == 0);
    rogue_skill_asset_validate("test_assets/corrupt_sprite.png", &missing, &load_failed, &dim_err,
                               &w, &h, &ext_warn);
    assert(missing == 0);
    assert(load_failed == 1 || dim_err == 1);
    rogue_skill_asset_validate("test_assets/readme.txt", &missing, &load_failed, &dim_err, &w, &h,
                               &ext_warn);
    assert(missing == 0 && ext_warn == 1);
}

static void test_grid_infer()
{
    int gw, gh, fc;
    int ok = rogue_visuals_infer_grid(256, 256, &gw, &gh, &fc);
    assert(ok == 1 && gw == 4 && gh == 4 && fc == 16);
    ok = rogue_visuals_infer_grid(96, 64, &gw, &gh, &fc);
    assert(ok == 1 && gw == 3 && gh == 2 && fc == 6);
    ok = rogue_visuals_infer_grid(100, 50, &gw, &gh, &fc);
    assert(ok == 0);
}

int main(void)
{
    test_missing_corrupt_ext();
    test_grid_infer();
    printf("ok\n");
    return 0;
}
