#include "../../src/asset/asset_manager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Smoke test: ensure poll function callable headless and returns >=0. */
static int assert_true(int e, const char* m)
{
    if (!e)
    {
        printf("FAIL: %s\n", m);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!rogue_asset_manager_init(NULL))
    {
        printf("FAIL: init\n");
        return 1;
    }
    int idx = rogue_asset_manager_acquire_texture("assets/placeholder.png");
    if (!assert_true(idx >= 0, "acquire placeholder"))
        return 1;
    int reloads = rogue_asset_manager_poll_reload();
    if (!assert_true(reloads >= 0, "poll reload non-negative"))
        return 1;
    printf("OK test_asset_manager_reload\n");
    return 0;
}
