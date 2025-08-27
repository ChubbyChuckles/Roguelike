#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

/* Phase 9.1: Deterministic ordering by (emit_frame, priority, seq)
    Verify that swapping emit order between different priorities yields identical digest.
    Use VFX events to avoid audio registry interaction in this focused test. */
int main(void)
{
    /* Run 1: emit UI then COMBAT (as VFX ids) */
    rogue_fx_frame_begin(0);
    RogueEffectEvent ui = {0};
    ui.type = ROGUE_FX_VFX_SPAWN;
    ui.priority = ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
    strncpy_s(ui.id, sizeof ui.id, "UI", _TRUNCATE);
#else
    strncpy(ui.id, "UI", sizeof ui.id - 1);
    ui.id[sizeof ui.id - 1] = '\0';
#endif
    RogueEffectEvent cb = {0};
    cb.type = ROGUE_FX_VFX_SPAWN;
    cb.priority = ROGUE_FX_PRI_COMBAT;
#if defined(_MSC_VER)
    strncpy_s(cb.id, sizeof cb.id, "CB", _TRUNCATE);
#else
    strncpy(cb.id, "CB", sizeof cb.id - 1);
    cb.id[sizeof cb.id - 1] = '\0';
#endif
    assert(rogue_fx_emit(&ui) == 0);
    assert(rogue_fx_emit(&cb) == 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    unsigned int d1 = rogue_fx_get_frame_digest();

    /* Run 2: emit COMBAT then UI (should produce same digest since priority sorts first) */
    rogue_fx_frame_begin(0);
    assert(rogue_fx_emit(&cb) == 0);
    assert(rogue_fx_emit(&ui) == 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    unsigned int d2 = rogue_fx_get_frame_digest();
    if (d1 != d2)
    {
        /* Debug aid: print both digests to help diagnose order invariance */
        printf("digest run1=%08x run2=%08x\n", d1, d2);
    }
    assert(d1 == d2);
    return 0;
}
