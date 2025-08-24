/* Phase 4.2: Hot reload & validation error reporting */
#include "../../src/audio_vfx/effects.h"
#include "../../src/audio_vfx/vfx_config.h"
#include "../../src/util/hot_reload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_all(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_vfx_set_timescale(1.0f);
    rogue_vfx_set_frozen(0);
    rogue_hot_reload_reset();
}

int main(void)
{
    reset_all();

    const char* fname = "vfx_hot_reload_test.cfg";
    /* Write an initial valid CSV */
    {
        FILE* f = fopen(fname, "w");
        assert(f);
        fprintf(f, "spark,MID,400,0,30,120,4\n");
        fclose(f);
    }

    int loaded = 0;
    assert(rogue_vfx_load_cfg(fname, &loaded) == 0);
    assert(loaded == 1);

    /* Watch file for hot reload */
    assert(rogue_vfx_config_watch(fname) == 0);

    /* Modify the file to trigger reload: change layer to UI and parameters */
    {
        FILE* f = fopen(fname, "w");
        assert(f);
        fprintf(f, "spark,UI,500,0,60,100,6\n");
        fclose(f);
    }

    /* Poll hot reload tick to detect change */
    int fired = rogue_hot_reload_tick();
    assert(fired >= 1);

    /* Validate registry reflects updated definition */
    RogueVfxLayer layer;
    unsigned life;
    int world;
    assert(rogue_vfx_registry_get("spark", &layer, &life, &world) == 0);
    assert(layer == ROGUE_VFX_LAYER_UI && life == 500 && world == 0);

    /* Now write a bad row (too few fields) and ensure validation errors recorded */
    {
        FILE* f = fopen(fname, "w");
        assert(f);
        fprintf(f, "bad_row,UI,500\n"); /* only 3 fields */
        fclose(f);
    }
    rogue_hot_reload_tick();
    int err_ct = rogue_vfx_last_cfg_error_count();
    assert(err_ct >= 1);
    char buf[256];
    assert(rogue_vfx_last_cfg_error_get(0, buf, sizeof buf) == 0);
    assert(strlen(buf) > 0);

    return 0;
}
