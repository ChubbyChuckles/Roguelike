#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

/* Test: frame compaction merges identical events and updates digest */
int main(void)
{
    rogue_fx_frame_begin(42);
    RogueEffectEvent e1 = {0};
    e1.type = ROGUE_FX_AUDIO_PLAY;
    e1.priority = ROGUE_FX_PRI_COMBAT;
#if defined(_MSC_VER)
    strncpy_s(e1.id, sizeof e1.id, "HIT", _TRUNCATE);
#else
    strncpy(e1.id, "HIT", sizeof e1.id - 1);
    e1.id[sizeof e1.id - 1] = '\0';
#endif
    RogueEffectEvent e2 = e1; /* identical */
    RogueEffectEvent e3 = e1; /* identical */
    RogueEffectEvent e4 = {0};
    e4.type = ROGUE_FX_AUDIO_PLAY;
    e4.priority = ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
    strncpy_s(e4.id, sizeof e4.id, "CLICK", _TRUNCATE);
#else
    strncpy(e4.id, "CLICK", sizeof e4.id - 1);
    e4.id[sizeof e4.id - 1] = '\0';
#endif
    assert(rogue_fx_emit(&e1) == 0);
    assert(rogue_fx_emit(&e2) == 0);
    assert(rogue_fx_emit(&e3) == 0);
    assert(rogue_fx_emit(&e4) == 0);
    rogue_fx_frame_end();
    int processed = rogue_fx_dispatch_process();
    /* After compaction we should have 2 processed entries: HIT x3 and CLICK x1 */
    assert(processed == 2);
    unsigned int d1 = rogue_fx_get_frame_digest();

    /* Repeat the same sequence; digest should be identical */
    rogue_fx_frame_begin(42);
    assert(rogue_fx_emit(&e1) == 0);
    assert(rogue_fx_emit(&e2) == 0);
    assert(rogue_fx_emit(&e3) == 0);
    assert(rogue_fx_emit(&e4) == 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    unsigned int d2 = rogue_fx_get_frame_digest();
    assert(d1 == d2);
    return 0;
}
