#include "ai/core/blackboard.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    // Basic set & get new types
    assert(rogue_bb_set_vec2(&bb, "last_player_pos", 10.f, 20.f));
    RogueBBVec2 v;
    assert(rogue_bb_get_vec2(&bb, "last_player_pos", &v));
    assert(v.x == 10.f && v.y == 20.f);
    assert(rogue_bb_set_timer(&bb, "alert_timer", 1.0f));
    float t = 0.f;
    assert(rogue_bb_get_timer(&bb, "alert_timer", &t) && t == 1.0f);

    // Write policies
    assert(rogue_bb_set_int(&bb, "score", 5));
    assert(rogue_bb_write_int(&bb, "score", 3, ROGUE_BB_WRITE_MAX)); // 5 -> stays 5
    int iv = 0;
    assert(rogue_bb_get_int(&bb, "score", &iv) && iv == 5);
    assert(rogue_bb_write_int(&bb, "score", 8, ROGUE_BB_WRITE_MAX));
    assert(rogue_bb_get_int(&bb, "score", &iv) && iv == 8);
    assert(rogue_bb_write_int(&bb, "score", 2, ROGUE_BB_WRITE_MIN));
    assert(rogue_bb_get_int(&bb, "score", &iv) && iv == 2);
    assert(rogue_bb_write_int(&bb, "score", 5, ROGUE_BB_WRITE_ACCUM)); // 2+5=7
    assert(rogue_bb_get_int(&bb, "score", &iv) && iv == 7);

    // Float policies
    assert(rogue_bb_set_float(&bb, "threat", 1.0f));
    assert(rogue_bb_write_float(&bb, "threat", 0.5f, ROGUE_BB_WRITE_MAX) == false); // no change
    float fv = 0.f;
    assert(rogue_bb_get_float(&bb, "threat", &fv) && fv == 1.0f);
    assert(rogue_bb_write_float(&bb, "threat", 2.0f, ROGUE_BB_WRITE_MAX));
    assert(rogue_bb_get_float(&bb, "threat", &fv) && fv == 2.0f);
    assert(rogue_bb_write_float(&bb, "threat", 1.0f, ROGUE_BB_WRITE_MIN)); // 1 < 2
    assert(rogue_bb_get_float(&bb, "threat", &fv) && fv == 1.0f);
    assert(rogue_bb_write_float(&bb, "threat", 0.5f, ROGUE_BB_WRITE_ACCUM)); // 1 + .5
    assert(rogue_bb_get_float(&bb, "threat", &fv) && fv == 1.5f);

    // TTL expiration & timer countdown
    assert(rogue_bb_set_ttl(&bb, "threat", 0.05f));
    for (int i = 0; i < 10; i++)
    {
        rogue_bb_tick(&bb, 0.01f);
    }
    // After 0.1s threat should be cleared
    assert(!rogue_bb_get_float(&bb, "threat", &fv));

    // Timer countdown to zero
    for (int i = 0; i < 10; i++)
    {
        rogue_bb_tick(&bb, 0.1f);
    }
    assert(rogue_bb_get_timer(&bb, "alert_timer", &t));
    assert(t == 0.f);

    // Dirty flag behavior
    assert(rogue_bb_is_dirty(&bb, "last_player_pos"));
    rogue_bb_clear_dirty(&bb, "last_player_pos");
    assert(!rogue_bb_is_dirty(&bb, "last_player_pos"));
    assert(rogue_bb_write_float(&bb, "speed", 1.0f, ROGUE_BB_WRITE_SET));
    assert(rogue_bb_is_dirty(&bb, "speed"));

    printf("[test_ai_phase2_blackboard] Passed.\n");
    return 0;
}
