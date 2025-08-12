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
#include "graphics/renderer.h"

typedef enum RogueWindowMode
{
    ROGUE_WINDOW_WINDOWED = 0,
    ROGUE_WINDOW_BORDERLESS,
    ROGUE_WINDOW_FULLSCREEN
} RogueWindowMode;

/* RogueColor defined in renderer.h */

typedef struct RogueAppConfig
{
    const char* window_title;            /* Title bar text */
    int window_width;                    /* Initial physical window width */
    int window_height;                   /* Initial physical window height */
    int logical_width;                   /* Virtual/logical render width (e.g. 320) */
    int logical_height;                  /* Virtual/logical render height (e.g. 180) */
    int target_fps;                      /* 0 = uncapped busy loop */
    int enable_vsync;                    /* 1 = request vsync, 0 = no vsync */
    int allow_resize;                    /* 1 = user can resize window */
    int integer_scale;                   /* 1 = force integer scaling for crisp pixels */
    RogueWindowMode window_mode;         /* initial window mode */
    RogueColor background_color;         /* Clear color */
} RogueAppConfig;

bool rogue_app_init(const RogueAppConfig* cfg);
/* Run full loop until exit requested */
void rogue_app_run(void);
/* Execute exactly one frame (for tests & tools) */
void rogue_app_step(void);
/* Total frames processed since init */
int rogue_app_frame_count(void);
void rogue_app_shutdown(void);

/* Dynamic controls */
void rogue_app_toggle_fullscreen(void);
void rogue_app_set_vsync(int enabled);

/* Frame metrics (updated each frame). Any pointer may be NULL. */
void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms);

/* Time since last frame in seconds (clamped). */
double rogue_app_delta_time(void);

#endif
