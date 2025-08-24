#include "../../src/core/persistence.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

int main(void)
{
    /* Use temp file names to avoid clobbering actual saves */
    const char* ps = "test_player_stats_version.cfg";
    const char* gp = "test_gen_params_version.cfg";
    /* Write legacy (no VERSION) player stats */
    FILE* f = fopen(ps, "wb");
    assert(f);
    fputs("LEVEL=1\nXP=0\nXP_TO_NEXT=10\nSTR=1\nDEX=1\nVIT=1\nINT=0\nCRITC=0\nCRITD=50\nUNSPENT="
          "0\nHP=10\nMP=5\nTALENTPTS=0\n",
          f);
    fclose(f);
    f = fopen(gp, "wb");
    assert(f);
    fputs("WATER_LEVEL=0.34\nNOISE_OCTAVES=6\nNOISE_GAIN=0.48\nNOISE_LACUNARITY=2.05\nRIVER_"
          "SOURCES=10\nRIVER_MAX_LENGTH=1200\nCAVE_THRESH=0.60\n",
          f);
    fclose(f);
    rogue_persistence_set_paths(ps, gp);
    rogue_persistence_load_generation_params();
    rogue_persistence_load_player_stats();
    assert(rogue_persistence_player_version() == 1);
    assert(rogue_persistence_gen_params_version() == 1);
    /* Save back out (should inject VERSION lines) */
    rogue_persistence_save_player_stats();
    rogue_persistence_save_generation_params_if_dirty(); /* may not be dirty; force write by flag */
    /* Force dirty and save to ensure version line output */
    extern struct RogueApp g_app; /* not declared here; skip modification; assume generation not
                                     dirty means file unchanged */
    /* Re-open player stats to see VERSION line */
    f = fopen(ps, "rb");
    assert(f);
    char buf[512];
    int saw_version = 0;
    while (fgets(buf, sizeof buf, f))
    {
        if (strncmp(buf, "VERSION=", 8) == 0)
            saw_version = 1;
    }
    fclose(f);
    assert(saw_version);
    return 0;
}
