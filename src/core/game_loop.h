#ifndef ROGUE_CORE_GAME_LOOP_H
#define ROGUE_CORE_GAME_LOOP_H

#include <stdbool.h>

typedef struct RogueGameLoopConfig {
    int target_fps;
} RogueGameLoopConfig;

typedef struct RogueGameLoopState {
    RogueGameLoopConfig cfg;
    bool running;
    double target_frame_seconds;
} RogueGameLoopState;

extern RogueGameLoopState g_game_loop;

void rogue_game_loop_init(const RogueGameLoopConfig *cfg);
void rogue_game_loop_iterate(void);
void rogue_game_loop_request_exit(void);

#endif
