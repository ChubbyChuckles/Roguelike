#include "../../src/audio_vfx/effects.h"
#include <assert.h>
#include <string.h>

static void emit_three_frames(void)
{
    /* Frame 10: one VFX spawn */
    rogue_fx_frame_begin(10);
    RogueEffectEvent v = {0};
    v.type = ROGUE_FX_VFX_SPAWN;
    v.priority = ROGUE_FX_PRI_COMBAT;
#if defined(_MSC_VER)
    strncpy_s(v.id, sizeof v.id, "spark", _TRUNCATE);
#else
    strncpy(v.id, "spark", sizeof v.id - 1);
    v.id[sizeof v.id - 1] = '\0';
#endif
    v.x = 1.0f;
    v.y = 2.0f;
    assert(rogue_fx_emit(&v) == 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();

    /* Frame 11: two audio plays */
    rogue_fx_frame_begin(11);
    RogueEffectEvent a = {0};
    a.type = ROGUE_FX_AUDIO_PLAY;
    a.priority = ROGUE_FX_PRI_UI;
#if defined(_MSC_VER)
    strncpy_s(a.id, sizeof a.id, "click", _TRUNCATE);
#else
    strncpy(a.id, "click", sizeof a.id - 1);
    a.id[sizeof a.id - 1] = '\0';
#endif
    RogueEffectEvent b = a;
#if defined(_MSC_VER)
    strncpy_s(b.id, sizeof b.id, "hover", _TRUNCATE);
#else
    strncpy(b.id, "hover", sizeof b.id - 1);
    b.id[sizeof b.id - 1] = '\0';
#endif
    assert(rogue_fx_emit(&a) == 0);
    assert(rogue_fx_emit(&b) == 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();

    /* Frame 12: empty (no events) */
    rogue_fx_frame_begin(12);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
}

int main(void)
{
    /* Record a short session */
    rogue_fx_replay_begin_record();
    emit_three_frames();
    RogueEffectEvent buf[64];
    int n = rogue_fx_replay_end_record(buf, 64);
    assert(n > 0);
    unsigned long long h1 = rogue_fx_events_hash(buf, n);

    /* Load and play back frame 11 events into a fresh frame; digest should be stable. */
    rogue_fx_replay_load(buf, n);
    rogue_fx_frame_begin(11);
    int enq = rogue_fx_replay_enqueue_frame(11);
    assert(enq > 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    unsigned int d11 = rogue_fx_get_frame_digest();

    /* Repeat the enqueue for the same frame and expect same digest */
    rogue_fx_frame_begin(11);
    enq = rogue_fx_replay_enqueue_frame(11);
    assert(enq > 0);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    unsigned int d11b = rogue_fx_get_frame_digest();
    assert(d11 == d11b);

    /* Hash should be identical when re-hashing the same buffer */
    unsigned long long h2 = rogue_fx_events_hash(buf, n);
    assert(h1 == h2);

    /* Divergence accumulator: combine per-frame digests and compare across two runs */
    rogue_fx_hash_reset(0);
    rogue_fx_frame_begin(100);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    rogue_fx_hash_accumulate_frame();
    rogue_fx_frame_begin(101);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    rogue_fx_hash_accumulate_frame();
    unsigned long long acc1 = rogue_fx_hash_get();

    rogue_fx_hash_reset(0);
    rogue_fx_frame_begin(100);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    rogue_fx_hash_accumulate_frame();
    rogue_fx_frame_begin(101);
    rogue_fx_frame_end();
    (void) rogue_fx_dispatch_process();
    rogue_fx_hash_accumulate_frame();
    unsigned long long acc2 = rogue_fx_hash_get();
    assert(acc1 == acc2);

    return 0;
}
