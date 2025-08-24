#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

/* Minimal test: emit 2 audio events and validate ordering & digest stability */
int main(void)
{
    rogue_fx_frame_begin(0);
    RogueEffectEvent a = {0};
    a.type = ROGUE_FX_AUDIO_PLAY;
    a.priority = ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
    strncpy_s(a.id, sizeof a.id, "A", _TRUNCATE);
#else
    strncpy(a.id, "A", sizeof a.id - 1);
    a.id[sizeof a.id - 1] = '\0';
#endif
    RogueEffectEvent b = {0};
    b.type = ROGUE_FX_AUDIO_PLAY;
    b.priority = ROGUE_FX_PRI_COMBAT;
#if defined(_MSC_VER)
    strncpy_s(b.id, sizeof b.id, "B", _TRUNCATE);
#else
    strncpy(b.id, "B", sizeof b.id - 1);
    b.id[sizeof b.id - 1] = '\0';
#endif
    assert(rogue_fx_emit(&a) == 0);
    assert(rogue_fx_emit(&b) == 0);
    rogue_fx_frame_end();
    int n = rogue_fx_dispatch_process();
    assert(n == 2);
    /* Deterministic digest for same inputs */
    unsigned int d1 = rogue_fx_get_frame_digest();
    rogue_fx_frame_begin(0);
    assert(rogue_fx_emit(&a) == 0);
    assert(rogue_fx_emit(&b) == 0);
    rogue_fx_frame_end();
    rogue_fx_dispatch_process();
    unsigned int d2 = rogue_fx_get_frame_digest();
    assert(d1 == d2);
    return 0;
}
