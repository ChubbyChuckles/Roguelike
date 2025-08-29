#ifndef ROGUE_SAVE_REPLAY_H
#define ROGUE_SAVE_REPLAY_H

#include <stdint.h>

typedef struct RogueReplayEvent
{
    uint32_t frame;
    uint32_t action;
    int32_t value;
} RogueReplayEvent;

#define ROGUE_REPLAY_MAX_EVENTS 4096

void rogue_save_replay_reset(void);
int rogue_save_replay_record_input(uint32_t frame, uint32_t action_code, int32_t value);
const unsigned char* rogue_save_last_replay_hash(void);
void rogue_save_last_replay_hash_hex(char out[65]);
uint32_t rogue_save_last_replay_event_count(void);

/* internal */
void rogue_replay_compute_hash(void);
extern RogueReplayEvent g_replay_events[ROGUE_REPLAY_MAX_EVENTS];
extern uint32_t g_replay_event_count;
extern unsigned char g_last_replay_hash[32];

#endif /* ROGUE_SAVE_REPLAY_H */
