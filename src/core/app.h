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
#ifndef ROGUE_CORE_APP_H
#define ROGUE_CORE_APP_H

#include <stdbool.h>

typedef struct RogueAppConfig
{
    const char* window_title;
    int window_width;
    int window_height;
    int target_fps; /* 0 = uncapped */
} RogueAppConfig;

bool rogue_app_init(const RogueAppConfig* cfg);
/* Run full loop until exit requested */
void rogue_app_run(void);
/* Execute exactly one frame (for tests & tools) */
void rogue_app_step(void);
/* Total frames processed since init */
int rogue_app_frame_count(void);
void rogue_app_shutdown(void);

#endif
