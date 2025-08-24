/* Phase M4.1 additional tests: persistence edge cases (negative version, dirty save gating) */
#include "../../src/core/app_state.h"
#include "../../src/core/persistence.h"
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
    const char* ps = "test_player_stats_neg_version.cfg";
    const char* gp = "test_gen_params_neg_version.cfg";
    /* Player stats with negative version and some values */
    write_file(ps, "VERSION=-3\nLEVEL=5\nXP=10\nXP_TO_NEXT=20\nSTR=2\nDEX=3\nVIT=4\nINT=1\nCRITC="
                   "5\nCRITD=60\nUNSPENT=1\nHP=25\nMP=8\nTALENTPTS=0\n");
    /* Gen params with negative version; include comments & blank lines */
    write_file(gp, "# comment "
                   "line\nVERSION=-2\n\nWATER_LEVEL=0.40\nNOISE_OCTAVES=7\nNOISE_GAIN=0.50\nNOISE_"
                   "LACUNARITY=2.10\nRIVER_SOURCES=11\nRIVER_MAX_LENGTH=1300\nCAVE_THRESH=0.65\n");
    rogue_persistence_set_paths(ps, gp);
    rogue_persistence_load_player_stats();
    rogue_persistence_load_generation_params();
    /* Negative versions clamped to 1 */
    assert(rogue_persistence_player_version() == 1);
    assert(rogue_persistence_gen_params_version() == 1);
    /* Loaded player value */
    assert(g_app.player.level == 5);
    /* Modify and save player */
    g_app.player.level = 7;
    g_app.stats_dirty = 1; /* mark dirty just in case future gate added */
    rogue_persistence_save_player_stats();
    /* Read back ensure VERSION line present and updated level */
    FILE* f = fopen(ps, "rb");
    assert(f);
    char line[256];
    int saw_version = 0, saw_level = 0;
    while (fgets(line, sizeof line, f))
    {
        if (strncmp(line, "VERSION=", 8) == 0)
            saw_version = 1;
        if (strncmp(line, "LEVEL=7", 7) == 0)
            saw_level = 1;
    }
    fclose(f);
    assert(saw_version && saw_level);
    /* Gen params save only when dirty */
    remove(gp); /* ensure file absence to detect write */
    g_app.gen_water_level = 0.55f;
    g_app.gen_params_dirty = 1; /* mark dirty to force write */
    rogue_persistence_save_generation_params_if_dirty();
    f = fopen(gp, "rb");
    assert(f);
    int saw_wl = 0;
    saw_version = 0;
    while (fgets(line, sizeof line, f))
    {
        if (strncmp(line, "VERSION=", 8) == 0)
            saw_version = 1;
        if (strncmp(line, "WATER_LEVEL=0.55", 16) == 0)
            saw_wl = 1;
    }
    fclose(f);
    assert(saw_version && saw_wl);
    return 0;
}
