/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ROGUE_CORE_GAME_LOOP_H
#define ROGUE_CORE_GAME_LOOP_H

#include <stdbool.h>

typedef struct RogueGameLoopConfig
{
    int target_fps;
} RogueGameLoopConfig;

typedef struct RogueGameLoopState
{
    RogueGameLoopConfig cfg;
    bool running;
    double target_frame_seconds;
} RogueGameLoopState;

extern RogueGameLoopState g_game_loop;

void rogue_game_loop_init(const RogueGameLoopConfig* cfg);
void rogue_game_loop_iterate(void);
void rogue_game_loop_request_exit(void);

#endif
