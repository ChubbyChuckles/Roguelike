#include "save_replay.h"
#include "save_utils.h"
#include <string.h>

RogueReplayEvent g_replay_events[ROGUE_REPLAY_MAX_EVENTS];
uint32_t g_replay_event_count = 0;
unsigned char g_last_replay_hash[32];

void rogue_save_replay_reset(void)
{
    g_replay_event_count = 0;
    memset(g_last_replay_hash, 0, 32);
}

int rogue_save_replay_record_input(uint32_t frame, uint32_t action_code, int32_t value)
{
    if (g_replay_event_count >= ROGUE_REPLAY_MAX_EVENTS)
        return -1;
    g_replay_events[g_replay_event_count].frame = frame;
    g_replay_events[g_replay_event_count].action = action_code;
    g_replay_events[g_replay_event_count].value = value;
    g_replay_event_count++;
    return 0;
}

void rogue_replay_compute_hash(void)
{
    RogueSHA256Ctx sha;
    rogue_sha256_init(&sha);
    rogue_sha256_update(&sha, g_replay_events, g_replay_event_count * sizeof(RogueReplayEvent));
    rogue_sha256_final(&sha, g_last_replay_hash);
}

const unsigned char* rogue_save_last_replay_hash(void) { return g_last_replay_hash; }

void rogue_save_last_replay_hash_hex(char out[65])
{
    static const char* hx = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        out[2 * i] = hx[g_last_replay_hash[i] >> 4];
        out[2 * i + 1] = hx[g_last_replay_hash[i] & 0xF];
    }
    out[64] = '\0';
}

uint32_t rogue_save_last_replay_event_count(void) { return g_replay_event_count; }
