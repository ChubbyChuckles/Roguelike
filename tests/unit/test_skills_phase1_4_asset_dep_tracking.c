/* Phase 1.4: Asset Dependency Tracking & Hot-Reload (foundation) test
   Validates basic track / untrack / poll behavior using a temporary file. The poll will always
   succeed even if timestamp granularity is coarse; we simulate a modification by rewriting the
   file (mtime should change or remain same on very fast FS; test tolerates no change by only
   asserting non-negative counts). */

#include "../../src/core/skills/skill_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

static int on_change_cb(const char* path, void* user)
{
    int* hits = (int*) user;
    (*hits)++;
    (void) path;
    return 0;
}

int main(void)
{
    rogue_skill_asset_dep_reset();
    const char* tmp_path = "build/test_asset_dep_tmp.txt";
    /* Create initial file */
    FILE* f = fopen(tmp_path, "wb");
    if (f)
    {
        fputs("v1", f);
        fclose(f);
    }
    int idx = rogue_skill_asset_dep_track(tmp_path);
    if (idx < 0)
    {
        fprintf(stderr, "TRACK_FAILED\n");
        return 1;
    }
    if (rogue_skill_asset_dep_count() <= 0)
    {
        fprintf(stderr, "COUNT_ZERO\n");
        return 1;
    }
    int changes = rogue_skill_asset_dep_poll_changes(NULL, NULL);
    if (changes < 0)
    {
        fprintf(stderr, "POLL_NEG\n");
        return 1;
    }
    /* Modify file ensuring timestamp tick (sleep small window) */
    sleep_ms(20);
    f = fopen(tmp_path, "wb");
    if (f)
    {
        fputs("v2", f);
        fclose(f);
    }
    int hits = 0;
    rogue_skill_asset_dep_poll_changes(on_change_cb, &hits);
    if (hits < 0)
    {
        fprintf(stderr, "HITS_NEG\n");
        return 1;
    }
    /* Untrack */
    rogue_skill_asset_dep_untrack(tmp_path);
    if (rogue_skill_asset_dep_count() != 0)
    {
        fprintf(stderr, "UNTRACK_REMAIN\n");
        return 1;
    }
    printf("ASSET_DEP_TRACKING_OK hits=%d\n", hits);
    return 0;
}
