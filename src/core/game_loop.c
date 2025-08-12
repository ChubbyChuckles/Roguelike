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
#include "core/game_loop.h"
#include <time.h>

RogueGameLoopState g_game_loop;

static double now_seconds(void) { return (double) clock() / (double) CLOCKS_PER_SEC; }

void rogue_game_loop_init(const RogueGameLoopConfig* cfg)
{
    g_game_loop.cfg = *cfg;
    g_game_loop.running = true;
    g_game_loop.target_frame_seconds = (cfg->target_fps > 0) ? 1.0 / (double) cfg->target_fps : 0.0;
}

void rogue_game_loop_iterate(void)
{
    static double last = 0.0;
    if (last == 0.0)
        last = now_seconds();
    double start = now_seconds();
    double dt = start - last;
    last = start;
    (void) dt;
    if (g_game_loop.target_frame_seconds > 0.0)
    {
        for (;;)
        {
            double elapsed = now_seconds() - start;
            if (elapsed >= g_game_loop.target_frame_seconds)
                break;
        }
    }
}

void rogue_game_loop_request_exit(void) { g_game_loop.running = false; }
