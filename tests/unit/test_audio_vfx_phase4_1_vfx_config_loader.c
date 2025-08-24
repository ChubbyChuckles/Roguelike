/* Phase 4.1: VFX authoring config loader (CSV) */
#include "../../src/audio_vfx/effects.h"
#include "../../src/audio_vfx/vfx_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_all(void)
{
    rogue_vfx_registry_clear();
    rogue_vfx_clear_active();
    rogue_vfx_set_timescale(1.0f);
    rogue_vfx_set_frozen(0);
}

int main(void)
{
    reset_all();

    /* Prepare a temporary CSV file in build dir */
    const char* fname = "vfx_test_tmp.cfg";
    FILE* f = fopen(fname, "w");
    assert(f);
    /* id,layer,lifetime_ms,world_space,emit_hz,particle_lifetime_ms,max_particles */
    fprintf(f, "# VFX defs\n");
    fprintf(f, "dust_world,MID,500,1,60,150,6\n");
    fprintf(f, "spark_ui,UI,800,0,120,100,8\n");
    fclose(f);

    int loaded = 0;
    assert(rogue_vfx_load_cfg(fname, &loaded) == 0);
    assert(loaded == 2);

    /* Validate registry contents */
    RogueVfxLayer layer;
    uint32_t life;
    int world;
    assert(rogue_vfx_registry_get("dust_world", &layer, &life, &world) == 0);
    assert(layer == ROGUE_VFX_LAYER_MID && life == 500 && world == 1);
    assert(rogue_vfx_registry_get("spark_ui", &layer, &life, &world) == 0);
    assert(layer == ROGUE_VFX_LAYER_UI && life == 800 && world == 0);

    /* Spawn and step a bit; expect particles from both */
    assert(rogue_vfx_spawn_by_id("dust_world", 10.0f, 5.0f) == 0);
    assert(rogue_vfx_spawn_by_id("spark_ui", 200.0f, 100.0f) == 0);
    rogue_vfx_update(100);
    int total = rogue_vfx_particles_active_count();
    assert(total > 0);

    /* Ensure both layers have some presence */
    int mid_ct = rogue_vfx_particles_layer_count(ROGUE_VFX_LAYER_MID);
    int ui_ct = rogue_vfx_particles_layer_count(ROGUE_VFX_LAYER_UI);
    assert(mid_ct >= 1 && ui_ct >= 1);

    return 0;
}
